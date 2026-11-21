#include "settings_theme_tab.h"

#include "assets/lang_config.h"
#include "assets/lang_pixel.h"
#include "fontpack_lvgl.h"
#include "home_screen/home_screen.h"
#include "settings_common.h"
#include "settings_screen.h"
#include "screen_common.h"
#include "system_font.h"

#include <cstring>
#include <string>
#include <vector>

struct SettingsThemeOption {
    int style; // 卡片样式枚举
    const char* (*title)(); // 选项标题
    const char* (*current_text)(); // 当前选中文案
};

static const char* ThemeTitleWhite() {
    return Lang::Strings::SETTINGS_THEME_WHITE;
}
static const char* ThemeTitleGray() {
    return Lang::Strings::SETTINGS_THEME_GRAY;
}
static const char* ThemeTitleBorder() {
    return Lang::Strings::SETTINGS_THEME_BORDER;
}
static const char* ThemeTitleSlash() {
    return Lang::Strings::SETTINGS_THEME_SLASH;
}
static const char* ThemeTitlePixel() {
    return Lang::Strings::SETTINGS_THEME_PIXEL;
}
static const char* ThemeCurPixel() {
    return Lang::Strings::SETTINGS_THEME_CURRENT_PIXEL;
}
static const char* ThemeCurWhite() {
    return Lang::Strings::SETTINGS_THEME_CURRENT_WHITE;
}
static const char* ThemeCurGray() {
    return Lang::Strings::SETTINGS_THEME_CURRENT_GRAY;
}
static const char* ThemeCurBorder() {
    return Lang::Strings::SETTINGS_THEME_CURRENT_BORDER;
}
static const char* ThemeCurSlash() {
    return Lang::Strings::SETTINGS_THEME_CURRENT_SLASH;
}

static constexpr SettingsThemeOption kThemeOptions[] = {
    {HomeScreen::kCardStyleWhite, ThemeTitleWhite, ThemeCurWhite},
    {HomeScreen::kCardStyleGray, ThemeTitleGray, ThemeCurGray},
    {HomeScreen::kCardStyleBorder, ThemeTitleBorder, ThemeCurBorder},
    {HomeScreen::kCardStyleSlash, ThemeTitleSlash, ThemeCurSlash},
    {HomeScreen::kCardStylePixel, ThemeTitlePixel, ThemeCurPixel},
};
static constexpr int kThemeOptionCount = sizeof(kThemeOptions) / sizeof(kThemeOptions[0]);

struct SettingsThemeUi {
    lv_obj_t* btns[kThemeOptionCount] = {}; // 选项按钮
    lv_obj_t* current_lbl = nullptr; // 当前主题文案
    lv_obj_t* page = nullptr; // 本页容器；为空表示页面已拆除
    lv_obj_t* font_status = nullptr; // 系统字体状态文案
    std::vector<lv_obj_t*> font_btns; // [0] 内置 MiSans，其后为 SD 字体族
    std::vector<SystemFontEntry> fonts; // 与 font_btns[1..] 对应
    lv_timer_t* font_timer = nullptr; // 轮询扫描结果与加载状态；空闲时暂停
    bool fonts_listed = false;
    std::string shown_choice; // 上次刷新时的选择、状态与按钮数，未变化则不重绘
    int shown_state = -1;
    size_t shown_buttons = 0;
};

static SettingsThemeUi s_ui;

static const char* ThemeCurrentText(int style) {
    for (int i = 0; i < kThemeOptionCount; ++i) {
        if (kThemeOptions[i].style == style) {
            return kThemeOptions[i].current_text();
        }
    }
    return kThemeOptions[0].current_text();
}

static void RefreshThemeOptions() {
    const int style = HomeScreen::LoadCardStyle();
    for (int i = 0; i < kThemeOptionCount; ++i) {
        if (s_ui.btns[i] != nullptr) {
            SettingsStyleSelectable(s_ui.btns[i], lv_obj_get_child(s_ui.btns[i], 0),
                                    kThemeOptions[i].style == style);
        }
    }
    if (s_ui.current_lbl != nullptr) {
        lv_label_set_text(s_ui.current_lbl, ThemeCurrentText(style));
    }
}

static void OnThemeOptionClicked(lv_event_t* e) {
    const int style = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (HomeScreen::LoadCardStyle() == style) {
        return;
    }
    HomeScreen::SaveCardStyle(style);
    RefreshThemeOptions();
}

static std::string FontIdAt(size_t index) {
    return index == 0 ? std::string("builtin") : s_ui.fonts[index - 1].id;
}

