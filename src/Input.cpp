#include "Input.h"
#include <windows.h>

namespace {
    HHOOK hook = nullptr;
    bool controlDown = false;
    bool altDown = false;
    bool qDown = false;

    // A dropped key-up (rare under a low-level hook) would leave a latch stuck
    // on. Later, merely pressing Ctrl+Alt would then look like the full combo
    // and pop the wheel open without Q. Reconcile each latch against the real
    // physical key state before it is read so a lost release self-corrects.
    void reconcileWithPhysicalState() {
        if (!(GetAsyncKeyState(VK_CONTROL) & 0x8000)) { controlDown = false; }
        if (!(GetAsyncKeyState(VK_MENU) & 0x8000)) { altDown = false; }
        if (!(GetAsyncKeyState('Q') & 0x8000)) { qDown = false; }
    }

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
        reconcileWithPhysicalState();
        return controlDown;
    }

    bool altHeld() {
        reconcileWithPhysicalState();
        return altDown;
    }

    bool qHeld() {
        reconcileWithPhysicalState();
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
