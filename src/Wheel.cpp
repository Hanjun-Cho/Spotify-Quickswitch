#include "Wheel.h"
#include <iostream>

void Wheel::PerformAction(WheelAction action) {
    switch (action) {
        case WheelAction::Previous:
            Previous();
            break;
        case WheelAction::PlayPause:
            PlayPause();
            break;
        case WheelAction::Next:
            Next();
            break;
        default:
            break;
    }
}

void Wheel::Previous() {
    std::cout << "previous" << std::endl;
}

void Wheel::PlayPause() {
    std::cout << "play pause" << std::endl;
}

void Wheel::Next() {
    std::cout << "next" << std::endl;
}
