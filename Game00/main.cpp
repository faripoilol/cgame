// If your project uses precompiled headers, keep this FIRST:
#ifdef __has_include
#  if __has_include("pch.h")
#    include "pch.h"
#  endif
#endif

// Prevent <windows.h> from defining min/max macros that break C++ and GDI+
#ifndef NOMINMAX
#  define NOMINMAX
#endif

// Ensure Unicode (wWinMain, wide paths)
#ifndef UNICODE
#  define UNICODE
#endif
#ifndef _UNICODE
#  define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>

// GDI+ needs ObjIDL; include it BEFORE gdiplus.h for some VS setups
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// No 'using namespace Gdiplus;' to avoid DllExports/Status collisions

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(h, m, w, l);
}

int APIENTRY wWinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    const wchar_t* cls = L"CGameWnd";
    WNDCLASSEX wc{ sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = cls;
    RegisterClassEx(&wc);

    HWND wnd = CreateWindowEx(
        0, cls, L"2D Starter", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 640,
        nullptr, nullptr, hInstance, nullptr);
    ShowWindow(wnd, SW_SHOW);
    UpdateWindow(wnd);

    // High-resolution timer
    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    // ---- GDI+ init ----
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartupInput gsi{};
    gsi.GdiplusVersion = 1;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gsi, nullptr) != Gdiplus::Ok) {
        MessageBox(wnd, L"GDI+ failed to start.", L"Error", MB_ICONERROR);
        return 0;
    }

    // Load image once (ensure the PNG is next to your .exe or use absolute path)
    // Load images once (make sure both PNGs are next to your .exe or set “Copy to Output Directory”)
    Gdiplus::Image tankBody(L"Tank_Body.png");
    Gdiplus::Image tankBarrel(L"Tank_Barrel.png");


    // Optional: fail fast if not loaded
    if (tankBody.GetLastStatus() != Gdiplus::Ok) {
        MessageBox(wnd, L"Tank_Body.png not found or invalid.\nPut it next to the .exe.", L"Image Load Error", MB_ICONERROR);
        // You can continue without drawing image, but we exit to be explicit:
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }

    // Player state
    float x = 100.f, y = 100.f;
    float phi = 0.f;              // angle (we’ll just show the aiming ray)
    const int wImg = 128, hImg = 128; // on-screen size of PNG

    MSG msg{};
    bool running = true;
    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // dt
        QueryPerformanceCounter(&now);
        float dt = float(double(now.QuadPart - prev.QuadPart) / double(freq.QuadPart));
        prev = now;

        // input
        float vx = 0.f, vy = 0.f;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { vx += 300.f; phi -= 0.01f; }
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) { vx -= 300.f; phi += 0.01f; }
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) vy += 300.f;
        if (GetAsyncKeyState(VK_UP) & 0x8000) vy -= 300.f;

        x += vx * dt; y += vy * dt;

        // clamp to client rect
        RECT rc; GetClientRect(wnd, &rc);
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (x > rc.right - wImg)  x = float(rc.right - wImg);
        if (y > rc.bottom - hImg) y = float(rc.bottom - hImg);
        if (phi < 0) phi = 0; if (phi > 3.1415926f) phi = 3.1415926f;

        // ---- double buffer ----
        HDC hdc = GetDC(wnd);
        HDC mdc = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(mdc, bmp);

        HBRUSH bg = CreateSolidBrush(RGB(30, 30, 40));
        FillRect(mdc, &rc, bg);
        DeleteObject(bg);

        {
            Gdiplus::Graphics g(mdc);

            // Keep pixel-art crisp (no blurring)
            g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

            // ---- Barrel placement/rotation ----
            // Pivot where the white line used to start:
            const float pivotX = 400.0f;
            const float pivotY = 600.0f;

            // On-screen size of the barrel (adjust to your sprite; try 24x160 first)
            const float barrelW = 240.0f;
            const float barrelH = 160.0f;

            // We want the **bottom** of the barrel at the pivot, pointing along +cos/-sin like the line.
            // GDI+ rotation is in degrees and clockwise-positive is negative math angle, so invert sign.
            g.TranslateTransform(pivotX, pivotY);
            g.RotateTransform(-phi * 180.0f / 3.14159265f + 90);

            // Draw with origin at pivot: x = -barrelW/2 to center horizontally; y = -barrelH so bottom sits at pivot.
            g.DrawImage(&tankBarrel,
                Gdiplus::RectF(-barrelW * 0.5f, -barrelH, barrelW, barrelH));

            // Reset so later draws aren’t rotated
            g.ResetTransform();
        }


        // ---- draw PNG with GDI+ ----
        {
            Gdiplus::Graphics g(mdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(&tankBody, (int)((rc.left + rc.right - wImg) / 2), (int)(rc. bottom - hImg), wImg, hImg);
        }

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mdc, 0, 0, SRCCOPY);
        SelectObject(mdc, oldBmp);
        DeleteObject(bmp);
        DeleteDC(mdc);
        ReleaseDC(wnd, hdc);

        Sleep(1);
    }

    // ---- GDI+ shutdown ----
    if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}
