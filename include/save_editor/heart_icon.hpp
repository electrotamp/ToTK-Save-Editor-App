#pragma once

#include <borealis/core/view.hpp>

namespace totk::save_editor {

// A heart drawn from two circles (the top lobes) plus a triangle (the point)
// — the classic simple heart recipe, not traced from any reference art.
// Simple enough geometrically to be worth doing in code instead of sourcing
// icon assets; an ornate/stylized heart would not be.
class HeartIcon : public brls::View {
public:
    HeartIcon();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

    void setFilled(bool filled) { filled_ = filled; }

private:
    bool filled_ = true;
};

}  // namespace totk::save_editor
