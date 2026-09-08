#pragma once

#include "Spotify.h"

enum class WheelAction {
    Previous,
    PlayPause,
    Next,
    Cancel
};

class Wheel {
    public:
        Wheel(Spotify* spotify);
        void PerformAction(WheelAction action);

    private:
        Spotify* spotify;

        void Previous();
        void PlayPause();
        void Next();
        void Cancel();
};
