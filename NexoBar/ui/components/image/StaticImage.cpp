#include "StaticImage.h"
#include <algorithm>

void StaticImage::draw() {
    if (!texture || texture->id == 0 || m_area.width <= 0 || m_area.height <= 0)return;
    const float sw = float(texture->width), sh = float(texture->height);
    Rectangle src{0, 0, sw, sh}, dst = m_area;
    if (fit != ImageFit::Stretch) {
        const float scale = fit == ImageFit::Contain ? std::min(m_area.width / sw, m_area.height / sh) : std::max(m_area.width / sw, m_area.height / sh);
        dst.width = sw * scale;
        dst.height = sh * scale;
        dst.x = m_area.x + (m_area.width - dst.width) * .5f;
        dst.y = m_area.y + (m_area.height - dst.height) * .5f;
        if (fit == ImageFit::Cover) {
            const float cw = m_area.width / scale, ch = m_area.height / scale;
            src = {float(texture->width - cw) * .5f, float(texture->height - ch) * .5f, cw, ch};
            dst = m_area;
        }
    }
    DrawTexturePro(*texture, src, dst, {0, 0}, 0, tint);
}
