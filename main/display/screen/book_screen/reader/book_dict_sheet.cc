#include "book_screen/reader/book_dict_sheet.h"

#include "assets/lang_reader.h"
#include "book_screen/book_font_runtime.h"
#include "book_screen/book_nav.h"
#include "book_screen/book_screen_priv.h"
#include "book_screen/book_text_util.h"
#include "book_screen/reader/book_reader_overlay.h"
#include "book_screen/reader/book_layout_worker.h"
#include "book_screen/reader/book_reader_prefs.h"
#include "reader/dictionary.h"
#include "reader/page_word_hit.h"
#include "sd_access.h"
#include "sd_paths.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <string>
#include <vector>

namespace {

constexpr char kTag[] = "book_dict";
constexpr uint32_t kDictTaskStack = 8 * 1024;

lv_obj_t* s_sheet = nullptr;   // 卡片根对象；随阅读屏删除时由 DELETE 事件清空
lv_obj_t* s_body = nullptr;    // 释义容器
uint32_t s_generation = 0;     // 每次打开/关闭递增，作废过期查询结果（LVGL 线程）
std::atomic<bool> s_visible{false};
std::atomic<bool> s_task_busy{false};

enum class LookupStatus : uint8_t { kOk, kNoDictionary, kSdUnavailable };

struct LookupJob {
    std::string word;
    uint32_t generation = 0;
    LookupStatus status = LookupStatus::kOk;
    std::vector<reader::DictEntry> entries;
};

lv_obj_t* AddLabel(lv_obj_t* parent, const lv_font_t* font, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, text);
    return label;
}

