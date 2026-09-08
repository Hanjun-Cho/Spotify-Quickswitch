#include "Input.h"
#include <windows.h>

namespace {
    HHOOK hook = nullptr;
    bool controlDown = false;
    bool altDown = false;
    bool qDown = false;

    LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION) {
            KBDLLHOOKSTRUCT* key = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                if (key->vkCode == VK_LCONTROL) { controlDown = true; }
                if (key->vkCode == VK_LMENU) { altDown = true; }
                if (key->vkCode == 'Q') { qDown = true; }
            }

            if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                if (key->vkCode == VK_LCONTROL) { controlDown = false; }
                if (key->vkCode == VK_LMENU) { altDown = false; }
                if (key->vkCode == 'Q') { qDown = false; }
            }
        }

        return CallNextHookEx(hook, nCode, wParam, lParam);
    }
}

namespace Input {
    void startHook() {
        hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardProc, GetModuleHandle(nullptr), 0);
    }

    void stopHook() {
        if (hook) {
            UnhookWindowsHookEx(hook);
            hook = nullptr;
        }
    }

    bool controlHeld() {
        return controlDown;
    }

    bool altHeld() {
        return altDown;
    }

    bool qHeld() {
        return qDown;
    }

    POINT getCursor() {
        POINT cursor;
        GetCursorPos(&cursor);
        return cursor;
    }

    MONITORINFO getMonitor() {
        HMONITOR monitor = MonitorFromPoint(getCursor(), MONITOR_DEFAULTTONEAREST);

        MONITORINFO info{};
        info.cbSize = sizeof(info);
        GetMonitorInfo(monitor, &info);
        return info;
    }
}
