#include "hooks.h"
#include "logging.h"
#include "ui.h"
#include "imgui/imgui_impl_dx9.h"

LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_imguiInitialized && g_menuOpen)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
    }

    if (g_originalWndProc)
        return CallWindowProc(g_originalWndProc, hWnd, msg, wParam, lParam);

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

HRESULT APIENTRY HookedEndScene(IDirect3DDevice9* device)
{
    if (!g_loggedEndScene)
    {
        DebugLog("CoopCompanion: EndScene hook hit.\n");
        g_loggedEndScene = true;
    }

    RenderOverlay(device, false);
    g_renderedThisFrame = true;

    return g_originalEndScene ? g_originalEndScene(device) : D3D_OK;
}

HRESULT APIENTRY HookedPresent(IDirect3DDevice9* device, const RECT* src, const RECT* dest, HWND window, const RGNDATA* dirtyRegion)
{
    if (!g_loggedPresent)
    {
        DebugLog("CoopCompanion: Present hook hit.\n");
        g_loggedPresent = true;
    }

    if (!g_renderedThisFrame)
    {
        RenderOverlay(device, true);
        g_renderedThisFrame = true;
    }

    g_renderedThisFrame = false;
    return g_originalPresent ? g_originalPresent(device, src, dest, window, dirtyRegion) : D3D_OK;
}

HRESULT APIENTRY HookedSwapChainPresent(IDirect3DSwapChain9* swapChain, const RECT* src, const RECT* dest, HWND window, const RGNDATA* dirtyRegion, DWORD flags)
{
    if (!g_loggedSwapPresent)
    {
        DebugLog("CoopCompanion: SwapChain Present hook hit.\n");
        g_loggedSwapPresent = true;
    }

    if (!g_renderedThisFrame && swapChain)
    {
        IDirect3DDevice9* device = nullptr;
        if (SUCCEEDED(swapChain->GetDevice(&device)) && device)
        {
            RenderOverlay(device, true);
            g_renderedThisFrame = true;
            device->Release();
        }
    }

    g_renderedThisFrame = false;
    return g_originalSwapChainPresent ? g_originalSwapChainPresent(swapChain, src, dest, window, dirtyRegion, flags) : D3D_OK;
}

HRESULT APIENTRY HookedReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params)
{
    if (g_imguiInitialized)
        ImGui_ImplDX9_InvalidateDeviceObjects();

    HRESULT hr = g_originalReset(device, params);

    if (SUCCEEDED(hr) && g_imguiInitialized)
        ImGui_ImplDX9_CreateDeviceObjects();

    return hr;
}

bool PatchVTable(void** vtable, int index, void* hook, void** original)
{
    if (!vtable || !hook || !original)
        return false;

    if (vtable[index] == hook)
        return true;

    DWORD oldProtect = 0;
    if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    if (!*original)
        *original = vtable[index];
    vtable[index] = hook;

    VirtualProtect(&vtable[index], sizeof(void*), oldProtect, &oldProtect);
    return true;
}

bool HookDeviceVTable(IDirect3DDevice9* device, bool replaceOriginals = false)
{
    if (!device)
        return false;

    void** vtable = *reinterpret_cast<void***>(device);
    for (size_t i = 0; i < _countof(kDeviceSignatureIndices); ++i)
    {
        if (!g_deviceSignature[i])
            g_deviceSignature[i] = vtable[kDeviceSignatureIndices[i]];
    }

    if (replaceOriginals)
    {
        // A late-attached game's device can use a distinct vtable from the
        // temporary device used during startup (notably when DxWnd is active).
        // Keep the originals paired with the device we actually render through.
        if (vtable[kEndSceneIndex] != reinterpret_cast<void*>(&HookedEndScene))
            g_originalEndScene = reinterpret_cast<EndSceneFn>(vtable[kEndSceneIndex]);
        if (vtable[kResetIndex] != reinterpret_cast<void*>(&HookedReset))
            g_originalReset = reinterpret_cast<ResetFn>(vtable[kResetIndex]);
        if (vtable[kPresentIndex] != reinterpret_cast<void*>(&HookedPresent))
            g_originalPresent = reinterpret_cast<PresentFn>(vtable[kPresentIndex]);
    }

    bool ok = PatchVTable(vtable, kEndSceneIndex, reinterpret_cast<void*>(&HookedEndScene), reinterpret_cast<void**>(&g_originalEndScene));
    ok = ok && PatchVTable(vtable, kResetIndex, reinterpret_cast<void*>(&HookedReset), reinterpret_cast<void**>(&g_originalReset));
    ok = ok && PatchVTable(vtable, kPresentIndex, reinterpret_cast<void*>(&HookedPresent), reinterpret_cast<void**>(&g_originalPresent));

    DebugLog(ok ? "CoopCompanion: HookDeviceVTable success.\n" : "CoopCompanion: HookDeviceVTable failed.\n");
    return ok;
}

