#include "ui/zonai_wisp_overlay.hpp"

#include <algorithm>
#include <cmath>

#include <borealis/core/application.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/hint.hpp>
#include <borealis/views/label.hpp>

#include "ui/zonai_wisp_session.hpp"
#include "ui/zonai_wisp_settings.hpp"
#include "util/totk_log.hpp"

namespace totk::ui {

namespace {

void enableAButtonTouch(brls::View* view) {
    if (!view) return;
    if (auto* hints = dynamic_cast<brls::Hints*>(view)) {
        // Borealis deliberately excludes A from footer-hint touch by
        // default, even though B/L/R are tappable. Editor screens already
        // make their visible A targets touchable; enable the matching footer
        // hint too so tapping the on-screen A performs the advertised action.
        hints->setAllowAButtonTouch(true);
    }
    if (auto* box = dynamic_cast<brls::Box*>(view)) {
        for (brls::View* child : box->getChildren()) enableAButtonTouch(child);
    }
}

constexpr float kScreenCullPad = 48.0f;

float smoothstep(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
}

WispPoint2 catmullRom(const WispPoint2& p0, const WispPoint2& p1, const WispPoint2& p2, const WispPoint2& p3, float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    return WispPoint2{
        0.5f * (2.0f * p1.x + (-p0.x + p2.x) * t + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
        0.5f * (2.0f * p1.y + (-p0.y + p2.y) * t + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3),
    };
}

void sampleSmoothPathInto(const std::vector<float>& xs, const std::vector<float>& ys, int pointCount, int subdiv,
                          std::vector<WispPoint2>& out, std::vector<float>& px, std::vector<float>& py) {
    out.clear();
    if (pointCount < 2) {
        if (!xs.empty()) out.push_back({xs[0], ys[0]});
        return;
    }

    const int subdivClamped = std::max(1, subdiv);
    out.reserve(static_cast<size_t>((pointCount - 1) * subdivClamped + 1));

    px.resize(static_cast<size_t>(pointCount));
    py.resize(static_cast<size_t>(pointCount));
    for (int i = 0; i < pointCount; ++i) {
        px[static_cast<size_t>(i)] = xs[static_cast<size_t>(pointCount - 1 - i)];
        py[static_cast<size_t>(i)] = ys[static_cast<size_t>(pointCount - 1 - i)];
    }

    out.push_back({px[0], py[0]});
    for (int i = 0; i < pointCount - 1; ++i) {
        const WispPoint2 p0 = i > 0 ? WispPoint2{px[static_cast<size_t>(i - 1)], py[static_cast<size_t>(i - 1)]}
                                    : WispPoint2{px[static_cast<size_t>(i)], py[static_cast<size_t>(i)]};
        const WispPoint2 p1{px[static_cast<size_t>(i)], py[static_cast<size_t>(i)]};
        const WispPoint2 p2{px[static_cast<size_t>(i + 1)], py[static_cast<size_t>(i + 1)]};
        const WispPoint2 p3 = i + 2 < pointCount
                                  ? WispPoint2{px[static_cast<size_t>(i + 2)], py[static_cast<size_t>(i + 2)]}
                                  : WispPoint2{px[static_cast<size_t>(i + 1)], py[static_cast<size_t>(i + 1)]};
        for (int s = 1; s <= subdivClamped; ++s) {
            out.push_back(catmullRom(p0, p1, p2, p3, static_cast<float>(s) / static_cast<float>(subdivClamped)));
        }
    }
}

unsigned char alphaByte(float alpha) {
    return static_cast<unsigned char>(std::max(0.0f, std::min(1.0f, alpha)) * 255.0f);
}

}  // namespace

ZonaiWispOverlay::ZonaiWispOverlay() {
    this->setFocusable(false);
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);
    this->setHideHighlightBackground(true);
}

void ZonaiWispOverlay::willAppear(bool resetState) {
    brls::View::willAppear(resetState);
    ZonaiWispSession::instance().ensureRunning();
}

bool ZonaiWispOverlay::isOnScreen(float px, float py, float x, float y, float width, float height) const {
    return px >= x - kScreenCullPad && px <= x + width + kScreenCullPad && py >= y - kScreenCullPad &&
           py <= y + height + kScreenCullPad;
}

void ZonaiWispOverlay::buildScreenRibbon(const std::vector<float>& xs, const std::vector<float>& ys, int pointCount,
                                         int subdiv, float baseAlpha, float headWidth, float x, float y, float width,
                                         float height, const WispProfile& profile) {
    sampleSmoothPathInto(xs, ys, pointCount, subdiv, sampledPath_, scratchPx_, scratchPy_);
    if (sampledPath_.size() < 2) {
        screenRibbon_.clear();
        return;
    }

    const float scaleX = width / WispSimulator::kSimWidth;
    const float scaleY = height / WispSimulator::kSimHeight;
    const int last = static_cast<int>(sampledPath_.size()) - 1;

    screenRibbon_.resize(sampledPath_.size());
    for (int i = 0; i <= last; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(last);
        const float taper = smoothstep(t);
        ScreenPoint& point = screenRibbon_[static_cast<size_t>(i)];
        point.x = x + sampledPath_[static_cast<size_t>(i)].x * scaleX;
        point.y = y + sampledPath_[static_cast<size_t>(i)].y * scaleY;
        point.width = headWidth * (profile.tailWidthMin + (1.0f - profile.tailWidthMin) * taper);
        point.alpha = baseAlpha * (profile.tailAlphaMin + (1.0f - profile.tailAlphaMin) * std::pow(taper, profile.tailAlphaPower));
    }
}

void ZonaiWispOverlay::drawWisp(NVGcontext* vg, float x, float y, float width, float height, const WispEntity& wisp,
                                const WispProfile& profile, WispSimulator& simulator) {
    const float baseAlpha = simulator.wispAlpha(wisp, profile);
    if (baseAlpha <= 0.002f) return;

    const std::vector<float>& xs = wisp.hasSmooth ? wisp.sx : wisp.x;
    const std::vector<float>& ys = wisp.hasSmooth ? wisp.sy : wisp.y;
    buildScreenRibbon(xs, ys, wisp.pointCount, profile.curveSubdiv, baseAlpha, wisp.width, x, y, width, height, profile);
    if (screenRibbon_.size() < 2) return;

    const int last = static_cast<int>(screenRibbon_.size()) - 1;

    // A ribbon frequently has its head beyond an edge while a visible part
    // of its tail is still crossing the screen.  Culling only from the head
    // made that whole visible tail vanish on the exact frame the head left.
    // Keep drawing until every sampled point has moved out of the padded
    // viewport; NanoVG/Borealis performs the final per-segment clipping.
    bool anyRibbonPointOnScreen = false;
    for (const ScreenPoint& point : screenRibbon_) {
        if (isOnScreen(point.x, point.y, x, y, width, height)) {
            anyRibbonPointOnScreen = true;
            break;
        }
    }
    if (!anyRibbonPointOnScreen) return;

    const int rgbR = profile.colorR;
    const int rgbG = profile.colorG;
    const int rgbB = profile.colorB;
    const bool drawHalo = profile.haloAlpha > 0.001f;
    const bool drawWhite = profile.whiteHighlight > 0.001f;

    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);

