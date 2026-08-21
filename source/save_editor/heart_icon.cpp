#include "save_editor/heart_icon.hpp"

namespace totk::save_editor {

HeartIcon::HeartIcon() {
    this->setFocusable(false);
}

void HeartIcon::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                      brls::FrameContext* ctx) {
    (void)style;
    (void)ctx;

    const float cx = x + width * 0.5f;
    const float r = width * 0.25f;
    const NVGcolor color = filled_ ? nvgRGB(224, 70, 90) : nvgRGBA(255, 255, 255, 30);

    nvgFillColor(vg, color);

    // Two lobes (circles), radius r, centered a radius apart so they meet at
    // the heart's vertical midline.
    nvgBeginPath(vg);
    nvgCircle(vg, cx - r, y + r, r);
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, cx + r, y + r, r);
    nvgFill(vg);

    // The point: a triangle spanning the lobes' shared horizontal diameter
    // down to a single apex, which reads as one continuous heart silhouette
    // when filled the same solid color as the lobes above.
    nvgBeginPath(vg);
    nvgMoveTo(vg, cx - 2.0f * r, y + r);
    nvgLineTo(vg, cx + 2.0f * r, y + r);
    nvgLineTo(vg, cx, y + height);
    nvgClosePath(vg);
    nvgFill(vg);
}

}  // namespace totk::save_editor