static void SetLabelIfChanged(lv_obj_t* label, const char* text) {
    if (label != nullptr && std::strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

static void RefreshFontOptions() {
    const std::string choice = SystemFontChoiceId();
    const int state_now = static_cast<int>(SystemFontStatus());
    if (choice == s_ui.shown_choice && state_now == s_ui.shown_state && s_ui.font_btns.size() == s_ui.shown_buttons) {
        return; // 无变化：不重设样式，避免墨水屏反复刷新
    }
    s_ui.shown_choice = choice;
    s_ui.shown_state = state_now;
    s_ui.shown_buttons = s_ui.font_btns.size();
    for (size_t i = 0; i < s_ui.font_btns.size(); ++i) {
        SettingsStyleSelectable(s_ui.font_btns[i], lv_obj_get_child(s_ui.font_btns[i], 0), FontIdAt(i) == choice);
    }
    const char* status = "";
    switch (SystemFontStatus()) {
        case SystemFontState::kLoading: status = Lang::Strings::SETTINGS_SYSTEM_FONT_LOADING; break;
        case SystemFontState::kMissing: status = Lang::Strings::SETTINGS_SYSTEM_FONT_MISSING; break;
        case SystemFontState::kFailed: status = Lang::Strings::SYSTEM_FONT_LOAD_FAILED; break;
        default: break;
    }
    SetLabelIfChanged(s_ui.font_status, status);
}

static void OnFontApplied(bool ok) {
    if (s_ui.page == nullptr) return; // 设置页已离开
    if (ok) {
        SettingsScreen::ReloadAfterSystemFontChange();
        return;
    }
    ScreenShowToast(Lang::Strings::SYSTEM_FONT_LOAD_FAILED);
    RefreshFontOptions();
}

static void OnFontClicked(lv_event_t* e) {
    const size_t index = static_cast<size_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (index > s_ui.fonts.size()) return;
    const std::string id = FontIdAt(index);
    const SystemFontState state = SystemFontStatus();
    if (id == SystemFontChoiceId() && state != SystemFontState::kMissing && state != SystemFontState::kFailed) return;
    if (!SystemFontSelect(id.c_str(), OnFontApplied)) ScreenShowToast(Lang::Strings::SYSTEM_FONT_LOAD_FAILED);
    RefreshFontOptions();
    if (s_ui.font_timer != nullptr) lv_timer_resume(s_ui.font_timer); // 跟踪加载状态
}

static void AddFontButton(size_t index, const char* title) {
    lv_obj_t* btn = SettingsCreateSelectableOption(s_ui.page, title, OnFontClicked, static_cast<intptr_t>(index));
    s_ui.font_btns.push_back(btn);
}

static void OnFontTimer(lv_timer_t* /*timer*/) {
    if (!s_ui.fonts_listed && SystemFontScanDone(&s_ui.fonts)) {
        s_ui.fonts_listed = true;
        for (size_t i = 0; i < s_ui.fonts.size(); ++i) AddFontButton(i + 1, s_ui.fonts[i].label.c_str());
        if (s_ui.font_status != nullptr) lv_obj_move_to_index(s_ui.font_status, -1); // 状态文案保持在列表末尾
    }
    RefreshFontOptions();
    // 列表已显示且不在加载中：暂停轮询，下次点击时恢复
    if (s_ui.fonts_listed && SystemFontStatus() != SystemFontState::kLoading && s_ui.font_timer != nullptr) {
        lv_timer_pause(s_ui.font_timer);
    }
}

void SettingsThemeTab_Reset() {
    if (s_ui.font_timer != nullptr) lv_timer_delete(s_ui.font_timer);
    s_ui = {};
}

void SettingsThemeTab_Build(lv_obj_t* page) {
    SettingsThemeTab_Reset();
    s_ui.page = page;

    lv_obj_t* title = lv_label_create(page);
    lv_label_set_text(title, Lang::Strings::SETTINGS_THEME_TITLE);
    lv_obj_set_style_text_font(title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* hint = lv_label_create(page);
    lv_label_set_text(hint, Lang::Strings::SETTINGS_THEME_HINT);
    lv_obj_set_width(hint, lv_pct(100));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(hint, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(hint, lv_color_black(), 0);
    lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < kThemeOptionCount; ++i) {
        s_ui.btns[i] =
            SettingsCreateSelectableOption(page, kThemeOptions[i].title(), OnThemeOptionClicked,
                                           static_cast<intptr_t>(kThemeOptions[i].style));
    }

    s_ui.current_lbl = lv_label_create(page);
    lv_label_set_text(s_ui.current_lbl, "");
    lv_obj_set_style_text_font(s_ui.current_lbl, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(s_ui.current_lbl, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.current_lbl, LV_OBJ_FLAG_CLICKABLE);

    RefreshThemeOptions();

    // 系统字体：内置 MiSans + SD 卡字体族（后台扫描后追加）
    lv_obj_t* font_title = lv_label_create(page);
    lv_label_set_text(font_title, Lang::Strings::SETTINGS_SYSTEM_FONT_TITLE);
    lv_obj_set_style_text_font(font_title, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(font_title, lv_color_black(), 0);
    lv_obj_clear_flag(font_title, LV_OBJ_FLAG_CLICKABLE);
    AddFontButton(0, Lang::Strings::SETTINGS_SYSTEM_FONT_BUILTIN);
    s_ui.font_status = lv_label_create(page);
    lv_label_set_text(s_ui.font_status, "");
    lv_obj_set_width(s_ui.font_status, lv_pct(100));
    lv_label_set_long_mode(s_ui.font_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_ui.font_status, fontpack_lv_font_ui(), 0);
    lv_obj_set_style_text_color(s_ui.font_status, lv_color_black(), 0);
    lv_obj_clear_flag(s_ui.font_status, LV_OBJ_FLAG_CLICKABLE);
    SystemFontRequestScan();
    s_ui.font_timer = lv_timer_create(OnFontTimer, 500, nullptr);
    RefreshFontOptions();
}
