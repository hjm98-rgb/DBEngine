// win32_main.cpp - DBEngine Windows 原生窗口后端：用 GDI 窗口显示引擎(软件渲染)的画面。
// 复用心形化的渲染内核(跨平台)，只换窗口层。CI 用 MSVC 编译成 dbengine-win.exe。
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "engine/framebuffer.hpp"
#include "engine/voxel.hpp"
#include "engine/texture.hpp"
#include <cmath>
using namespace dbg;

static const int WIN_W = 640, WIN_H = 360;
static Framebuffer* g_fb = nullptr;

static void renderFrame(float yaw) {
    static VoxelRenderer vr;
    static TexturePack tp;
    static bool init = false;
    if (!init) { init = true; }   // 默认贴图即可（Windows二进制不依赖PNG/zlib）
    World w(48, 32, 48); w.generate();
    Camera3 cam; cam.pos = V3(24, 26, 24); cam.yaw = yaw; cam.pitch = -0.15f;
    vr.render(*g_fb, cam, w, 14, tp);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
            bi.bmiHeader.biWidth = WIN_W; bi.bmiHeader.biHeight = -WIN_H;
            bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
            // 帧缓冲是 ABGR(小端BGRA)，正好是 GDI 需要的顺序
            SetDIBitsToDevice(hdc, 0, 0, WIN_W, WIN_H, 0, 0, 0, WIN_H,
                              g_fb ? g_fb->px.data() : nullptr, &bi, DIB_RGB_COLORS);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    Framebuffer fb(WIN_W, WIN_H); g_fb = &fb;
    WNDCLASS wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = TEXT("DBEngineWin");
    RegisterClass(&wc);
    HWND hwnd = CreateWindow(TEXT("DBEngineWin"), TEXT("DBEngine - Windows"),
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             WIN_W, WIN_H, nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, nShow);

    SetTimer(hwnd, 1, 30, nullptr);
    MSG msg; float yaw = 0.6f;
    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        renderFrame(yaw);
        yaw += 0.008f;
        InvalidateRect(hwnd, nullptr, FALSE);
        Sleep(16);
    }
    return 0;
}
