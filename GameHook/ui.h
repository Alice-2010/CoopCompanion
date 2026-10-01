#pragma once

#include "imgui/imgui.h"

#include <windows.h>
#include <d3d9.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern HWND g_gameWindow;
extern WNDPROC g_originalWndProc;
extern bool g_imguiInitialized;
extern bool g_menuOpen;
extern int g_overlayCursorShowCalls;

void SetOverlayCursorVisible(bool);
void RenderOverlay(IDirect3DDevice9*, bool);
