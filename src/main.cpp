#include "Wheel.h"
#include "Window.h"
#include "Input.h"
#include <iostream>

int main() {
    Window window;
    Wheel wheel;

    bool isActive = false;
    Input::startHook();

    while (true) {
        bool bothHeld = Input::altHeld() && Input::controlHeld();
        POINT cursor = Input::getCursor();
        MONITORINFO info = Input::getMonitor();

        if (!isActive && bothHeld) {
            isActive = true;
            window.show();
        }

        if (isActive && !bothHeld) {
            isActive = false;
            window.hide(wheel);
            continue;
        }

        int monitorWidth = info.rcMonitor.right - info.rcMonitor.left;
        int monitorHeight = info.rcMonitor.bottom - info.rcMonitor.top;
        int x = info.rcMonitor.left + ((monitorWidth + 1) / 2);
        int y = info.rcMonitor.top + ((monitorHeight + 1) / 2);

        window.setPosition(
            x, y
        );

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                break;
            }
        }

        window.update(cursor);

        window.render();
        SDL_Delay(10);
    }

    Input::stopHook();
    return 0;
}
