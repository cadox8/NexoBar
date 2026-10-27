#include "NavigationButton.h"

void NavigationButton::draw() {
    const Color previous = background;
    if (selected) background = selectedBackground;
    Button::draw();
    background = previous;
}