    for (int i = 0; i < last; ++i) {
        const ScreenPoint& p0 = screenRibbon_[static_cast<size_t>(i)];
        const ScreenPoint& p1 = screenRibbon_[static_cast<size_t>(i + 1)];
        const float t = static_cast<float>(i) / static_cast<float>(last);
        const float segAlpha = p0.alpha;
        const float segW = p0.width;

        if (drawHalo) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, p0.x, p0.y);
            nvgLineTo(vg, p1.x, p1.y);
            nvgStrokeColor(vg, nvgRGBA(rgbR, rgbG, rgbB, alphaByte(segAlpha * profile.haloAlpha)));
            nvgStrokeWidth(vg, segW * 1.1f * (1.0f + profile.haloBlur * 0.35f));
            nvgStroke(vg);
        }

        nvgBeginPath(vg);
        nvgMoveTo(vg, p0.x, p0.y);
        nvgLineTo(vg, p1.x, p1.y);
        nvgStrokeColor(vg, nvgRGBA(rgbR, rgbG, rgbB, alphaByte(segAlpha * profile.coreAlpha)));
        nvgStrokeWidth(vg, segW * 0.58f);
        nvgStroke(vg);

        if (drawWhite && t > profile.whiteFrom) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, p0.x, p0.y);
            nvgLineTo(vg, p1.x, p1.y);
            nvgStrokeColor(vg, nvgRGBA(255, 255, 255, alphaByte(segAlpha * profile.whiteHighlight)));
            nvgStrokeWidth(vg, std::max(0.35f, segW * 0.14f));
            nvgStroke(vg);
        }
    }
}