bool IsWritableMemory(const void* address, size_t size)
{
    MEMORY_BASIC_INFORMATION info = {};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD))
        return false;

    const DWORD protect = info.Protect & 0xFF;
    if (protect != PAGE_READWRITE && protect != PAGE_WRITECOPY &&
        protect != PAGE_EXECUTE_READWRITE && protect != PAGE_EXECUTE_WRITECOPY)
        return false;

    const auto begin = reinterpret_cast<uintptr_t>(address);
    const auto end = begin + size;
    const auto regionEnd = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return end >= begin && end <= regionEnd;
}

bool IsExecutableAddress(const void* address)
{
    MEMORY_BASIC_INFORMATION info = {};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD))
        return false;

    const DWORD protect = info.Protect & 0xFF;
    return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ ||
           protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
}

bool LooksLikeD3D9Device(IDirect3DDevice9* device)
{
    if (!device || !IsWritableMemory(device, sizeof(void*)))
        return false;

    void** vtable = nullptr;
    __try
    {
        vtable = *reinterpret_cast<void***>(device);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    if (!vtable)
        return false;

    constexpr int requiredIndices[] = { 0, 1, 2, kResetIndex, kPresentIndex, kEndSceneIndex };
    for (const int index : requiredIndices)
    {
        __try
        {
            if (!IsExecutableAddress(vtable[index]))
                return false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    // The old scan only checked for executable addresses, which also matches
    // unrelated COM objects.  Require a fingerprint of device-only methods
    // captured from our known-good dummy device before invoking a candidate.
    for (size_t i = 0; i < _countof(kDeviceSignatureIndices); ++i)
    {
        if (!g_deviceSignature[i] || vtable[kDeviceSignatureIndices[i]] != g_deviceSignature[i])
            return false;
    }

    D3DDEVICE_CREATION_PARAMETERS params = {};
    __try
    {
        if (FAILED(device->GetCreationParameters(&params)) || !params.hFocusWindow || !IsWindow(params.hFocusWindow))
            return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    DWORD windowProcessId = 0;
    GetWindowThreadProcessId(params.hFocusWindow, &windowProcessId);
    return windowProcessId == GetCurrentProcessId();
}

bool HookExistingD3D9Device()
{
    DebugLog("CoopCompanion: searching for an existing D3D9 device.\n");

    SYSTEM_INFO systemInfo = {};
    GetSystemInfo(&systemInfo);

    BYTE* cursor = static_cast<BYTE*>(systemInfo.lpMinimumApplicationAddress);
    const BYTE* const maximumAddress = static_cast<BYTE*>(systemInfo.lpMaximumApplicationAddress);
    while (cursor < maximumAddress)
    {
        MEMORY_BASIC_INFORMATION info = {};
        if (VirtualQuery(cursor, &info, sizeof(info)) != sizeof(info))
            break;

        BYTE* const next = static_cast<BYTE*>(info.BaseAddress) + info.RegionSize;
        if (info.State == MEM_COMMIT && IsWritableMemory(info.BaseAddress, 1))
        {
            BYTE* const end = static_cast<BYTE*>(info.BaseAddress) + info.RegionSize;
            for (BYTE* address = static_cast<BYTE*>(info.BaseAddress); address + sizeof(void*) <= end; address += sizeof(void*))
            {
                IDirect3DDevice9* candidate = nullptr;
                __try
                {
                    candidate = *reinterpret_cast<IDirect3DDevice9**>(address);
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    continue;
                }

                if (!LooksLikeD3D9Device(candidate))
                    continue;

                if (HookDeviceVTable(candidate, true))
                {
                    DebugLogF("CoopCompanion: hooked existing D3D9 device at %p.\n", candidate);
                    return true;
                }
            }
        }

        if (next <= cursor)
            break;
        cursor = next;
    }

    DebugLog("CoopCompanion: existing D3D9 device not found.\n");
    return false;
}

HRESULT APIENTRY HookedCreateDevice(
    IDirect3D9* d3d,
    UINT Adapter,
    D3DDEVTYPE DeviceType,
    HWND hFocusWindow,
    DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS* pPresentationParameters,
    IDirect3DDevice9** ppReturnedDeviceInterface)
{
    DebugLog("CoopCompanion: CreateDevice hook hit.\n");
    if (!g_originalCreateDevice)
        return D3DERR_INVALIDCALL;

    HRESULT hr = g_originalCreateDevice(d3d, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
    if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface)
        HookDeviceVTable(*ppReturnedDeviceInterface);

    return hr;
}

HRESULT APIENTRY HookedCreateDeviceEx(
    IDirect3D9Ex* d3d,
    UINT Adapter,
    D3DDEVTYPE DeviceType,
    HWND hFocusWindow,
    DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS* pPresentationParameters,
    D3DDISPLAYMODEEX* pFullscreenDisplayMode,
    IDirect3DDevice9Ex** ppReturnedDeviceInterface)
{
    DebugLog("CoopCompanion: CreateDeviceEx hook hit.\n");
    if (!g_originalCreateDeviceEx)
        return D3DERR_INVALIDCALL;

    HRESULT hr = g_originalCreateDeviceEx(d3d, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);
    if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface)
        HookDeviceVTable(*ppReturnedDeviceInterface);

    return hr;
}

bool HookD3D9Interface(IDirect3D9* d3d)
{
    if (!d3d)
        return false;

    void** vtable = *reinterpret_cast<void***>(d3d);
    bool ok = PatchVTable(vtable, kCreateDeviceIndex, reinterpret_cast<void*>(&HookedCreateDevice), reinterpret_cast<void**>(&g_originalCreateDevice));
    DebugLog(ok ? "CoopCompanion: HookD3D9Interface success.\n" : "CoopCompanion: HookD3D9Interface failed.\n");
    return ok;
}

bool HookD3D9ExInterface(IDirect3D9Ex* d3d)
{
    if (!d3d)
        return false;

    void** vtable = *reinterpret_cast<void***>(d3d);
    bool ok = PatchVTable(vtable, kCreateDeviceExIndex, reinterpret_cast<void*>(&HookedCreateDeviceEx), reinterpret_cast<void**>(&g_originalCreateDeviceEx));
    DebugLog(ok ? "CoopCompanion: HookD3D9ExInterface success.\n" : "CoopCompanion: HookD3D9ExInterface failed.\n");
    return ok;
}

IDirect3D9* WINAPI HookedDirect3DCreate9(UINT sdkVersion)
{
    DebugLog("CoopCompanion: Direct3DCreate9 hook hit.\n");
    if (!g_originalCreate9)
        return nullptr;

    IDirect3D9* d3d = g_originalCreate9(sdkVersion);
    HookD3D9Interface(d3d);
    return d3d;
}

HRESULT WINAPI HookedDirect3DCreate9Ex(UINT sdkVersion, IDirect3D9Ex** out)
{
    DebugLog("CoopCompanion: Direct3DCreate9Ex hook hit.\n");
    if (!g_originalCreate9Ex)
        return D3DERR_INVALIDCALL;

    HRESULT hr = g_originalCreate9Ex(sdkVersion, out);
    if (SUCCEEDED(hr) && out && *out)
        HookD3D9ExInterface(*out);

    return hr;
}

bool HookIAT(HMODULE module, const char* importedModule, const char* functionName, void* hook, void** original)
{
    if (!module || !importedModule || !functionName || !hook)
        return false;

    PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;

    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return false;

    IMAGE_DATA_DIRECTORY importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDir.VirtualAddress)
        return false;

    PIMAGE_IMPORT_DESCRIPTOR importDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(reinterpret_cast<BYTE*>(module) + importDir.VirtualAddress);
    for (; importDesc->Name; ++importDesc)
    {
        const char* modName = reinterpret_cast<const char*>(reinterpret_cast<BYTE*>(module) + importDesc->Name);
        if (_stricmp(modName, importedModule) != 0)
            continue;

        PIMAGE_THUNK_DATA thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(module) + importDesc->FirstThunk);
        PIMAGE_THUNK_DATA origThunk = importDesc->OriginalFirstThunk
            ? reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(module) + importDesc->OriginalFirstThunk)
            : nullptr;

        for (; thunk->u1.Function; ++thunk, origThunk ? ++origThunk : origThunk)
        {
            if (!origThunk)
                continue;

            if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal))
                continue;

            PIMAGE_IMPORT_BY_NAME importByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(reinterpret_cast<BYTE*>(module) + origThunk->u1.AddressOfData);
            if (strcmp(reinterpret_cast<const char*>(importByName->Name), functionName) != 0)
                continue;

            DWORD oldProtect = 0;
            if (!VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect))
                return false;

            if (original && !*original)
                *original = reinterpret_cast<void*>(thunk->u1.Function);

            thunk->u1.Function = reinterpret_cast<ULONG_PTR>(hook);
            VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
            return true;
        }
    }

    return false;
}

