#pragma once

namespace Tray {
    // Starts the system-tray icon on a background thread. Safe to call more
    // than once; subsequent calls are ignored while already running.
    void start();

    // Removes the tray icon and joins the tray thread. Safe to call anytime.
    void stop();

    // Returns true once the user selected "Quit" from the tray menu.
    bool exitRequested();

    // Reflects/updates the "launch on sign in" autostart registry entry.
    bool autoStartEnabled();
    void setAutoStartEnabled(bool enable);
}