void ZonaiWispOverlay::drawFragment(NVGcontext* vg, float x, float y, float width, float height,
                                    const WispFragment& fragment, const WispProfile& profile,
                                    WispSimulator& simulator) {
    const float alpha = simulator.fragmentAlpha(fragment, profile);
    if (alpha <= 0.002f) return;

    const float scaleX = width / WispSimulator::kSimWidth;
    const float scaleY = height / WispSimulator::kSimHeight;
    const float px = x + fragment.x * scaleX;
    const float py = y + fragment.y * scaleY;
    if (!isOnScreen(px, py, x, y, width, height)) return;

    nvgBeginPath(vg);
    nvgCircle(vg, px, py, fragment.size * 0.35f * ((scaleX + scaleY) * 0.5f));
    nvgFillColor(vg, nvgRGBA(profile.colorR, profile.colorG, profile.colorB, alphaByte(alpha * 0.9f)));
    nvgFill(vg);
}

void ZonaiWispOverlay::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style /*style*/,
                            brls::FrameContext* /*ctx*/) {
    if (width <= 0.0f || height <= 0.0f) return;

    ZonaiWispSession& session = ZonaiWispSession::instance();
    if (!session.isRunning()) return;

    WispSimulator& simulator = session.simulator();

    const auto& settings = ZonaiWispSettings::instance();
    const auto& profiles = settings.profiles();

    for (const WispFragment& fragment : simulator.fragments()) {
        if (fragment.profileIndex >= profiles.size()) continue;
        drawFragment(vg, x, y, width, height, fragment, profiles[fragment.profileIndex], simulator);
    }

    for (const WispEntity& wisp : simulator.wisps()) {
        if (!wisp.alive) continue;
        if (wisp.profileIndex >= profiles.size()) continue;
        drawWisp(vg, x, y, width, height, wisp, profiles[wisp.profileIndex], simulator);
    }
}

ZonaiWispOverlay* attachWispOverlay(brls::Box* parent) {
    if (!parent || !ZonaiWispSettings::instance().loaded()) return nullptr;

    auto* overlay = new ZonaiWispOverlay();
    overlay->setPositionType(brls::PositionType::ABSOLUTE);
    overlay->setPositionTop(0);
    overlay->setPositionLeft(0);
    overlay->setWidthPercentage(100);
    overlay->setHeightPercentage(100);
    overlay->setFocusable(false);
    parent->addView(overlay, 0);
    ZonaiWispSession::instance().ensureRunning();
    return overlay;
}

EditorWallpaper::EditorWallpaper() {
    this->setFocusable(false);
}

