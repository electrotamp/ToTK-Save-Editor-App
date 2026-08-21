#include "save_editor/glow_panel.hpp"

namespace totk::save_editor {

GlowPanel::GlowPanel() {
    this->setCornerRadius(panelCornerRadius_);
}

void GlowPanel::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                      brls::FrameContext* ctx) {
    // Halo: a box-gradient fading to transparent past the panel edges — nanovg
    // has no native blur/box-shadow, so this is the standard fake-glow trick
    // (same approach the Zonai wisp overlay uses for its ribbon halos). Fill a
    // rect larger than the panel; the panel's own background (drawn next,
    // via Box::draw) covers the interior, leaving only the spread outside the
    // edges visible.
    const NVGcolor transparent = nvgRGBA(glowColor_.r * 255, glowColor_.g * 255, glowColor_.b * 255, 0);
    NVGpaint glow = nvgBoxGradient(vg, x, y, width, height, panelCornerRadius_, glowSpread_, glowColor_, transparent);
    nvgBeginPath(vg);
    nvgRect(vg, x - glowSpread_ * 2, y - glowSpread_ * 2, width + glowSpread_ * 4, height + glowSpread_ * 4);
    nvgFillPaint(vg, glow);
    nvgFill(vg);

    Box::draw(vg, x, y, width, height, style, ctx);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + strokeWidth_ * 0.5f, y + strokeWidth_ * 0.5f, width - strokeWidth_,
                   height - strokeWidth_, panelCornerRadius_);
    nvgStrokeColor(vg, strokeColor_);
    nvgStrokeWidth(vg, strokeWidth_);
    nvgStroke(vg);
}

}  // namespace totk::save_editor
