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
#include <random>

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

    // Resolve sprite locations relative to the executable directory
    wchar_t exePath[MAX_PATH]{};
    DWORD exeLen = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir;
    if (exeLen > 0 && exeLen < MAX_PATH) {
        exeDir.assign(exePath, exeLen);
        size_t slash = exeDir.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            exeDir.resize(slash + 1);
        } else {
            exeDir.clear();
        }
    }

    {
        std::wstring tankBodyPath = exeDir + L"Tank_Body.png";
        std::wstring tankBarrelPath = exeDir + L"Tank_Barrel.png";
        std::wstring tankBulletPath = exeDir + L"Tank_Bullet.png";

        Gdiplus::Image tankBody(tankBodyPath.c_str());
        Gdiplus::Image tankBarrel(tankBarrelPath.c_str());
        Gdiplus::Image tankBullet(tankBulletPath.c_str());

        const bool bodyOk = tankBody.GetLastStatus() == Gdiplus::Ok;
        const bool barrelOk = tankBarrel.GetLastStatus() == Gdiplus::Ok;
        const bool bulletOk = tankBullet.GetLastStatus() == Gdiplus::Ok;

        if (!bodyOk || !barrelOk) {
            MessageBox(wnd, L"Tank sprites not found next to the executable.", L"Image Load Error", MB_ICONERROR);
        } else {
            // Player
            float x = 100.f, y = 100.f;
            float phi = 0.f;
            const int wImg = 128, hImg = 128;

            // Bullet state
            bool isBulletActive = false;
            float bulletX = 0.f, bulletY = 0.f;
            float bulletVx = 0.f, bulletVy = 0.f;
            const float bulletSpeed = 700.f; // pixels per second
            const float gravity = 600.f;     // pixels per second^2 (positive = down)
            const int bulletDrawW = 16, bulletDrawH = 16; // draw size
            bool prevSpaceDown = false;

            std::random_device rd;
            std::mt19937 rng(rd());
            std::uniform_int_distribution<int> heliTopDist(100, 200);
            std::uniform_int_distribution<int> heliDirection(0, 1);

            float leftXHeli = 0, topYHeli = 0;
            float rightXHeli = 0, bottomYHeli = 0;
            float VXHeli = 0;
            bool isHeliActive = false;

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

                float vx = 0.f, vy = 0.f;
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { vx += 300.f; phi -= 0.01f; }
                if (GetAsyncKeyState(VK_LEFT) & 0x8000) { vx -= 300.f; phi += 0.01f; }
                if (GetAsyncKeyState(VK_DOWN) & 0x8000) vy += 300.f;
                if (GetAsyncKeyState(VK_UP) & 0x8000) vy -= 300.f;

                x += vx * dt; y += vy * dt;

                RECT rc; GetClientRect(wnd, &rc);
                if (x < 0) x = 0; if (y < 0) y = 0;
                if (x > rc.right - wImg)  x = float(rc.right - wImg);
                if (y > rc.bottom - hImg) y = float(rc.bottom - hImg);
                if (phi < 0) phi = 0; if (phi > 3.1415926f) phi = 3.1415926f;

                // Compute pivot used for barrel and the white dot (muzzle)
                const float pivotX = (rc.right + rc.left) / 2.0f;
                const float pivotY = rc.bottom - 37.0f;
                const float muzzleRadius = 70.0f;
                const float muzzleX = pivotX + muzzleRadius * cosf(phi);
                const float muzzleY = pivotY - muzzleRadius * sinf(phi);

                // Handle space key edge (spawn bullet once on press)
                bool spaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
                if (spaceDown && !prevSpaceDown) {
                    if (!isBulletActive) {
                        isBulletActive = true;
                        // Spawn at white dot (muzzle)
                        bulletX = muzzleX;
                        bulletY = muzzleY;
                        bulletVx = bulletSpeed * cosf(phi);
                        bulletVy = -bulletSpeed * sinf(phi); // negative because screen Y grows down
                    }
                }
                prevSpaceDown = spaceDown;

                // Update bullet physics
                if (isBulletActive) {
                    bulletVy += gravity * dt;
                    bulletX += bulletVx * dt;
                    bulletY += bulletVy * dt;

                    if (bulletX < -50 || bulletY < -50 || bulletX > rc.right + 50 || bulletY > rc.bottom + 50) {
                        isBulletActive = false;
                    }
                }

                if (isHeliActive) {
                    leftXHeli += VXHeli * dt;
                    rightXHeli += VXHeli * dt;
                    if (leftXHeli > rc.right || rightXHeli < rc.left) {
                        isHeliActive = false;
                    }
                    if (isBulletActive && 
                        leftXHeli <= bulletX && bulletX <= rightXHeli && 
                        topYHeli <= bulletY && bulletY <= bottomYHeli) 
                    {
                        isHeliActive = false;
                        isBulletActive = false;
                    }
                }
                else {
                    int HeliDir = heliDirection(rng);
                    if (HeliDir) {
                        leftXHeli = 0;
                        rightXHeli = 50;
                        VXHeli = 100;
                    }
                    else {
                        leftXHeli = rc.right - 50;
                        rightXHeli = rc.right;
                        VXHeli = -100;
                    }
                    isHeliActive = true;
                    topYHeli = heliTopDist(rng);
                    bottomYHeli = topYHeli + 20;
                }

                HDC hdc = GetDC(wnd);
                HDC mdc = CreateCompatibleDC(hdc);
                HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
                HGDIOBJ oldBmp = SelectObject(mdc, bmp);

                HBRUSH bg = CreateSolidBrush(RGB(30, 30, 40));
                FillRect(mdc, &rc, bg);
                DeleteObject(bg);

                {
                    // Draw barrel + white dot (muzzle)
                    Gdiplus::Graphics g(mdc);
                    g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
                    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

                    const float barrelW = 240.0f;
                    const float barrelH = 160.0f;

                    g.TranslateTransform(pivotX, pivotY);
                    g.RotateTransform(-phi * 180.0f / 3.14159265f + 90);
                    g.DrawImage(&tankBarrel, Gdiplus::RectF(-barrelW * 0.5f, -barrelH, barrelW, barrelH));
                    g.ResetTransform();
                }

                {
                    // Draw tank body
                    Gdiplus::Graphics g(mdc);
                    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                    g.DrawImage(&tankBody, (int)((rc.left + rc.right - wImg) / 2), (int)(rc.bottom - hImg), wImg, hImg);
                }

                // Draw bullet (on top)
                if (isBulletActive) {
                    Gdiplus::Graphics g(mdc);
                    if (bulletOk) {
                        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

                        // Angle: atan2(y, x). y is negated because screen Y increases downward.
                        float phiBullet = atan2f(bulletVy, bulletVx); 
                        float angleDeg = phiBullet * 180.0f / 3.1415926f;

                        // Move origin to bullet position, rotate around that origin,
                        // then draw the image centered at (0,0).
                        g.TranslateTransform(bulletX, bulletY);
                        g.RotateTransform(angleDeg + 90.0f); // +90 if your sprite's "up" is along +Y

                        g.DrawImage(&tankBullet, Gdiplus::RectF(
                            -bulletDrawW * 0.5f,
                            -bulletDrawH * 0.5f,
                            static_cast<Gdiplus::REAL>(bulletDrawW),
                            static_cast<Gdiplus::REAL>(bulletDrawH)
                        ));

                        // Restore to identity so subsequent drawing isn't affected
                        g.ResetTransform();
                    } else {
                        // fallback small white circle if bullet image missing
                        HBRUSH bbr = CreateSolidBrush(RGB(255, 255, 255));
                        HGDIOBJ old = SelectObject(mdc, bbr);
                        Ellipse(mdc, int(bulletX - 4), int(bulletY - 4), int(bulletX + 4), int(bulletY + 4));
                        SelectObject(mdc, old);
                        DeleteObject(bbr);
                    }
                }

                {

                    if (isHeliActive) {
                        Rectangle(mdc, leftXHeli, topYHeli, rightXHeli, bottomYHeli);
                    }
                }

                BitBlt(hdc, 0, 0, rc.right, rc.bottom, mdc, 0, 0, SRCCOPY);
                SelectObject(mdc, oldBmp);
                DeleteObject(bmp);
                DeleteDC(mdc);
                ReleaseDC(wnd, hdc);

                Sleep(1);
            }
        }
    }
    // GDI+ shutdown
    if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}