#include "Wheel.h"
#include "Window.h"
#include "Input.h"
#include "Spotify.h"
#include <iostream>

int main() {
    SpotifyAuth auth;
    if (!auth.ensureAuthenticated()) {
        std::cerr << "Spotify authentication failed." << std::endl;
        return 1;
    }

    Window window;
    Wheel wheel;

    bool isActive = false;
    Input::startHook();

    while (true) {
        bool bothHeld = Input::altHeld() && Input::controlHeld();
        POINT cursor = Input::getCursor();
        MONITORINFO info = Input::getMonitor();
        window.setMonitor(&info);

        if (!isActive && bothHeld) {
            isActive = true;
            window.show();
        }

        if (isActive && !bothHeld) {
            isActive = false;
            window.hide(wheel);
            continue;
        }


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