void EditorWallpaper::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style /*style*/,
                           brls::FrameContext* /*ctx*/) {
    if (!loadAttempted_) {
        loadAttempted_ = true;
        // Same raw-nvg load IconAtlas::ensureSheetResident uses for its sheet
        // textures — deliberately not brls::Image::setImageFromRes, see the
        // header comment.
        const std::string filePath = std::string(BRLS_RESOURCES) + "assets/wallpaper/editor_background.jpg";
        texture_ = nvgCreateImage(vg, filePath.c_str(), 0);
        int texW = 0, texH = 0;
        if (texture_ > 0) nvgImageSize(vg, texture_, &texW, &texH);
        TOTK_LOG("wallpaper: draw box=(%.1f,%.1f,%.1f,%.1f) tex=%d texWH=(%d,%d) windowWH=(%u,%u) contentWH=(%.1f,%.1f)",
                 x, y, width, height, texture_, texW, texH, brls::Application::windowWidth,
                 brls::Application::windowHeight, brls::Application::contentWidth, brls::Application::contentHeight);
    }
    if (texture_ <= 0 || width <= 0.0f || height <= 0.0f) return;

    // x/y/width/height here are the view's actual on-screen box, computed by
    // the same ordinary (non-measure-function) Yoga sizing ZonaiWispOverlay
    // relies on — the image pattern is built to exactly match it, so the
    // draw always fills the real box regardless of the source JPG's own
    // pixel dimensions.
    NVGpaint paint = nvgImagePattern(vg, x, y, width, height, 0.0f, texture_, 1.0f);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, height);
    nvgFillPaint(vg, paint);
    nvgFill(vg);
}

void attachEditorBackground(brls::Box* parent) {
    if (!parent) return;

    if (auto* frame = dynamic_cast<brls::AppletFrame*>(parent)) {
        enableAButtonTouch(frame->getFooter());
    }

    // Wisp first — attachWispOverlay always inserts at index 0, so at this
    // point it's temporarily the frontmost-drawn-first (i.e. backmost) child.
    attachWispOverlay(parent);

    // Wallpaper second, explicitly at index 0 too: this bumps whatever's
    // already there (the wisp overlay, if it was just added) to index 1, so
    // the wisps paint just above the wallpaper instead of being hidden under
    // it — see the header comment for the full ordering rationale.
    auto* wallpaper = new EditorWallpaper();
    wallpaper->setPositionType(brls::PositionType::ABSOLUTE);
    // Pinning all four edges instead of top/left + width%/height%: logging
    // confirmed the offset (top/left) and the percentage size were resolving
    // against two different boxes — position ignored the parent's own
    // padding (as absolute offsets normally do) while width%/height%
    // subtracted it, leaving the view's right/bottom edges short of the
    // parent's true bounds by exactly that padding. With all four edges
    // pinned to 0, Yoga derives the size from the (consistently-referenced)
    // offsets instead, so there's no percentage-vs-offset mismatch left to
    // fall short.
    wallpaper->setPositionTop(0);
    wallpaper->setPositionLeft(0);
    wallpaper->setPositionRight(0);
    wallpaper->setPositionBottom(0);
    wallpaper->setFocusable(false);
    parent->addView(wallpaper, 0);
}

void setCenteredHeaderTitle(brls::AppletFrame* frame, const std::string& text) {
    if (!frame) return;
    frame->setTitle("");

    // 28px matches AppletFrame's own title style (style.cpp,
    // brls/applet_frame/header_title_font_size) so this reads identically to
    // the label it replaces, just centered across the header instead of
    // left-aligned against it.
    auto* label = new brls::Label();
    label->setFontSize(28.0f);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setFocusable(false);
    label->setPositionType(brls::PositionType::ABSOLUTE);
    label->setPositionTop(0);
    label->setPositionLeft(0);
    label->setPositionRight(0);
    label->setPositionBottom(0);
    label->setText(text);
    frame->getHeader()->addView(label);
}

brls::View* ZonaiWispOverlay::create() {
    return new ZonaiWispOverlay();
}

}  // namespace totk::ui
