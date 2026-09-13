#include "ui.h"
#include "hooks.h"
#include "logging.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

void InitImGui(IDirect3DDevice9* device)
{
    if (g_imguiInitialized)
        return;

    DebugLog("CoopCompanion: InitImGui start.\n");

    D3DDEVICE_CREATION_PARAMETERS params = {};
    if (SUCCEEDED(device->GetCreationParameters(&params)))
    {
        g_gameWindow = params.hFocusWindow;
    }
    else
    {
        DebugLog("CoopCompanion: GetCreationParameters failed.\n");
    }

    if (!g_gameWindow)
        g_gameWindow = GetForegroundWindow();

    if (!g_gameWindow)
    {
        DebugLog("CoopCompanion: InitImGui no window handle.\n");
        MessageBoxA(nullptr, "CoopCompanion: InitImGui no window handle.\n", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(g_gameWindow);
    ImGui_ImplDX9_Init(device);

    g_originalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(g_gameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc))
    );

    g_imguiInitialized = true;
}

void RenderImGui()
{
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_menuOpen)
    {
        ImGui::Begin("Alice Coop Companion", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ImGui::BeginTabBar("CompanionGroups"))
        {
            if (ImGui::BeginTabItem("Main"))
            {
                ImGui::Text("Overlay injected.");
                ImGui::Text("Press Insert to toggle this menu.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Logs"))
            {
                ImGui::InputTextMultiline("##Logs", g_logBuffer, sizeof(g_logBuffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16), ImGuiInputTextFlags_ReadOnly);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void SetOverlayCursorVisible(bool visible)
{
    if (visible)
    {
        if (g_overlayCursorShowCalls)
            return;

        // ShowCursor uses a process-wide display counter. Track our own
        // increments so closing the menu restores the game's prior state.
        do
        {
            ++g_overlayCursorShowCalls;
        } while (ShowCursor(TRUE) < 0);
        return;
    }

    while (g_overlayCursorShowCalls > 0)
    {
        ShowCursor(FALSE);
        --g_overlayCursorShowCalls;
    }
}

void RenderOverlay(IDirect3DDevice9* device, bool wrapBeginScene)
{
    if (!g_imguiInitialized)
        InitImGui(device);

    if (!g_imguiInitialized)
        return;

    if (GetAsyncKeyState(VK_INSERT) & 1)
    {
        g_menuOpen = !g_menuOpen;
    }

    SetOverlayCursorVisible(g_menuOpen);

    if (wrapBeginScene && SUCCEEDED(device->BeginScene()))
    {
        RenderImGui();
        device->EndScene();
    }
    else
    {
        RenderImGui();
    }

    // The window close button can update g_menuOpen during RenderImGui.
    SetOverlayCursorVisible(g_menuOpen);
}
