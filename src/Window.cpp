#include <iostream>
#include <vector>
#include <cmath>
#include "Window.h"

Window::Window() {
    width = (buttonCount * buttonRadius * 2) + ((buttonCount - 1) * buttonOffset) + buttonCount;
    height = 200;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cout << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return;
    }

    window = SDL_CreateWindow("quickwheel", width, height, 
            SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_TRANSPARENT);
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

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    std::cout << "Renderer Created" << std::endl;

    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    SetWindowLongPtr(hwnd, GWL_EXSTYLE, style | WS_EX_TOPMOST | WS_EX_NOACTIVATE);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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

void Window::setMonitor(MONITORINFO* monitor) {
    width = monitor->rcMonitor.right - monitor->rcMonitor.left;
    height = monitor->rcMonitor.bottom - monitor->rcMonitor.top;
    SDL_SetWindowSize(window, width, height);
    SDL_SetWindowPosition(window, monitor->rcMonitor.left, monitor->rcMonitor.top);
}

void Window::updateSelectedAction(POINT cursor) {
    int cx = cursor.x;
    int cy = cursor.y;

    int windowX, windowY;
    SDL_GetWindowPosition(window, &windowX, &windowY);

    bool triggered = false;
    for (int i = 0; i < buttonCount; i++) {
        int xOffset = (width / 2) - (buttonWidthTotal / 2);
        int bx = windowX + xOffset + ((i * buttonRadius * 2) + (i * buttonOffset) - (buttonOffset / 2));
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
    Uint8 cr, cg, cb, ca;
    SDL_GetRenderDrawColor(renderer, &cr, &cg, &cb, &ca);

    int top = (int)std::floor(y - radius);
    int bottom = (int)std::ceil(y + radius);

    for (int yy = top; yy <= bottom; yy++) {
        float relY = (float)yy - y;
        float hwSq = radius * radius - relY * relY;
        if (hwSq <= 0.0f) {
            continue;
        }

        float hw = std::sqrt(hwSq);
        float xl = x - hw;
        float xr = x + hw;

        int colI0 = (int)std::ceil(xl);
        int colI1 = (int)std::floor(xr - 1.0f);

        if (colI1 >= colI0) {
            SDL_SetRenderDrawColor(renderer, cr, cg, cb, ca);
            SDL_RenderLine(renderer, colI0, yy, colI1 + 1, yy);
        }

        float leftCov = colI0 - xl;
        if (leftCov > 0.0f) {
            SDL_SetRenderDrawColor(renderer, cr, cg, cb, (Uint8)(ca * leftCov));
            SDL_RenderLine(renderer, colI0 - 1, yy, colI0, yy);
        }

        int colR = colI1 + 1;
        float rightCov = xr - colR;
        if (rightCov > 0.0f) {
            SDL_SetRenderDrawColor(renderer, cr, cg, cb, (Uint8)(ca * rightCov));
            SDL_RenderLine(renderer, colR, yy, colR + 1, yy);
        }
    }
}

void Window::drawBorderedCircle(float border, float radius, float x, float y, SDL_Color outerColor, SDL_Color innerColor) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 230);
    drawCircle(radius, x + 3, y + 3);

    SDL_SetRenderDrawColor(renderer, outerColor.r, outerColor.g, outerColor.b, outerColor.a);
    drawCircle(radius, x, y);

    SDL_SetRenderDrawColor(renderer, innerColor.r, innerColor.g, innerColor.b, innerColor.a);
    drawCircle(radius - border, x, y);
}

void Window::render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 130);
    SDL_RenderClear(renderer);

    int xOffset = (width / 2) -  (buttonWidthTotal / 2);
    int y = (height / 2);

    for (int i = 0; i < buttonCount; i++) {
        int x = xOffset + ((i * 2 * buttonRadius) + (i * buttonOffset) + buttonRadius);

        if (actions[i] == selectedAction) {
            drawBorderedCircle(1, buttonRadius, x, y, selectedColor, borderColor);
        }
        else {
            drawBorderedCircle(1, buttonRadius, x, y, defaultColor, defaultBorderColor);
        }
    }

    SDL_RenderPresent(renderer);
}
