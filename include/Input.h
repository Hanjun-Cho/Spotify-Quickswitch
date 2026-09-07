#pragma once

#include <windows.h>

namespace Input {
    void startHook();
    void stopHook();
    
    bool altHeld();
    bool controlHeld();

    POINT getCursor();
    MONITORINFO getMonitor();
}
