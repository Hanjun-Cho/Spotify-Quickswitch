#pragma once

#include "Wheel.h"
#include <windows.h>
#include <iterator>
#include <SDL3/SDL.h>

class Window {
    public:
        Window();
        ~Window();

        void show();
        void hide(Wheel wheel);
        void setPosition(int x, int y);

        void updateSelectedAction(POINT cursor);
        void update(POINT cursor);

        void drawCircle(float radius, float x, float y);
        void render();

    private:
        int width;
        int height;

        int buttonRadius = 36;
        int buttonOffset = 60;

        WheelAction actions[4] = {
            WheelAction::Previous,
            WheelAction::PlayPause,
            WheelAction::Next,
            WheelAction::Cancel
        };

        WheelAction selectedAction = WheelAction::Previous;
        int buttonCount = std::size(actions);

        SDL_Window* window;
        SDL_Renderer* renderer;
};
