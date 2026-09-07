#include <iostream>
#include "Window.h"

Window::Window() {
    width = (buttonCount * buttonRadius * 2) + ((buttonCount - 1) * buttonOffset) + buttonCount;
    height = buttonRadius * 2;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cout << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return;
    }

    window = SDL_CreateWindow("quickwheel", width, height, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!window) {
        std::cout << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return;
    }

    std::cout << "Window Created" << std::endl;

    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cout << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        return;
    }

    std::cout << "Renderer Created" << std::endl;
}

Window::~Window() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Window::show() {
    SDL_ShowWindow(window);
}

void Window::hide(Wheel wheel) {
    SDL_HideWindow(window);

    wheel.PerformAction(selectedAction);
}

void Window::setPosition(int x, int y) {
    int xOffset = width / 2;
    int yOffset = height / 2;

    SDL_SetWindowPosition(window, x - xOffset, y - yOffset);
}

void Window::updateSelectedAction(POINT cursor) {
    int cx = cursor.x;
    int cy = cursor.y;

    int windowX, windowY;
    SDL_GetWindowPosition(window, &windowX, &windowY);

    bool triggered = false;
    for (int i = 0; i < buttonCount; i++) {
        int bx = (i * buttonRadius * 2) + (i * buttonOffset) + windowX - (buttonOffset / 2);
        if (cx >= bx) {
            selectedAction = actions[i];
            triggered = true;
        }
    }

    if (!triggered) {
        selectedAction = actions[0];
    }
}

void Window::update(POINT cursor) {
    updateSelectedAction(cursor);
}

void Window::drawCircle(float radius, float x, float y) {
    for (float ry = -radius; ry <= radius; ry++) {
        float width = sqrtf(radius * radius - ry * ry);

        SDL_RenderLine(
            renderer,
            x - width,
            ry + y,
            x + width,
            ry + y
        );
    }
}

void Window::render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);


    for (int i = 0; i < buttonCount; i++) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        
        if (actions[i] == selectedAction) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        }

        int x = (i * 2 * buttonRadius) + (i * buttonOffset) + buttonRadius;
        drawCircle(buttonRadius, x, buttonRadius);
    }

    SDL_RenderPresent(renderer);
}
