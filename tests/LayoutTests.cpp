#include "ui/components/layout/Layout.h"

#include <gtest/gtest.h>

namespace {
class DrawAreaProbe final : public IComponent {
public:
    void update(float) override {}

    void draw() override {
        areaAtDraw = m_area;
        drawn = true;
    }

    Rectangle areaAtDraw{};
    bool drawn = false;
};
}

TEST(LayoutTests, PropagatesAreaBeforeFirstDraw) {
    Layout layout(LayoutAxis::Horizontal);
    const auto probe = std::make_shared<DrawAreaProbe>();
    layout.add(probe);
    layout.setArea({12.f, 24.f, 320.f, 180.f});

    // Deliberately draw without an update pass. This is the sequence used when
    // a screen transition occurs from inside the previous screen's update().
    layout.draw();

    ASSERT_TRUE(probe->drawn);
    EXPECT_FLOAT_EQ(probe->areaAtDraw.x, 12.f);
    EXPECT_FLOAT_EQ(probe->areaAtDraw.y, 24.f);
    EXPECT_FLOAT_EQ(probe->areaAtDraw.width, 320.f);
    EXPECT_FLOAT_EQ(probe->areaAtDraw.height, 180.f);
}
