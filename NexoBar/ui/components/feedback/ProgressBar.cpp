#include "ProgressBar.h"

#include <algorithm>

void ProgressBar::draw() {
    if (!visible) return;
    DrawRectangleRounded(m_area, roundness, 8, track);
    Rectangle progress = m_area;
    progress.width *= std::clamp(value, 0.f, 1.f);
    if (progress.width > 0.f) DrawRectangleRounded(progress, roundness, 8, fill);
}
