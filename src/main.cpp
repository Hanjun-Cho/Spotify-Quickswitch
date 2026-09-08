#include "Spotify.h"
#include "Wheel.h"
#include "Window.h"
#include "Input.h"
#include "SpotifyAuth.h"
#include <iostream>

int main() {
    SpotifyAuth auth;
    if (!auth.ensureAuthenticated()) {
        std::cerr << "Spotify authentication failed." << std::endl;
        return 1;
    }
    Spotify spotify(&auth);
    spotify.startPolling();

    SpotifyTrack track = spotify.getCurrentlyPlayingTrack();

    Window window;
    Wheel wheel(&spotify);

    bool isActive = false;
    Input::startHook();

    while (true) {
        bool allHeld = Input::controlHeld() && Input::altHeld() && Input::qHeld();
        POINT cursor = Input::getCursor();
        MONITORINFO info = Input::getMonitor();
        window.setMonitor(&info);

        if (!isActive && allHeld) {
            isActive = true;
            window.show();
        }

        if (isActive && !allHeld) {
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
        window.updateAlbumArt(spotify.getCurrentTrack());

        window.render();
        SDL_Delay(10);
    }

    Input::stopHook();
    return 0;
}
