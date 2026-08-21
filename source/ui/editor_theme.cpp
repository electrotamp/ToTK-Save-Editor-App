#include "ui/editor_theme.hpp"

#include <algorithm>
#include <cstdio>

#include <borealis/core/application.hpp>
#include <borealis/core/theme.hpp>
#include <nlohmann/json.hpp>

#include "ui/zonai_wisp_settings.hpp"
#include "util/file_text.hpp"
#include "util/resource_loader.hpp"
#include "util/totk_log.hpp"

namespace totk::ui {

namespace {

NVGcolor defaultAccentColor() {
    return nvgRGB(0, 255, 204);
}

NVGcolor defaultFocusColor() {
    return nvgRGB(25, 138, 198);
}

NVGcolor mixColor(NVGcolor a, NVGcolor b, float t) {
    return nvgLerpRGBA(a, b, t);
}

NVGcolor lightenColor(NVGcolor color, float amount) {
    return mixColor(color, nvgRGB(255, 255, 255), amount);
}

NVGcolor parseColorJson(const nlohmann::json& value, NVGcolor fallback) {
    if (value.is_string()) {
        const std::string text = value.get<std::string>();
        if (text.size() >= 7 && text[0] == '#') {
            unsigned r = 0, g = 0, b = 0, a = 255;
            if (text.size() >= 7) std::sscanf(text.c_str() + 1, "%2x%2x%2x", &r, &g, &b);
            if (text.size() >= 9) std::sscanf(text.c_str() + 1, "%2x%2x%2x%2x", &r, &g, &b, &a);
            return nvgRGBA(static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                           static_cast<unsigned char>(b), static_cast<unsigned char>(a));
        }
    }
    if (value.is_array()) {
        const int r = value.size() > 0 ? value[0].get<int>() : 255;
        const int g = value.size() > 1 ? value[1].get<int>() : 255;
        const int b = value.size() > 2 ? value[2].get<int>() : 255;
        const int a = value.size() > 3 ? value[3].get<int>() : 255;
        return nvgRGBA(static_cast<unsigned char>(std::clamp(r, 0, 255)),
                       static_cast<unsigned char>(std::clamp(g, 0, 255)),
                       static_cast<unsigned char>(std::clamp(b, 0, 255)),
                       static_cast<unsigned char>(std::clamp(a, 0, 255)));
    }
    return fallback;
}

void buildUiColors(NVGcolor accent, NVGcolor focus, std::unordered_map<std::string, NVGcolor>& colors) {
    // Keep the complete Borealis focus treatment tied to the theme editor's
    // focus color.  Mixing the second outline color and cursor tint from the
    // accent made the selected focus color appear to have little or no effect.
    const NVGcolor highlight2 = lightenColor(focus, 0.42f);
    const NVGcolor buttonPrimary = mixColor(accent, nvgRGB(0, 0, 0), 0.12f);

    colors["brls/accent"] = accent;
    colors["brls/highlight/color1"] = focus;
    colors["brls/highlight/color2"] = highlight2;
    colors["brls/highlight/background"] = nvgRGBA(31, 34, 39, 255);
    colors["brls/sidebar/active_item"] = accent;
    colors["brls/list/listItem_value_color"] = accent;
    colors["brls/button/primary_enabled_background"] = buttonPrimary;
    colors["brls/button/highlight_enabled_text"] = accent;
    colors["brls/button/highlight_disabled_text"] = accent;
    colors["brls/slider/line_filled"] = buttonPrimary;
    NVGcolor clickPulse = focus;
    clickPulse.a = 38.0f / 255.0f;
    colors["brls/click_pulse"] = clickPulse;
    colors["brls/cursor/tint"] = focus;
}

NVGcolor readUiColor(const nlohmann::json& ui, const char* primaryKey, const char* colorsKey,
                     NVGcolor fallback) {
    if (ui.contains(primaryKey)) {
        return parseColorJson(ui[primaryKey], fallback);
    }
    if (ui.contains("colors") && ui["colors"].is_object() && ui["colors"].contains(colorsKey)) {
        return parseColorJson(ui["colors"][colorsKey], fallback);
    }
    return fallback;
}

}  // namespace

EditorTheme& EditorTheme::instance() {
    static EditorTheme theme;
    return theme;
}

void EditorTheme::setWispReloadHandler(std::function<void()> handler) {
    wispReloadHandler_ = std::move(handler);
}

bool EditorTheme::loadStartup() {
    try {
        const auto editorTheme = totk::loadResourceText("data/editor_theme.json");
        if (!editorTheme.empty() && loadFromText(editorTheme, "romfs:/data/editor_theme.json")) {
            apply();
            return true;
        }
    } catch (...) {
    }

    const std::string active = loadFileText(kActiveThemePath);
    if (!active.empty() && loadFromText(active, kActiveThemePath)) {
        apply();
        return true;
    }

    if (ZonaiWispSettings::instance().loadFromRomfs()) {
        name_ = "Bundled wisps";
        sourcePath_ = "romfs:/data/zonai_wisps.json";
        loaded_ = true;
        setThemeColors(defaultAccentColor(), defaultFocusColor());
        apply();
        return true;
    }

    return false;
}

bool EditorTheme::loadFromFile(const std::string& path) {
    const auto text = loadFileText(path);
    if (text.empty()) return false;
    if (!loadFromText(text, path)) return false;
    persistActiveCopy(text);
    apply();
    return true;
}

bool EditorTheme::loadFromText(const std::string& jsonText, const std::string& sourceLabel) {
    try {
        const auto root = nlohmann::json::parse(jsonText);
        return parseDocument(root, sourceLabel);
    } catch (const std::exception& ex) {
        TOTK_LOG("editor theme: parse failed — %s", ex.what());
        return false;
    } catch (...) {
        TOTK_LOG("editor theme: parse failed — unknown error");
        return false;
    }
}

bool EditorTheme::parseDocument(const nlohmann::json& root, const std::string& sourceLabel) {
    name_ = root.value("name", std::string("Custom"));
    sourcePath_ = sourceLabel;

    const nlohmann::json* wispsDoc = &root;
    if (root.contains("wisps")) wispsDoc = &root["wisps"];

    nlohmann::json wispsJson = *wispsDoc;
    if (root.contains("meta")) wispsJson["meta"] = root["meta"];

    if (!ZonaiWispSettings::instance().loadWispsDocument(wispsJson)) {
        return false;
    }

    NVGcolor accent = defaultAccentColor();
    NVGcolor focus = defaultFocusColor();
    const nlohmann::json* colorOverrides = nullptr;
    if (root.contains("ui") && root["ui"].is_object()) {
        const auto& ui = root["ui"];
        accent = readUiColor(ui, "accentColor", "brls/accent", accent);
        focus = readUiColor(ui, "focusColor", "brls/highlight/color1", focus);
        if (ui.contains("colors") && ui["colors"].is_object()) {
            colorOverrides = &ui["colors"];
        }
    }

    setThemeColors(accent, focus, colorOverrides);
    loaded_ = true;
    TOTK_LOG("editor theme: loaded '%s' from %s", name_.c_str(), sourcePath_.c_str());
    return true;
}

void EditorTheme::setThemeColors(NVGcolor accent, NVGcolor focus, const nlohmann::json* colorOverrides) {
    uiColors_.clear();
    buildUiColors(accent, focus, uiColors_);

    if (colorOverrides) {
        for (auto& [key, color] : uiColors_) {
            if (colorOverrides->contains(key)) {
                color = parseColorJson((*colorOverrides)[key], color);
            }
        }
    }
}

void EditorTheme::apply() {
    if (!loaded_) return;

    brls::Theme& dark = brls::Theme::getDarkTheme();
    for (const auto& [key, color] : uiColors_) {
        dark.addColor(key, color);
    }
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);

    if (wispReloadHandler_) wispReloadHandler_();
    refreshViews();
}

void EditorTheme::refreshViews() {
    for (brls::Activity* activity : brls::Application::getActivitiesStack()) {
        if (brls::View* view = activity->getContentView()) {
            view->invalidate();
        }
    }
}

void EditorTheme::persistActiveCopy(const std::string& jsonText) {
#if defined(__SWITCH__)
    if (ensureParentDirectory(kActiveThemePath)) {
        if (!writeFileText(kActiveThemePath, jsonText)) {
            TOTK_LOG("editor theme: failed to persist active theme");
        }
    }
#else
    (void)jsonText;
#endif
}

}  // namespace totk::ui