bool HookD3D9Imports()
{
    DebugLog("CoopCompanion: HookD3D9Imports start.\n");
    HMODULE module = GetModuleHandleW(nullptr);
    if (!module)
        return false;

    bool ok9 = HookIAT(module, "d3d9.dll", "Direct3DCreate9", reinterpret_cast<void*>(&HookedDirect3DCreate9), reinterpret_cast<void**>(&g_originalCreate9));
    bool ok9ex = HookIAT(module, "d3d9.dll", "Direct3DCreate9Ex", reinterpret_cast<void*>(&HookedDirect3DCreate9Ex), reinterpret_cast<void**>(&g_originalCreate9Ex));
    if (ok9 || ok9ex)
        DebugLog("CoopCompanion: HookD3D9Imports success.\n");
    else
        DebugLog("CoopCompanion: HookD3D9Imports failed.\n");

    return ok9 || ok9ex;
}

HWND CreateDummyWindow(HINSTANCE instance)
{
    const wchar_t* className = L"CoopCompanionDummyWindow";
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    wc.style = CS_CLASSDC;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return nullptr;

    return CreateWindowExW(
        0,
        className,
        L"CoopCompanion",
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        100,
        100,
        nullptr,
        nullptr,
        instance,
        nullptr
    );
}

void DestroyDummyWindow(HINSTANCE instance, HWND hwnd)
{
    if (hwnd)
        DestroyWindow(hwnd);

    UnregisterClassW(L"CoopCompanionDummyWindow", instance);
}

