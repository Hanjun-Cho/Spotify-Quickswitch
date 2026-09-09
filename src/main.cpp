// Build as a GUI application so no console window is created. The entry point
// stays int main(), which mainCRTStartup calls before WinMain-style GUI setup.
#if defined(_MSC_VER)
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif

#include "Spotify.h"
#include "Wheel.h"
#include "Window.h"
#include "Input.h"
#include "SpotifyAuth.h"
#include "Tray.h"
#include <windows.h>

int main() {
    SpotifyAuth auth;
    if (!auth.ensureAuthenticated()) {
        MessageBoxW(nullptr, L"Spotify authentication failed. The app will close.",
                    L"Spotify Quick Wheel", MB_OK | MB_ICONERROR);
        return 1;
    }

    Spotify spotify(&auth);
    spotify.startPolling();
    spotify.getCurrentlyPlayingTrack();

    Window window;
    Wheel wheel(&spotify);

    bool isActive = false;
    Input::startHook();
    Tray::start();

    while (!Tray::exitRequested()) {
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
    spotify.stopPolling();
    Tray::stop();
    return 0;
}
