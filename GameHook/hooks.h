#pragma once

#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <string.h>
#include <winnt.h>

using EndSceneFn = HRESULT(APIENTRY*)(IDirect3DDevice9*);
using PresentFn = HRESULT(APIENTRY*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using SwapChainPresentFn = HRESULT(APIENTRY*)(IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
using ResetFn = HRESULT(APIENTRY*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using CreateDeviceFn = HRESULT(APIENTRY*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
using CreateDeviceExFn = HRESULT(APIENTRY*)(IDirect3D9Ex*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*, IDirect3DDevice9Ex**);
using Direct3DCreate9Fn = IDirect3D9*(WINAPI*)(UINT);
using Direct3DCreate9ExFn = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);

static EndSceneFn g_originalEndScene = nullptr;
static PresentFn g_originalPresent = nullptr;
static SwapChainPresentFn g_originalSwapChainPresent = nullptr;
static ResetFn g_originalReset = nullptr;
static CreateDeviceFn g_originalCreateDevice = nullptr;
static CreateDeviceExFn g_originalCreateDeviceEx = nullptr;
static Direct3DCreate9Fn g_originalCreate9 = nullptr;
static Direct3DCreate9ExFn g_originalCreate9Ex = nullptr;

constexpr int kSwapChainPresentIndex = 3;
constexpr int kEndSceneIndex = 42;
constexpr int kResetIndex = 16;
constexpr int kPresentIndex = 17;
constexpr int kCreateDeviceIndex = 16;
constexpr int kCreateDeviceExIndex = 20;
constexpr int kDeviceSignatureIndices[] = { 3, 4, 5, 6, 7, 8, 9, 10, 40, 41 };

static bool g_loggedEndScene = false;
static bool g_renderedThisFrame = false;
static bool g_loggedPresent = false;
static bool g_loggedSwapPresent = false;
static void* g_deviceSignature[_countof(kDeviceSignatureIndices)] = {};

bool HookD3D9Imports();
bool InstallHooks();
LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