bool InstallD3D9Hooks(HWND hwnd)
{
    DebugLog("CoopCompanion: InstallD3D9Hooks start.\n");
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        DebugLog("CoopCompanion: Direct3DCreate9 failed.\n");
        return false;
    }

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = hwnd;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;

    IDirect3DDevice9* device = nullptr;
    HRESULT hr = d3d->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &pp,
        &device
    );
    if (FAILED(hr) || !device)
    {
        DebugLogF("CoopCompanion: D3D9 CreateDevice failed (hr=0x%08X).\n", static_cast<unsigned int>(hr));
        d3d->Release();
        return false;
    }
    DebugLog("CoopCompanion: D3D9 CreateDevice ok.\n");

    bool ok = HookDeviceVTable(device);

    device->Release();
    d3d->Release();

    DebugLog(ok ? "CoopCompanion: InstallD3D9Hooks success.\n" : "CoopCompanion: InstallD3D9Hooks failed.\n");
    return ok;
}

bool InstallD3D9ExHooks(HWND hwnd)
{
    DebugLog("CoopCompanion: InstallD3D9ExHooks start.\n");
    using Direct3DCreate9ExFn = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);

    HMODULE d3d9 = GetModuleHandleW(L"d3d9.dll");
    if (!d3d9)
        d3d9 = LoadLibraryW(L"d3d9.dll");

    if (!d3d9)
    {
        DebugLog("CoopCompanion: d3d9.dll not loaded.\n");
        return false;
    }

    auto create9Ex = reinterpret_cast<Direct3DCreate9ExFn>(GetProcAddress(d3d9, "Direct3DCreate9Ex"));
    if (!create9Ex)
    {
        DebugLog("CoopCompanion: Direct3DCreate9Ex not found.\n");
        return false;
    }

    IDirect3D9Ex* d3d = nullptr;
    if (FAILED(create9Ex(D3D_SDK_VERSION, &d3d)) || !d3d)
    {
        DebugLog("CoopCompanion: Direct3DCreate9Ex failed.\n");
        return false;
    }

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = hwnd;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;

    IDirect3DDevice9Ex* device = nullptr;
    HRESULT hr = d3d->CreateDeviceEx(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &pp,
        nullptr,
        &device
    );

    if (FAILED(hr) || !device)
    {
        DebugLogF("CoopCompanion: D3D9Ex CreateDeviceEx failed (hr=0x%08X).\n", static_cast<unsigned int>(hr));
        d3d->Release();
        return false;
    }
    DebugLog("CoopCompanion: D3D9Ex CreateDeviceEx ok.\n");

    void** vtable = *reinterpret_cast<void***>(device);
    bool ok = PatchVTable(vtable, kEndSceneIndex, reinterpret_cast<void*>(&HookedEndScene), reinterpret_cast<void**>(&g_originalEndScene));
    ok = ok && PatchVTable(vtable, kResetIndex, reinterpret_cast<void*>(&HookedReset), reinterpret_cast<void**>(&g_originalReset));
    ok = ok && PatchVTable(vtable, kPresentIndex, reinterpret_cast<void*>(&HookedPresent), reinterpret_cast<void**>(&g_originalPresent));

    IDirect3DSwapChain9* swapChain = nullptr;
    if (SUCCEEDED(device->GetSwapChain(0, &swapChain)) && swapChain)
    {
        void** scVtable = *reinterpret_cast<void***>(swapChain);
        ok = ok && PatchVTable(scVtable, kSwapChainPresentIndex, reinterpret_cast<void*>(&HookedSwapChainPresent), reinterpret_cast<void**>(&g_originalSwapChainPresent));
        swapChain->Release();
    }

    device->Release();
    d3d->Release();

    DebugLog(ok ? "CoopCompanion: InstallD3D9ExHooks success.\n" : "CoopCompanion: InstallD3D9ExHooks failed.\n");
    return ok;
}

bool InstallHooks()
{
    DebugLog("CoopCompanion: InstallHooks start.\n");
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HWND hwnd = CreateDummyWindow(instance);
    if (!hwnd)
    {
        DebugLog("CoopCompanion: CreateDummyWindow failed.\n");
        return false;
    }

    bool ok = InstallD3D9Hooks(hwnd);
    ok = InstallD3D9ExHooks(hwnd) || ok;

    DestroyDummyWindow(instance, hwnd);
    const bool foundExistingDevice = HookExistingD3D9Device();
    if (!foundExistingDevice)
        DebugLog("CoopCompanion: late D3D9 hook unavailable; waiting for a new device.\n");
    DebugLog(ok ? "CoopCompanion: InstallHooks success.\n" : "CoopCompanion: InstallHooks failed.\n");
    return ok;
}