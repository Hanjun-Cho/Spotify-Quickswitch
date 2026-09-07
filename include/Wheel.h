#pragma once

enum class WheelAction {
    Previous,
    PlayPause,
    Next,
    Cancel
};

class Wheel {
    public:
        void PerformAction(WheelAction action);

    private:
        void Previous();
        void PlayPause();
        void Next();
        void Cancel();
};
