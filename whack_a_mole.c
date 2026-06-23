#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int score = 0;
static int cx = 200, cy = 200;   /* button center */
static const int BW = 80, BH = 40; /* button size */
static RECT btn_rect;
static HWND hwnd_btn;
static HWND hwnd_main;
static HBRUSH bg_brush;
static HBRUSH btn_brush;

static RECT desk_rect; /* usable work area */

static void randomize_button(void) {
    RECT cr;
    GetClientRect(hwnd_main, &cr);
    int w = (cr.right - 200 > 100) ? cr.right - 200 : 100;
    int h = (cr.bottom - 200 > 100) ? cr.bottom - 200 : 100;
    cx = 50 + rand() % w;
    cy = 50 + rand() % h;
    btn_rect.left   = cx - BW/2;
    btn_rect.right  = cx + BW/2;
    btn_rect.top    = cy - BH/2;
    btn_rect.bottom = cy + BH/2;
    if (hwnd_btn) {
        SetWindowPos(hwnd_btn, NULL,
            btn_rect.left, btn_rect.top,
            BW, BH, SWP_NOZORDER);
        InvalidateRect(hwnd_btn, NULL, TRUE);
    }
}

static LRESULT CALLBACK BtnWndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_LBUTTONDOWN) {
        score++;
        char buf[64];
        sprintf(buf, "Whack-a-Mole - SCORE: %d", score);
        SetWindowTextA(hwnd_main, buf);
        randomize_button();
        return 0;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hw, &ps);
        RECT r; GetClientRect(hw, &r);
        FillRect(hdc, &r, btn_brush);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255,255,255));
        HFONT hf = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
        SelectObject(hdc, hf);
        RECT tr = r;
        DrawTextA(hdc, "WHACK ME!", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DeleteObject(hf);
        EndPaint(hw, &ps);
        return 0;
    }
    return DefWindowProcW(hw, msg, wp, lp);
}

static LRESULT CALLBACK MainWndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CREATE) {
        hwnd_main = hw;
        srand((unsigned)time(NULL));
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &desk_rect, 0);
        bg_brush = CreateSolidBrush(RGB(30, 30, 40));
        btn_brush = CreateSolidBrush(RGB(220, 50, 50));
        randomize_button();
        hwnd_btn = CreateWindowExW(0, L"WHACK_BUTTON", L"WHACK ME!",
            WS_CHILD | WS_VISIBLE,
            btn_rect.left, btn_rect.top, BW, BH,
            hw, (HMENU)1, GetModuleHandleW(NULL), NULL);
        if (!hwnd_btn) {
            MessageBoxA(NULL, "Failed to create button", "Error", MB_OK);
        }
        SetTimer(hw, 1, 100, NULL); /* keep responsive */
        return 0;
    }
    if (msg == WM_DESTROY) {
        DeleteObject(bg_brush);
        DeleteObject(btn_brush);
        PostQuitMessage(0);
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        HDC hdc = (HDC)wp;
        RECT r; GetClientRect(hw, &r);
        FillRect(hdc, &r, bg_brush);
        return 1;
    }
    if (msg == WM_SIZE) {
        if (hwnd_btn) {
            /* just keep button where it is */
        }
        return 0;
    }
    return DefWindowProcW(hw, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    WNDCLASSW mwc = {0};
    mwc.lpfnWndProc   = MainWndProc;
    mwc.hInstance     = hInst;
    mwc.hCursor       = LoadCursorW(NULL, (LPCWSTR)MAKEINTRESOURCE(32512));
    mwc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    mwc.lpszClassName = L"WHACK_MAIN";
    RegisterClassW(&mwc);

    WNDCLASSW bwc = {0};
    bwc.lpfnWndProc   = BtnWndProc;
    bwc.hInstance     = hInst;
    bwc.hCursor       = LoadCursorW(NULL, (LPCWSTR)MAKEINTRESOURCE(32649));
    bwc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    bwc.lpszClassName = L"WHACK_BUTTON";
    RegisterClassW(&bwc);

    RECT wr = {100, 100, 700, 500};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hw = CreateWindowExW(0, L"WHACK_MAIN", L"Whack-a-Mole - SCORE: 0",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInst, NULL);
    if (!hw) return 1;

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
