#include "Tray.h"

#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr UINT WM_TRAYICON = WM_APP + 1;

constexpr const wchar_t* AUTOSTART_KEY =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* AUTOSTART_NAME = L"SpotifyQuickWheel";
constexpr const wchar_t* TRAY_CLASS = L"SpotifyQuickWheelTrayWindow";

enum MenuId {
    MenuAutostart = 1,
    MenuExit = 2,
};

std::atomic<bool> g_exit{false};
std::atomic<bool> g_running{false};

HWND g_hwnd = nullptr;
HINSTANCE g_instance = nullptr;
HICON g_icon = nullptr;

std::thread g_thread;
std::once_flag g_once;

// ---- Generated tray icon ------------------------------------------------

struct Pixels {
    int width = 32;
    int height = 32;
    std::vector<unsigned char> data; // premultiplied BGRA, top-down

    Pixels() : data((size_t)width * height * 4, 0) {}

    void set(int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
        if (x < 0 || x >= width || y < 0 || y >= height || a == 0) { return; }
        size_t i = ((size_t)y * width + x) * 4;
        double na = a / 255.0;
        data[i + 0] = (unsigned char)(b * na);
        data[i + 1] = (unsigned char)(g * na);
        data[i + 2] = (unsigned char)(r * na);
        data[i + 3] = a;
    }
};

bool inDisc(double px, double py, double cx, double cy, double r) {
    double dx = px - cx, dy = py - cy;
    return dx * dx + dy * dy <= r * r;
}

bool inEllipse(double px, double py, double cx, double cy, double rx, double ry) {
    double dx = (px - cx) / rx, dy = (py - cy) / ry;
    return dx * dx + dy * dy <= 1.0;
}

void drawMusicNote(Pixels& px, int cx, int cy) {
    // Simple single music note: head (lower-left), stem (up from the head),
    // and a flag beam toward the upper-right.
    double headCx = cx - 4.0, headCy = cy + 3.0; // note head
    double headRx = 5.5, headRy = 3.6;
    double stemLeft = cx - 0.5, stemRight = cx + 1.5;
    double stemTop = cy - 9.0, stemBottom = cy + 3.0;
    double beamCx = cx + 4.5, beamCy = cy - 7.0, beamRx = 3.6, beamRy = 1.5;

    double lo = std::floor((cy - 10.0));
    double hi = std::ceil((cy + 7.0));
    for (int y = (int)lo; y <= (int)hi; ++y) {
        double yy = y + 0.5;
        double xStart = std::floor(headCx - headRx);
        double xEnd = std::ceil(beamCx + beamRx);
        for (int x = (int)xStart; x <= (int)xEnd; ++x) {
            double xx = x + 0.5;
            bool inside = inEllipse(xx, yy, headCx, headCy, headRx, headRy) ||
                          inEllipse(xx, yy, beamCx, beamCy, beamRx, beamRy) ||
                          (xx >= stemLeft && xx <= stemRight && yy >= stemTop && yy <= stemBottom);
            if (inside) {
                px.set(x, y, 255, 255, 255, 255);
            }
        }
    }
}

HICON createTrayIcon() {
    const int S = 32;
    Pixels px;

    const double center = (S - 1) / 2.0;
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            if (inDisc(x + 0.5, y + 0.5, center, center, 15.0)) {
                px.set(x, y, 0x1D, 0xB9, 0x54, 255);
            }
        }
    }
    drawMusicNote(px, 16, 16);

    BITMAPV5HEADER bi{};
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = S;
    bi.bV5Height = -S; // top-down
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    HDC hdc = GetDC(nullptr);
    if (!hdc) { return nullptr; }

    void* bits = nullptr;
    HBITMAP color = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!color || !bits) {
        if (color) { DeleteObject(color); }
        return nullptr;
    }

    std::memcpy(bits, px.data.data(), px.data.size());

    HBITMAP mask = CreateBitmap(1, 1, 1, 1, nullptr);

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = color;
    ii.hbmMask = mask;
    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(color);
    DeleteObject(mask);
    return icon;
}

// ---- Autostart (registry) ----------------------------------------------

std::wstring exePath() {
    std::wstring path(MAX_PATH, L'\0');
    DWORD n = GetModuleFileNameW(nullptr, &path[0], (DWORD)path.size());
    if (n == 0) { return L""; }
    path.resize(n);
    return path;
}

} // namespace

namespace Tray {

bool autoStartEnabled() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0;
    LONG res = RegQueryValueExW(key, AUTOSTART_NAME, nullptr, &type, nullptr, nullptr);
    RegCloseKey(key);
    return res == ERROR_SUCCESS && type == REG_SZ;
}

void setAutoStartEnabled(bool enable) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }

    if (enable) {
        std::wstring path = L"\"" + exePath() + L"\"";
        RegSetValueExW(key, AUTOSTART_NAME, 0, REG_SZ,
                       (const BYTE*)path.c_str(), (DWORD)((path.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, AUTOSTART_NAME);
    }
    RegCloseKey(key);
}

} // namespace Tray

namespace {

void refreshCheck(HMENU menu, UINT id) {
    UINT flags = MF_BYCOMMAND;
    if (Tray::autoStartEnabled()) { flags |= MF_CHECKED; }
    else { flags |= MF_UNCHECKED; }
    CheckMenuItem(menu, id, flags);
}

void buildAndShowMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) { return; }

    AppendMenuW(menu, MF_STRING, MenuAutostart, L"Launch at startup");
    refreshCheck(menu, MenuAutostart);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MenuExit, L"Quit Spotify Quick Wheel");

    POINT pt;
    GetCursorPos(&pt);

    // Required so the menu can be dismissed by clicking elsewhere.
    SetForegroundWindow(hwnd);

    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                              pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    switch (cmd) {
        case MenuAutostart:
            Tray::setAutoStartEnabled(!Tray::autoStartEnabled());
            break;
        case MenuExit:
            g_exit.store(true);
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            break;
        default:
            break;
    }
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                buildAndShowMenu(hwnd);
            }
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (g_icon) {
                NOTIFYICONDATAW nid{};
                nid.cbSize = sizeof(nid);
                nid.hWnd = hwnd;
                nid.uID = 1;
                Shell_NotifyIconW(NIM_DELETE, &nid);
            }
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

bool registerWindowClass() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = windowProc;
    wc.hInstance = g_instance;
    wc.lpszClassName = TRAY_CLASS;
    return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

void trayThread() {
    if (!registerWindowClass()) { return; }

    g_hwnd = CreateWindowExW(0, TRAY_CLASS, L"SpotifyQuickWheel", 0,
                             CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, nullptr, nullptr,
                             g_instance, nullptr);
    if (!g_hwnd) { return; }

    g_icon = createTrayIcon();
    if (!g_icon) { g_icon = LoadIconW(nullptr, MAKEINTRESOURCEW(IDI_APPLICATION)); }

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = g_icon;
    wcscpy_s(nid.szTip, L"Spotify Quick Wheel");

    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        DestroyWindow(g_hwnd);
        return;
    }

    g_running.store(true);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_running.store(false);
    g_hwnd = nullptr;
}

} // namespace

namespace Tray {

void start() {
    std::call_once(g_once, [] {
        g_instance = (HINSTANCE)GetModuleHandleW(nullptr);
        g_thread = std::thread(trayThread);
    });
}

void stop() {
    if (g_running.load() && g_hwnd) {
        PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
    }
    if (g_thread.joinable()) {
        g_thread.join();
    }
}

bool exitRequested() {
    return g_exit.load();
}

} // namespace Tray
