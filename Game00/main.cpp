#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(h, m, w, l);
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    const wchar_t* cls = L"CGameWnd";
    WNDCLASSEX wc{ sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst; 
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = cls;
    RegisterClassEx(&wc);

    HWND wnd = CreateWindowEx(
        0, cls, L"2D Starter", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 640,
        nullptr, nullptr, hInst, nullptr);
    ShowWindow(wnd, SW_SHOW);

    // timing
    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    // player
    float x = 100.f, y = 100.f;
	float phi = 0.f; // angle
    const int w = 64, h = 64;

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
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { vx += 300.f; phi -= 0.01; }
		if (GetAsyncKeyState(VK_LEFT) & 0x8000) { vx -= 300.f; phi += 0.01; }
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) vy += 300.f;
        if (GetAsyncKeyState(VK_UP) & 0x8000) vy -= 300.f;

        x += vx * dt; y += vy * dt;

        // clamp to client rect
        RECT rc; GetClientRect(wnd, &rc);
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (x > rc.right - w)  x = float(rc.right - w);
        if (y > rc.bottom - h) y = float(rc.bottom - h);
		if (phi < 0) phi = 0; if (phi > 3.14f) phi = 3.14f;

        // draw (simple double buffer)
        HDC hdc = GetDC(wnd);
        HDC mdc = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(mdc, bmp);

        HBRUSH bg = CreateSolidBrush(RGB(30, 30, 40));
        FillRect(mdc, &rc, bg); DeleteObject(bg);

        // draw a line to indicate the angle
        HPEN pen = CreatePen(PS_SOLID, 30, RGB(255, 255, 255)); // 10px, white
        HPEN oldPen = (HPEN)SelectObject(mdc, pen);
        MoveToEx(mdc, 400, 600, nullptr);
        LineTo(mdc, 400 + (int)(150 * cos(phi)), 600 - (int)(150 * sin(phi)));
        SelectObject(mdc, oldPen);
        DeleteObject(pen);

        RECT r{ (int)x, (int)y, (int)x + w, (int)y + h };
        HBRUSH br = CreateSolidBrush(RGB(250, 170, 30));
        // FillRect(mdc, &r, br); 
		HBRUSH oldBr = (HBRUSH)SelectObject(mdc, br);
        Ellipse(mdc, 300, 500, 500, 700);
		
		SelectObject(mdc, oldBr);
		DeleteObject(oldBr);
        DeleteObject(br);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mdc, 0, 0, SRCCOPY);
        SelectObject(mdc, oldBmp);
        DeleteObject(bmp);
        DeleteDC(mdc);
        ReleaseDC(wnd, hdc);

        Sleep(1); // be nice to CPU
    }
    return 0;
}
