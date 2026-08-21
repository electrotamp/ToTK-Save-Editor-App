#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <nanovg.h>
#include <nlohmann/json.hpp>

namespace totk::ui {

class EditorTheme {
public:
    static EditorTheme& instance();

    bool loadStartup();
    bool loadFromFile(const std::string& path);
    bool loadFromText(const std::string& jsonText, const std::string& sourceLabel);

    void apply();
    void refreshViews();

    void setWispReloadHandler(std::function<void()> handler);

    bool loaded() const { return loaded_; }
    const std::string& name() const { return name_; }
    const std::string& sourcePath() const { return sourcePath_; }

    // Start at the SD root so importing a theme never creates or requires an
    // app-specific themes folder. Users can keep theme files wherever they
    // already organize Switch content.
    static constexpr const char* kImportDirectory = "sdmc:/";
    static constexpr const char* kActiveThemePath = "sdmc:/switch/totk-save-editor/active_editor_theme.json";

private:
    bool parseDocument(const nlohmann::json& root, const std::string& sourceLabel);
    void setThemeColors(NVGcolor accent, NVGcolor focus, const nlohmann::json* colorOverrides = nullptr);
    void persistActiveCopy(const std::string& jsonText);

    bool loaded_ = false;
    std::string name_ = "Default";
    std::string sourcePath_;
    std::unordered_map<std::string, NVGcolor> uiColors_;
    std::function<void()> wispReloadHandler_;
};

}  // namespace totk::ui
