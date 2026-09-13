#include "hooks.h"
#include "logging.h"
#include "ui.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

DWORD WINAPI InitThread(LPVOID)
{
    DebugLog("CoopCompanion: InitThread start.\n");
    HookD3D9Imports();
    if (!InstallHooks())
        DebugLog("CoopCompanion: failed to install D3D9 hooks.\n");

    return 0;
}

void Shutdown()
{
    SetOverlayCursorVisible(false);

    if (g_gameWindow && g_originalWndProc)
        SetWindowLongPtrA(g_gameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));

    if (g_imguiInitialized)
    {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_imguiInitialized = false;
    }
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DebugLog("CoopCompanion: DllMain attach.\n");
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, InitThread, hModule, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        DebugLog("CoopCompanion: DllMain detach.\n");
        // DllMain runs under the loader lock.  ImGui/D3D cleanup and window
        // subclass restoration can re-enter the loader or wait on the render
        // thread, which can leave the game process alive after its window has
        // closed.  On process termination Windows releases all DLL resources.
        // An explicit FreeLibrary unload must use a coordinated shutdown path
        // before unloading this DLL; it cannot be made safe from DllMain.
        if (!reserved)
            DebugLog("CoopCompanion: explicit unload requested without coordinated shutdown.\n");
    }

    return TRUE;
}
