#include "Input.h"
#include <windows.h>

namespace {
    HHOOK hook = nullptr;
    bool altDown = false;
    bool controlDown = false;

    LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION) {
            KBDLLHOOKSTRUCT* key = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                if (key->vkCode == VK_LMENU) { altDown = true; }
                if (key->vkCode == VK_LCONTROL) { controlDown = true; }
            }

            if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                if (key->vkCode == VK_LMENU) { altDown = false; }
                if (key->vkCode == VK_LCONTROL) { controlDown = false; }
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

    bool altHeld() {
        return altDown;
    }

    bool controlHeld() {
        return controlDown;
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
