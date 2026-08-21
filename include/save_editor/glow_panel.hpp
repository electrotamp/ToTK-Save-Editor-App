#pragma once

#include <borealis/core/box.hpp>

namespace totk::save_editor {

// A Box that draws a soft nanovg box-gradient halo around its rounded-rect
// bounds, plus a crisp inner stroke — the same "fake glow" technique the
// Zonai wisp overlay uses for its ribbon halos (nanovg has no native
// blur/box-shadow, so this is a gradient fill extending past the panel
// edges, faded to transparent). Purely decorative; children behave like any
// other Box.
class GlowPanel : public brls::Box {
public:
    GlowPanel();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

    void setGlowColor(NVGcolor color) { glowColor_ = color; }
    void setGlowSpread(float spread) { glowSpread_ = spread; }
    void setPanelCornerRadius(float radius) { panelCornerRadius_ = radius; }
    void setStrokeColor(NVGcolor color) { strokeColor_ = color; }
    void setStrokeWidth(float width) { strokeWidth_ = width; }

private:
    // ~5% visibility, dark teal — a subtle edge halo, not a colored wash
    // over the panel. The crisp stroke below is what should actually read
    // as "glowing," this is just softening behind it.
    NVGcolor glowColor_ = nvgRGBA(0, 90, 80, 13);
    float glowSpread_ = 18.0f;
    float panelCornerRadius_ = 12.0f;
    NVGcolor strokeColor_ = nvgRGBA(0, 255, 204, 160);
    float strokeWidth_ = 2.0f;
};

}  // namespace totk::save_editor
