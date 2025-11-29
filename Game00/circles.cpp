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
#include <string>

// GDI+ needs ObjIDL
#include <objidl.h>
#include <gdiplus.h>
#include <gdiplustypes.h>
#pragma comment(lib, "gdiplus.lib")

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
    {
        double phi = 0;
        MSG msg{};
        bool running = true;
        while (running) {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) running = false;
                if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) running = false;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            QueryPerformanceCounter(&now);
            float dt = float(double(now.QuadPart - prev.QuadPart) / double(freq.QuadPart));
            prev = now;

            RECT rc; GetClientRect(wnd, &rc);

            HDC hdc = GetDC(wnd);
            HDC mdc = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HGDIOBJ oldBmp = SelectObject(mdc, bmp);

            HBRUSH bg = CreateSolidBrush(RGB(30, 30, 40));
            FillRect(mdc, &rc, bg);
            DeleteObject(bg);

            int n = 50;
			double dphi = 2 * 3.14159265358979323846 / n;

            for (int v = 0; v < n; ++v) {
                double angle1 = v * dphi + phi;
                int cx1 = 400 + int(300 * cos(angle1));
                int cy1 = 100 - int(50 * sin(angle1));
				Ellipse(mdc, cx1 - 10, cy1 - 10, cx1 + 10, cy1 + 10);

                double angle2 = v * dphi;
                int cx2 = 400 + int(300 * cos(angle2));
                int cy2 = 460 - int(50 * sin(angle2));
                Ellipse(mdc, cx2 - 10, cy2 - 10, cx2 + 10, cy2 + 10);

                //Colorful lines
				HPEN pen = CreatePen(PS_SOLID, 2, RGB(
                    128 + BYTE(127 * sin(angle1)),
                    128 + BYTE(127 * sin(angle1 + 2)),
                    128 + BYTE(127 * sin(angle1 + 4))
				));

				SelectObject(mdc, pen);
				MoveToEx(mdc, cx1, cy1, nullptr);
				LineTo(mdc, cx2, cy2);

				DeleteObject(pen);

            }
			phi += dt / 2;

            {
                Gdiplus::Graphics g(mdc);
                g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
                g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            }

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, mdc, 0, 0, SRCCOPY);
            SelectObject(mdc, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mdc);
            ReleaseDC(wnd, hdc);

            Sleep(1);
        }
    }
    // GDI+ shutdown
    if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}