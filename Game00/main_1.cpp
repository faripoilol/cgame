// If your project uses precompiled headers, keep this FIRST:
#ifdef __has_include
#  if __has_include("pch.h")
#    include "pch.h"
#  endif
#endif

#ifndef NOMINMAX
#  define NOMINMAX
#endif

#ifndef UNICODE
#  define UNICODE
#endif
#ifndef _UNICODE
#  define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <vector>
#include <random>
#include <algorithm>
#include <string>
#include <memory>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")

struct Vec2 {
    float x = 0.f;
    float y = 0.f;
};

static Vec2 operator+(const Vec2& a, const Vec2& b) {
    return { a.x + b.x, a.y + b.y };
}

static Vec2 operator-(const Vec2& a, const Vec2& b) {
    return { a.x - b.x, a.y - b.y };
}

static Vec2 operator*(const Vec2& v, float s) {
    return { v.x * s, v.y * s };
}

static Vec2 operator*(float s, const Vec2& v) {
    return v * s;
}
template <typename T>
static T ClampValue(T value, T minValue, T maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}


struct Projectile {
    Vec2 pos;
    Vec2 vel;
    bool active = true;
};

struct Bomb {
    Vec2 pos;
    Vec2 vel;
    bool active = true;
};

struct Helicopter {
    Vec2 pos;
    float speed = 0.f;
    int dir = 1; // +1 = left to right, -1 = right to left
    float dropCooldown = 0.f;
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE,
                      _In_ LPWSTR,
                      _In_ int nCmdShow) {
    const wchar_t* cls = L"CGameWnd_1";

    WNDCLASSEX wc{ sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = cls;
    RegisterClassEx(&wc);

    HWND wnd = CreateWindowEx(0, cls, L"Tank Defense Prototype", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 960, 720,
                              nullptr, nullptr, hInstance, nullptr);
    ShowWindow(wnd, nCmdShow);
    UpdateWindow(wnd);

    LARGE_INTEGER freq{};
    LARGE_INTEGER prev{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartupInput gsi{};
    gsi.GdiplusVersion = 1;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gsi, nullptr) != Gdiplus::Ok) {
        MessageBox(wnd, L"Failed to initialize GDI+.", L"Error", MB_ICONERROR);
        return 0;
    }

    MSG msg{};
    bool running = true;

    RECT rcClient{};
    GetClientRect(wnd, &rcClient);
    const float screenWidth = static_cast<float>(rcClient.right);
    const float screenHeight = static_cast<float>(rcClient.bottom);

    Vec2 tankCenter{ screenWidth * 0.5f, screenHeight - 40.f };
    const float tankWidth = 140.f;
    const float tankHeight = 40.f;
    const float turretLength = 150.f;
    const float turretMinAngleDeg = 10.f;
    const float turretMaxAngleDeg = 170.f;
    float turretAngleDeg = 55.f;
    float fireCooldown = 0.f;
    bool spaceWasDown = false;
    int lives = 3;
    int score = 0;
    bool gameOver = false;

    std::vector<Projectile> shells;
    std::vector<Bomb> bombs;
    std::vector<Helicopter> helicopters;

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<float> heliAlt(90.f, 240.f);
    std::uniform_real_distribution<float> heliSpeed(90.f, 160.f);
    std::uniform_int_distribution<int> heliDir(0, 1);

    const float gravity = 900.f;
    const float projectileSpeed = 800.f;
    const float bombRadius = 12.f;
    const float helicopterWidth = 120.f;
    const float helicopterHeight = 40.f;

    auto resetHelicopter = [&](Helicopter& h, int forceDir) {
        h.dir = forceDir;
        h.speed = heliSpeed(rng);
        h.pos.y = heliAlt(rng);
        h.dropCooldown = 0.f;
        if (h.dir > 0) {
            h.pos.x = -helicopterWidth;
        } else {
            h.pos.x = screenWidth + helicopterWidth;
        }
    };

    for (int i = 0; i < 3; ++i) {
        Helicopter h{};
        int dir = heliDir(rng) ? 1 : -1;
        resetHelicopter(h, dir);
        h.pos.y += i * 30.f;
        helicopters.push_back(h);
    }

    
    std::wstring exeDir;
    {
        wchar_t exePath[MAX_PATH]{};
        DWORD exeLen = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (exeLen > 0 && exeLen < MAX_PATH) {
            exeDir.assign(exePath, exeLen);
            size_t slash = exeDir.find_last_of(L"\\/");
            if (slash != std::wstring::npos) {
                exeDir.resize(slash + 1);
            } else {
                exeDir.clear();
            }
        }
    }

    std::wstring tankBodyPath = exeDir + L"Tank_Body.png";
    std::wstring tankBarrelPath = exeDir + L"Tank_Barrel.png";

    std::unique_ptr<Gdiplus::Image> tankBodyImg;
    std::unique_ptr<Gdiplus::Image> tankBarrelImg;
    bool spritesLoaded = false;
    tankBodyImg.reset(new Gdiplus::Image(tankBodyPath.c_str()));
    tankBarrelImg.reset(new Gdiplus::Image(tankBarrelPath.c_str()));
    if (tankBodyImg->GetLastStatus() == Gdiplus::Ok && tankBarrelImg->GetLastStatus() == Gdiplus::Ok) {
        spritesLoaded = true;
    } else {
        MessageBox(wnd, L"Tank sprites not found next to the executable.", L"Image Load", MB_ICONWARNING);
    }
    const float tankSpriteScale = spritesLoaded ? (1.f / 3.f) : 1.f;
    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running) {
            break;
        }

        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        double dtRaw = static_cast<double>(now.QuadPart - prev.QuadPart) / static_cast<double>(freq.QuadPart);
        prev = now;
        float dt = static_cast<float>(dtRaw);
        dt = ClampValue(dt, 0.f, 0.05f);

        fireCooldown = std::max(0.f, fireCooldown - dt);

        if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
            turretAngleDeg = ClampValue(turretAngleDeg + 60.f * dt, turretMinAngleDeg, turretMaxAngleDeg);
        }
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
            turretAngleDeg = ClampValue(turretAngleDeg - 60.f * dt, turretMinAngleDeg, turretMaxAngleDeg);
        }

        float tankVisualWidth = spritesLoaded ? static_cast<float>(tankBodyImg->GetWidth()) * tankSpriteScale : tankWidth;
        float tankVisualHeight = spritesLoaded ? static_cast<float>(tankBodyImg->GetHeight()) * tankSpriteScale : tankHeight;
        Vec2 turretBase{ tankCenter.x, tankCenter.y - tankVisualHeight };
        float turretRad = turretAngleDeg * 3.14159265f / 180.f;
        Vec2 turretDir{ std::cos(turretRad), -std::sin(turretRad) };
        float effectiveTurretLength = spritesLoaded ? static_cast<float>(tankBarrelImg->GetHeight()) * tankSpriteScale * 0.85f : turretLength;
        Vec2 turretTip = turretBase + turretDir * effectiveTurretLength;

        bool spaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        if (spaceDown && !spaceWasDown && fireCooldown <= 0.f && !gameOver) {
            Projectile shell{};
            shell.pos = turretTip;
            shell.vel = turretDir * projectileSpeed;
            shells.push_back(shell);
            fireCooldown = 0.35f;
        }
        spaceWasDown = spaceDown;

        for (auto& shell : shells) {
            if (!shell.active) {
                continue;
            }
            shell.vel.y += gravity * dt;
            shell.pos = shell.pos + shell.vel * dt;
            if (shell.pos.y > screenHeight || shell.pos.x < -50.f || shell.pos.x > screenWidth + 50.f) {
                shell.active = false;
            }
        }

        for (auto& b : bombs) {
            if (!b.active) {
                continue;
            }
            b.vel.y += gravity * dt;
            b.pos = b.pos + b.vel * dt;
            if (b.pos.y > screenHeight + 50.f) {
                b.active = false;
            }
        }

        for (auto& h : helicopters) {
            h.pos.x += h.speed * h.dir * dt;
            h.dropCooldown = std::max(0.f, h.dropCooldown - dt);

            float heliCenterX = h.pos.x + helicopterWidth * 0.5f;
            if (!gameOver && h.dropCooldown <= 0.f && std::abs(heliCenterX - tankCenter.x) < tankVisualWidth * 0.35f) {
                Bomb bomb{};
                bomb.pos = { heliCenterX, h.pos.y + helicopterHeight };
                bomb.vel = { h.speed * 0.2f * h.dir, 0.f };
                bombs.push_back(bomb);
                h.dropCooldown = 2.2f;
            }

            if (h.dir > 0 && h.pos.x > screenWidth + helicopterWidth) {
                resetHelicopter(h, -1);
            } else if (h.dir < 0 && h.pos.x + helicopterWidth < -helicopterWidth) {
                resetHelicopter(h, 1);
            }
        }

        if (!gameOver) {
            for (auto& h : helicopters) {
                Gdiplus::RectF heliRect(h.pos.x, h.pos.y, helicopterWidth, helicopterHeight);
                for (auto& shell : shells) {
                    if (!shell.active) {
                        continue;
                    }
                    if (heliRect.Contains(shell.pos.x, shell.pos.y)) {
                        shell.active = false;
                        score += 10;
                        resetHelicopter(h, h.dir > 0 ? -1 : 1);
                        break;
                    }
                }
            }

            float tankLeft = tankCenter.x - tankVisualWidth * 0.5f;
            float tankRight = tankCenter.x + tankVisualWidth * 0.5f;
            float tankTop = tankCenter.y - tankVisualHeight;
            float tankBottom = tankCenter.y;

            for (auto& b : bombs) {
                if (!b.active) {
                    continue;
                }
                if (b.pos.x >= tankLeft && b.pos.x <= tankRight &&
                    b.pos.y + bombRadius >= tankTop && b.pos.y <= tankBottom) {
                    b.active = false;
                    lives -= 1;
                    if (lives <= 0) {
                        gameOver = true;
                    }
                }
            }
        }

        shells.erase(std::remove_if(shells.begin(), shells.end(), [](const Projectile& p) { return !p.active; }), shells.end());
        bombs.erase(std::remove_if(bombs.begin(), bombs.end(), [](const Bomb& b) { return !b.active; }), bombs.end());
        HDC hdc = GetDC(wnd);
        RECT rc{};
        GetClientRect(wnd, &rc);
        HDC mdc = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(mdc, bmp);

        HBRUSH bg = CreateSolidBrush(RGB(18, 26, 36));
        FillRect(mdc, &rc, bg);
        DeleteObject(bg);

        Gdiplus::Graphics g(mdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);

        Gdiplus::SolidBrush groundBrush(Gdiplus::Color(255, 40, 70, 50));
        g.FillRectangle(&groundBrush, 0.f, tankCenter.y - tankVisualHeight * 0.5f + 20.f, screenWidth, screenHeight);

        if (spritesLoaded) {
            g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

            const float barrelDrawWidth = static_cast<float>(tankBarrelImg->GetWidth()) * tankSpriteScale;
            const float barrelDrawHeight = static_cast<float>(tankBarrelImg->GetHeight()) * tankSpriteScale;
            g.TranslateTransform(turretBase.x, turretBase.y);
            g.RotateTransform(90.f - turretAngleDeg);
            g.DrawImage(tankBarrelImg.get(), Gdiplus::RectF(-barrelDrawWidth * 0.5f, -barrelDrawHeight, barrelDrawWidth, barrelDrawHeight));
            g.ResetTransform();

            const float bodyDrawWidth = static_cast<float>(tankBodyImg->GetWidth()) * tankSpriteScale;
            const float bodyDrawHeight = static_cast<float>(tankBodyImg->GetHeight()) * tankSpriteScale;
            const float bodyX = tankCenter.x - bodyDrawWidth * 0.5f;
            const float bodyY = tankCenter.y - bodyDrawHeight;
            g.DrawImage(tankBodyImg.get(), Gdiplus::RectF(bodyX, bodyY, bodyDrawWidth, bodyDrawHeight));

            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeDefault);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
        } else {
            Gdiplus::SolidBrush tankBrush(Gdiplus::Color(255, 70, 120, 60));
            Gdiplus::RectF tankRect(tankCenter.x - tankVisualWidth * 0.5f, tankCenter.y - tankVisualHeight, tankVisualWidth, tankVisualHeight);
            g.FillRectangle(&tankBrush, tankRect);

            Gdiplus::Pen turretPen(Gdiplus::Color(255, 180, 220, 200), 10.f);
            g.DrawLine(&turretPen, turretBase.x, turretBase.y, turretTip.x, turretTip.y);
        }

        Gdiplus::SolidBrush heliBrush(Gdiplus::Color(255, 180, 60, 60));
        for (const auto& h : helicopters) {
            g.FillRectangle(&heliBrush, h.pos.x, h.pos.y, helicopterWidth, helicopterHeight);
            g.FillRectangle(&heliBrush, h.pos.x - 15.f, h.pos.y + helicopterHeight * 0.5f - 5.f, helicopterWidth + 30.f, 10.f);
        }

        Gdiplus::SolidBrush projectileBrush(Gdiplus::Color(255, 240, 240, 200));
        for (const auto& shell : shells) {
            g.FillEllipse(&projectileBrush, shell.pos.x - 6.f, shell.pos.y - 6.f, 12.f, 12.f);
        }

        Gdiplus::SolidBrush bombBrush(Gdiplus::Color(255, 200, 80, 30));
        for (const auto& b : bombs) {
            g.FillEllipse(&bombBrush, b.pos.x - bombRadius, b.pos.y - bombRadius, bombRadius * 2.f, bombRadius * 2.f);
        }

        std::wstring overlay = L"Lives: " + std::to_wstring(lives) +
            L"   Score: " + std::to_wstring(score) +
            L"   Angle: " + std::to_wstring(static_cast<int>(turretAngleDeg)) + L" deg";
        if (gameOver) {
            overlay += L"   GAME OVER";
        }

        Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::Font font(L"Segoe UI", 18.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF textRect(0.f, 10.f, screenWidth, 30.f);
        g.DrawString(overlay.c_str(), -1, &font, textRect, &fmt, &textBrush);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mdc, 0, 0, SRCCOPY);
        SelectObject(mdc, oldBmp);
        DeleteObject(bmp);
        DeleteDC(mdc);
        ReleaseDC(wnd, hdc);

        if (gameOver) {
            Sleep(1000);
            running = false;
        }

        Sleep(1);
    }

    tankBarrelImg.reset();
    tankBodyImg.reset();

    if (gdiplusToken) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
    }

    if (gameOver) {
        MessageBox(wnd, L"The tank ran out of lives.", L"Game Over", MB_ICONINFORMATION);
    }

    return 0;
}














