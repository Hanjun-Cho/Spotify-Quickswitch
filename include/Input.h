#pragma once

#include <windows.h>

namespace Input {
    void startHook();
    void stopHook();
    
    bool controlHeld();
    bool altHeld();
    bool qHeld();

    POINT getCursor();
    MONITORINFO getMonitor();
}
