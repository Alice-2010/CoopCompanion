#pragma once

#include "imgui/imgui.h"

#include <windows.h>
#include <d3d9.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HWND g_gameWindow = nullptr;
static WNDPROC g_originalWndProc = nullptr;
static bool g_imguiInitialized = false;
static bool g_menuOpen = true;
static int g_overlayCursorShowCalls = 0;

void SetOverlayCursorVisible(bool);
void RenderOverlay(IDirect3DDevice9*, bool);