void ShowSheet(const std::string& word) {
    CloseDictSheet();
    auto& st = Book_State();
    lv_obj_t* scr = st.reader.read_scr;
    if (scr == nullptr || !lv_obj_is_valid(scr)) return;
    s_sheet = lv_obj_create(scr);
    lv_obj_remove_style_all(s_sheet);
    lv_obj_set_size(s_sheet, LV_HOR_RES - kSheetSidePad * 2, LV_VER_RES / 2);
    lv_obj_align(s_sheet, LV_ALIGN_BOTTOM_MID, 0, -kSheetSidePad);
    lv_obj_set_style_bg_color(s_sheet, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_sheet, kSheetBorderW, 0);
    lv_obj_set_style_border_color(s_sheet, lv_color_black(), 0);
    lv_obj_set_style_radius(s_sheet, kSheetRadius, 0);
    lv_obj_set_style_pad_all(s_sheet, kSheetInnerPad, 0);
    lv_obj_set_style_pad_row(s_sheet, kSheetGap, 0);
    lv_obj_set_flex_flow(s_sheet, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_sheet, LV_DIR_VER);
    lv_obj_add_flag(s_sheet, LV_OBJ_FLAG_CLICKABLE);
    // 点在卡片上不穿透到正文分区
    lv_obj_add_event_cb(s_sheet, [](lv_event_t* ev) { lv_event_stop_bubbling(ev); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(s_sheet, [](lv_event_t* ev) {
        if (lv_event_get_target(ev) == s_sheet) {
            s_sheet = nullptr;
            s_body = nullptr;
            s_visible.store(false);
        }
    }, LV_EVENT_DELETE, nullptr);
    AddLabel(s_sheet, Book_ListFont(), word.c_str());
    s_body = lv_obj_create(s_sheet);
    lv_obj_remove_style_all(s_body);
    lv_obj_set_size(s_body, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_body, kSheetGap, 0);
    lv_obj_clear_flag(s_body, LV_OBJ_FLAG_CLICKABLE);
    AddLabel(s_body, Book_ItemFont(), Lang::Strings::BOOK_DICT_LOOKING_UP);
    s_visible.store(true);
}

void ApplyResult(void* user_data) {
    LookupJob* job = static_cast<LookupJob*>(user_data);
    if (s_sheet != nullptr && s_body != nullptr && job->generation == s_generation) {
        lv_obj_clean(s_body);
        if (job->status == LookupStatus::kSdUnavailable) {
            AddLabel(s_body, Book_ItemFont(), Lang::Strings::BOOK_DICT_SD_UNAVAILABLE);
        } else if (job->status == LookupStatus::kNoDictionary) {
            AddLabel(s_body, Book_ItemFont(), Lang::Strings::BOOK_DICT_NONE);
        } else if (job->entries.empty()) {
            lv_label_set_text_fmt(AddLabel(s_body, Book_ItemFont(), ""), Lang::Strings::BOOK_DICT_NOT_FOUND,
                                  job->word.c_str());
        } else {
            for (const auto& entry : job->entries) {
                std::string title = entry.bookname;
                if (entry.headword != job->word) title += " \xC2\xB7 " + entry.headword; // " · "
                AddLabel(s_body, Book_ListFont(), title.c_str());
                AddLabel(s_body, Book_ItemFont(), entry.definition.c_str());
            }
        }
    }
    delete job;
}

void LookupTask(void* arg) {
    LookupJob* job = static_cast<LookupJob*>(arg);
    {
        sd_access::Transaction tx;
        if (!tx) {
            job->status = LookupStatus::kSdUnavailable;
        } else {
            const auto dicts = reader::ScanDictionaries(SD_MOUNT_POINT "/dict");
            if (dicts.empty()) job->status = LookupStatus::kNoDictionary;
            else job->entries = reader::DictLookup(dicts, job->word);
        }
    } // 发回 UI 前已关闭全部文件并释放 SD 许可
    if (!BookLvAsync(ApplyResult, job)) delete job;
    s_task_busy.store(false);
    vTaskDeleteWithCaps(nullptr);
}

bool FindWordAt(const lv_point_t& pt, std::string& word) {
    auto& st = Book_State();
    lv_obj_t* image = st.reader.text_raster_image;
    if (image == nullptr || !lv_obj_is_valid(image) || !st.reader.session) return false;
    const auto* raster = static_cast<const reader::RasterImage*>(lv_obj_get_user_data(image));
    const reader::Page* page = st.reader.session->CurrentPageData();
    const lv_font_t* font = BookFont();
    if (raster == nullptr || raster->empty() || page == nullptr || font == nullptr) return false;
    lv_area_t area;
    lv_obj_get_coords(image, &area);
    reader::TextStyle style = st.reader.session->Style();
    style.justify = BookReaderPrefsJustify() != 0; // 与 book_page_work 的排版输入一致
    reader::PageWordHit hit;
    if (!reader::PageWordAt(*page, font, st.reader.session->LineSpacingPercent(), st.reader.session->ParaGap(),
                            raster->width, st.reader.viewport_h, style, pt.x - area.x1, pt.y - area.y1, hit)) {
        return false;
    }
    word = std::move(hit.word);
    return true;
}

}  // namespace

bool DictSheetVisible() { return s_visible.load(); }

void CloseDictSheet() {
    ++s_generation;
    if (s_sheet != nullptr && lv_obj_is_valid(s_sheet)) lv_obj_delete(s_sheet);
    s_sheet = nullptr;
    s_body = nullptr;
    s_visible.store(false);
}

namespace {

// 长按处有单词且可查询时打开卡片并启动查询；否则返回 false
bool TryStartLookup(lv_event_t* e) {
    auto& st = Book_State();
    if (st.reader.read_chrome != BookUiState::ReadChrome::kReading) return false;
    if (st.reader.opening.load() || !st.reader.session || !st.reader.session->IsOpen()) return false;
    if (st.reader.session->VerticalWriting() || st.reader.session->IsPageImagesMode()) return false;
    // 页面准备任务也在用正文字体，忙时不取词，避免并发访问字形缓存
    if (ReaderWorkersBusy() || ReaderPageWorkPending() || s_task_busy.load()) return false;
    lv_point_t pt = {};
    std::string word;
    if (!ReadEventPoint(e, &pt) || !FindWordAt(pt, word)) return false;

    ShowSheet(word);
    if (s_sheet == nullptr) return false;
    auto* job = new LookupJob;
    job->word = word;
    job->generation = s_generation;
    s_task_busy.store(true);
    if (xTaskCreateWithCaps(LookupTask, "book_dict", kDictTaskStack, job, tskIDLE_PRIORITY + 1, nullptr,
                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        ESP_LOGW(kTag, "lookup task creation failed");
        s_task_busy.store(false);
        job->status = LookupStatus::kSdUnavailable;
        ApplyResult(job);
    }
    return true;
}

}  // namespace

void OnReadContentLongPressed(lv_event_t* e) {
    auto& st = Book_State();
    if (st.reader.content == nullptr || lv_event_get_target(e) != st.reader.content) return;
    if (DictSheetVisible()) {
        CloseDictSheet();  // 慢按也能收起卡片
        return;
    }
    // 没取到词：按普通点按处理（长按后不再有 SHORT_CLICKED），保持慢按翻页/菜单
    if (!TryStartLookup(e)) OnReadContentClicked(e);
}
