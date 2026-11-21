#include "system_font.h"

#include "application.h"
#include "epd_refresh.h"
#include "epdfont.h"
#include "fontpack_lvgl.h"
#include "screen_common.h"
#include "sd_access.h"
#include "sd_paths.h"
#include "settings.h"
#include "system_font_rules.h"
#include "ttf_runtime.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <new>

#include <dirent.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <sys/stat.h>

namespace {

constexpr const char* TAG = "SysFont";
constexpr const char* kNvsNs = "ui";
constexpr const char* kNvsKey = "sys_font";
constexpr uint32_t kWorkerStack = 12 * 1024; // 字形回调经 TTF 自有工作任务渲染，本任务只做拷贝
constexpr size_t kScratchBytes = 128 * 128; // 单字 A8 暂存上限
constexpr size_t kMaxScanEntries = 16;

struct SysGlyph {
    uint32_t cp;
    uint32_t offset; // pixels 中的起始位置
    uint16_t adv_w, box_w, box_h;
    int16_t ofs_x, ofs_y;
};

// 一个角色的只读快照：按码位排序的字形表 + 紧密排列的 A8 位图
struct SysFace {
    std::vector<SysGlyph> glyphs;
    std::vector<uint8_t> pixels;
    int ascent = 0;
    int descent = 0;
};

struct RoleSlot {
    lv_font_t font{};                       // 地址固定，供界面长期持有
    const lv_font_t* builtin = nullptr;     // 同规格内置 MiSans，亦为回退
    std::atomic<const SysFace*> face{nullptr};
    const SysFace* retired = nullptr;       // 上一次替换下来的快照，下次替换时释放
    bool inited = false;
};

RoleSlot s_roles[sysfont::kRoleCount];
std::mutex s_role_mutex;

struct BuildResult {
    std::string id;
    const SysFace* faces[sysfont::kRoleCount] = {};
    SystemFontState state = SystemFontState::kFailed;
    bool from_select = false;
    std::string prev_choice;
    SystemFontState prev_state = SystemFontState::kBuiltin;
};

std::mutex s_mu;                 // 保护以下字符串与请求
std::string s_choice = "ttf:Bookerly";
std::string s_pending;           // 待构建的选择串，空表示无
bool s_pending_from_select = false;
std::string s_prev_choice;
SystemFontState s_prev_state = SystemFontState::kBuiltin;
void (*s_done_cb)(bool) = nullptr;
std::vector<SystemFontEntry> s_scan;

std::atomic<SystemFontState> s_state{SystemFontState::kBuiltin};
std::atomic<bool> s_busy{false};
std::atomic<bool> s_cancel{false};
std::atomic<bool> s_resume{false};
std::atomic<bool> s_scan_busy{false};
std::atomic<bool> s_scan_ready{false};

const SysGlyph* Find(const SysFace* face, uint32_t cp) {
    const auto it = std::lower_bound(face->glyphs.begin(), face->glyphs.end(), cp,
                                     [](const SysGlyph& g, uint32_t v) { return g.cp < v; });
    return it != face->glyphs.end() && it->cp == cp ? &*it : nullptr;
}

bool GetDsc(const lv_font_t* font, lv_font_glyph_dsc_t* dsc, uint32_t letter, uint32_t /*next*/) {
    const auto* slot = static_cast<const RoleSlot*>(font->dsc);
    const SysFace* face = slot != nullptr ? slot->face.load(std::memory_order_acquire) : nullptr;
    if (face == nullptr || dsc == nullptr) return false; // 无快照或缺字：由 LVGL 回退内置字体
    if (letter == '\t') letter = ' ';
    const SysGlyph* g = Find(face, letter);
    if (g == nullptr) return false;
    dsc->adv_w = g->adv_w;
    dsc->box_w = g->box_w;
    dsc->box_h = g->box_h;
    dsc->ofs_x = g->ofs_x;
    dsc->ofs_y = g->ofs_y;
    dsc->stride = 0;
    dsc->format = LV_FONT_GLYPH_FORMAT_A8;
    dsc->is_placeholder = 0;
    dsc->gid.index = letter;
    return true;
}

const void* GetBitmap(lv_font_glyph_dsc_t* dsc, lv_draw_buf_t* buf) {
    if (dsc == nullptr || dsc->resolved_font == nullptr) return nullptr;
    const auto* slot = static_cast<const RoleSlot*>(dsc->resolved_font->dsc);
    const SysFace* face = slot != nullptr ? slot->face.load(std::memory_order_acquire) : nullptr;
    const SysGlyph* g = face != nullptr ? Find(face, dsc->gid.index) : nullptr;
    if (g == nullptr || g->box_w != dsc->box_w || g->box_h != dsc->box_h || g->box_w == 0 || g->box_h == 0) {
        return nullptr;
    }
    const uint8_t* pixels = face->pixels.data() + g->offset;
    if (dsc->req_raw_bitmap) return pixels;
    if (buf == nullptr || buf->data == nullptr) return nullptr;
    const uint32_t stride = buf->header.stride != 0 ? buf->header.stride
                                                    : lv_draw_buf_width_to_stride(g->box_w, LV_COLOR_FORMAT_A8);
    if (stride < g->box_w || static_cast<size_t>(stride) * g->box_h > buf->data_size) return nullptr;
    for (uint16_t y = 0; y < g->box_h; ++y) {
        std::memcpy(buf->data + static_cast<size_t>(y) * stride, pixels + static_cast<size_t>(y) * g->box_w, g->box_w);
    }
    buf->header.w = g->box_w;
    buf->header.h = g->box_h;
    buf->header.cf = LV_COLOR_FORMAT_A8;
    buf->header.stride = stride;
    lv_draw_buf_flush_cache(buf, nullptr);
    return buf;
}

RoleSlot* EnsureRole(int index) {
    if (index < 0 || index >= sysfont::kRoleCount) return nullptr;
    std::lock_guard<std::mutex> lock(s_role_mutex);
    RoleSlot& slot = s_roles[index];
    if (slot.inited) return &slot;
    const sysfont::Role role = sysfont::kRoles[index];
    const lv_font_t* builtin = fontpack_lv_font_builtin(role.size, role.bpp);
    if (builtin == nullptr) return nullptr;
    slot.builtin = builtin;
    slot.font.get_glyph_dsc = GetDsc;
    slot.font.get_glyph_bitmap = GetBitmap;
    slot.font.release_glyph = nullptr;
    slot.font.subpx = LV_FONT_SUBPX_NONE;
    slot.font.kerning = LV_FONT_KERNING_NONE;
    slot.font.static_bitmap = 0;
    slot.font.line_height = builtin->line_height;
    slot.font.base_line = builtin->base_line;
    slot.font.underline_position = builtin->underline_position;
    slot.font.underline_thickness = builtin->underline_thickness;
    slot.font.dsc = &slot;
    slot.font.fallback = builtin;
    slot.font.user_data = nullptr;
    slot.inited = true;
    return &slot;
}

void FreeFace(const SysFace* face) {
    delete face;
}

bool Aborted() {
    return s_cancel.load() || sd_access::IsQuiescing();
}

// 把源字体覆盖范围内的字形一次栅格化为快照；源字体缺字或经其自身回退得到的字形不收录
bool BuildFace(const lv_font_t* src, SysFace& face, bool* aborted) {
    std::vector<uint8_t> scratch(kScratchBytes);
    for (uint32_t cp = 0x20; cp <= 0x2122; ++cp) {
        if (!sysfont::InCoverage(cp)) continue;
        if (Aborted()) {
            *aborted = true;
            return false;
        }
        lv_font_glyph_dsc_t d{};
        if (!lv_font_get_glyph_dsc(src, &d, cp, 0) || d.resolved_font != src || d.is_placeholder) continue;
        SysGlyph g{cp, static_cast<uint32_t>(face.pixels.size()), d.adv_w, d.box_w, d.box_h, d.ofs_x, d.ofs_y};
        if (d.box_w != 0 && d.box_h != 0) {
            const size_t bytes = static_cast<size_t>(d.box_w) * d.box_h;
            lv_draw_buf_t buf{};
            if (bytes > scratch.size() ||
                lv_draw_buf_init(&buf, d.box_w, d.box_h, LV_COLOR_FORMAT_A8, d.box_w, scratch.data(),
                                 scratch.size()) != LV_RESULT_OK) {
                continue;
            }
            d.req_raw_bitmap = 0;
            if (lv_font_get_glyph_bitmap(&d, &buf) == nullptr) continue;
            const uint32_t stride = buf.header.stride != 0 ? buf.header.stride : d.box_w;
            for (uint16_t y = 0; y < d.box_h; ++y) {
                const uint8_t* row = static_cast<const uint8_t*>(buf.data) + static_cast<size_t>(y) * stride;
                face.pixels.insert(face.pixels.end(), row, row + d.box_w);
            }
            face.ascent = std::max(face.ascent, d.ofs_y + static_cast<int>(d.box_h));
            face.descent = std::max(face.descent, -static_cast<int>(d.ofs_y));
        }
        face.glyphs.push_back(g);
    }
    face.glyphs.shrink_to_fit();
    face.pixels.shrink_to_fit();
    return !face.glyphs.empty();
}

std::vector<std::string> ListDir(const char* dir) {
    std::vector<std::string> names;
    DIR* d = opendir(dir);
    if (d == nullptr) return names;
    while (dirent* e = readdir(d)) {
        if (e->d_name[0] != '.') names.emplace_back(e->d_name);
    }
    closedir(d);
    return names;
}

bool FileExists(const std::string& path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

// 逐角色构建；角色源文件缺失时该角色保持内置字体
SystemFontState BuildChoice(const sysfont::Choice& c, const SysFace* faces[], bool* aborted) {
    std::string files[sysfont::kRoleCount];
    if (c.kind == sysfont::FontKind::kTtf) {
        const sysfont::TtfFiles f = sysfont::PickTtfFiles(ListDir(SD_PATH_FONT_SRC), c.family);
        if (f.regular.empty()) return SystemFontState::kMissing;
        for (int i = 0; i < sysfont::kRoleCount; ++i) {
            // 粗体角色优先用粗体文件，没有时用常规字重
            files[i] = std::string(SD_PATH_FONT_SRC) + "/" + (i == 1 && !f.bold.empty() ? f.bold : f.regular);
        }
    } else {
        for (int i = 0; i < sysfont::kRoleCount; ++i) {
            const sysfont::Role r = sysfont::kRoles[i];
            std::string path = std::string(SD_PATH_FONTS) + "/" + sysfont::EfRoleFile(c.family, r.size, r.bpp);
            if (!FileExists(path) && r.bpp != 2) {
                path = std::string(SD_PATH_FONTS) + "/" + sysfont::EfRoleFile(c.family, r.size, 2);
            }
            if (FileExists(path)) files[i] = path;
        }
        if (files[0].empty()) return SystemFontState::kMissing;
    }
    for (int i = 0; i < sysfont::kRoleCount; ++i) {
        if (files[i].empty()) continue;
        const sysfont::Role r = sysfont::kRoles[i];
        const int64_t started = esp_timer_get_time();
        auto* face = new (std::nothrow) SysFace;
        if (face == nullptr) return SystemFontState::kFailed;
        bool built = false;
        if (c.kind == sysfont::FontKind::kTtf) {
            TtfRuntime* rt = TtfRuntimeOpen(files[i].c_str(), r.size);
            if (rt != nullptr) {
                built = BuildFace(TtfRuntimeFont(rt), *face, aborted);
                TtfRuntimeClose(rt);
            }
        } else {
            epdfont_t* ef = epdfont_open(files[i].c_str());
            if (ef != nullptr) {
                built = BuildFace(epdfont_get_lv_font(ef), *face, aborted);
                epdfont_close(ef);
            }
        }
        if (*aborted) {
            delete face;
            return SystemFontState::kFailed;
        }
        if (!built) {
            delete face;
            if (i == 0) return SystemFontState::kFailed; // 正文角色不可用则整体放弃
            continue;
        }
        ESP_LOGI(TAG, "role %u@%u glyphs=%u bytes=%u ms=%lld psram_free=%u", r.size, r.bpp,
                 static_cast<unsigned>(face->glyphs.size()), static_cast<unsigned>(face->pixels.size()),
                 static_cast<long long>((esp_timer_get_time() - started) / 1000),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
        faces[i] = face;
    }
    return SystemFontState::kActive;
}

void FreeResult(BuildResult* r) {
    for (const SysFace* f : r->faces) FreeFace(f);
    delete r;
}

void Persist(const std::string& id) {
    Application::GetInstance().Schedule([id]() {
        Settings settings(kNvsNs, true);
        if (settings.GetString(kNvsKey) != id) settings.SetString(kNvsKey, id);
    }, Application::TaskDomain::Storage);
}

void ApplyOnUi(void* arg);

void ApplyRetry(lv_timer_t* timer) {
    void* arg = lv_timer_get_user_data(timer);
    lv_timer_delete(timer);
    ApplyOnUi(arg);
}

// LVGL 线程：在界面绘制空闲时替换快照，旧快照延后到下次替换再释放
void ApplyOnUi(void* arg) {
    auto* r = static_cast<BuildResult*>(arg);
    if (!EpdTryBeginUiRender()) {
        lv_timer_create(ApplyRetry, 100, r);
        return;
    }
    const bool swap = r->state == SystemFontState::kActive || r->state == SystemFontState::kBuiltin;
    if (swap) {
        for (int i = 0; i < sysfont::kRoleCount; ++i) {
            RoleSlot* slot = EnsureRole(i);
            if (slot == nullptr) {
                FreeFace(r->faces[i]);
                r->faces[i] = nullptr;
                continue;
            }
            FreeFace(slot->retired);
            slot->retired = slot->face.load(std::memory_order_acquire);
            const SysFace* face = r->faces[i];
            r->faces[i] = nullptr;
            if (face != nullptr) {
                const sysfont::Metrics m = sysfont::CombineMetrics(slot->builtin->line_height,
                                                                   slot->builtin->base_line, face->ascent, face->descent);
                slot->font.line_height = static_cast<int32_t>(m.line_height);
                slot->font.base_line = static_cast<int32_t>(m.base_line);
            } else {
                slot->font.line_height = slot->builtin->line_height;
                slot->font.base_line = slot->builtin->base_line;
            }
            slot->face.store(face, std::memory_order_release);
        }
    }
    EpdEndUiRender();
    void (*done)(bool) = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mu);
        if (r->from_select) {
            done = s_done_cb;
            s_done_cb = nullptr;
            if (!swap) s_choice = r->prev_choice; // 选择失败：保留原字体与原选择
        }
    }
    s_state.store(swap || !r->from_select ? r->state : r->prev_state);
    if (swap) {
        lv_obj_report_style_change(nullptr); // 字体地址不变，通知所有标签按新字形重排
        Persist(r->id);
    }
    ESP_LOGI(TAG, "applied %s state=%u", r->id.c_str(), static_cast<unsigned>(r->state));
    const bool ok = swap;
    FreeResult(r);
    if (done != nullptr) done(ok);
}

void Worker(void*) {
    for (;;) {
        auto* r = new (std::nothrow) BuildResult;
        {
            std::lock_guard<std::mutex> lock(s_mu);
            if (s_pending.empty() || r == nullptr) {
                delete r;
                s_busy.store(false);
                break;
            }
            r->id.swap(s_pending);
            r->from_select = s_pending_from_select;
            r->prev_choice = s_prev_choice;
            r->prev_state = s_prev_state;
        }
        bool aborted = false;
        {
            sd_access::Transaction transaction;
            if (!transaction || Aborted()) {
                aborted = true;
            } else {
                r->state = BuildChoice(sysfont::ParseChoice(r->id), r->faces, &aborted);
            }
        }
        bool superseded = false;
        {
            std::lock_guard<std::mutex> lock(s_mu);
            superseded = !s_pending.empty(); // 更新的选择已到达，丢弃本次结果
        }
        if (aborted || superseded) {
            if (aborted && !s_cancel.load()) s_resume.store(true); // 待机中断：恢复后重试
            ESP_LOGW(TAG, "build %s %s", r->id.c_str(), aborted ? "aborted" : "superseded");
            FreeResult(r);
            continue;
        }
        while (!ScreenLvAsync(ApplyOnUi, r)) {
            if (s_cancel.load()) {
                FreeResult(r);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    vTaskDeleteWithCaps(nullptr);
}

bool StartBuild(const std::string& id, bool from_select) {
    {
        std::lock_guard<std::mutex> lock(s_mu);
        s_pending = id;
        s_pending_from_select = from_select;
    }
    s_cancel.store(false);
    s_state.store(SystemFontState::kLoading);
    if (s_busy.exchange(true)) return true; // 运行中的任务会取走最新请求
    if (xTaskCreatePinnedToCoreWithCaps(Worker, "sys_font", kWorkerStack, nullptr, 2, nullptr, tskNO_AFFINITY,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        s_busy.store(false);
        std::lock_guard<std::mutex> lock(s_mu);
        s_pending.clear();
        ESP_LOGW(TAG, "worker create failed");
        return false;
    }
    return true;
}

void ScanWorker(void*) {
    std::vector<SystemFontEntry> entries;
    {
        sd_access::Transaction transaction;
        if (transaction && !sd_access::IsQuiescing()) {
            std::vector<std::string> seen;
            auto add = [&](const std::string& kind, const std::string& family) {
                const std::string key = sysfont::Lower(kind + family);
                if (sysfont::IsBuiltinFamily(family) || entries.size() >= kMaxScanEntries ||
                    std::find(seen.begin(), seen.end(), key) != seen.end()) {
                    return;
                }
                seen.push_back(key);
                entries.push_back({kind + ":" + family, family});
            };
            for (const auto& n : ListDir(SD_PATH_FONT_SRC)) {
                const sysfont::TtfName t = sysfont::ParseTtfName(n);
                if (t.ok && t.style == sysfont::TtfStyle::kRegular) add("ttf", t.family);
            }
            for (const auto& n : ListDir(SD_PATH_FONTS)) {
                const sysfont::EfName e = sysfont::ParseEfName(n);
                if (e.ok && e.px == 30 && e.bpp == 2) add("ef", e.family);
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(s_mu);
        s_scan = std::move(entries);
    }
    s_scan_ready.store(true);
    s_scan_busy.store(false);
    vTaskDeleteWithCaps(nullptr);
}

} // namespace

const lv_font_t* SystemFontRole(uint16_t size, uint16_t bpp) {
    RoleSlot* slot = EnsureRole(sysfont::RoleIndex(size, bpp));
    return slot != nullptr ? &slot->font : nullptr;
}

void SystemFontInitAtBoot() {
    Settings settings(kNvsNs, false);
    const std::string id = sysfont::FormatChoice(sysfont::ParseChoice(settings.GetString(kNvsKey)));
    std::lock_guard<std::mutex> lock(s_mu);
    s_choice = id;
    ESP_LOGI(TAG, "boot choice %s", id.c_str());
}

void SystemFontStartBoot() {
    std::string id;
    {
        std::lock_guard<std::mutex> lock(s_mu);
        id = s_choice;
    }
    if (sysfont::ParseChoice(id).kind == sysfont::FontKind::kBuiltin) return;
    StartBuild(id, false);
}

bool SystemFontSelect(const char* id, void (*done)(bool ok)) {
    if (id == nullptr) return false;
    const std::string norm = sysfont::FormatChoice(sysfont::ParseChoice(id));
    {
        std::lock_guard<std::mutex> lock(s_mu);
        s_prev_choice = s_choice;
        s_prev_state = s_state.load();
        s_choice = norm;
        s_done_cb = done;
    }
    if (sysfont::ParseChoice(norm).kind == sysfont::FontKind::kBuiltin) {
        auto* r = new (std::nothrow) BuildResult;
        if (r == nullptr) return false;
        r->id = norm;
        r->state = SystemFontState::kBuiltin;
        r->from_select = true;
        {
            std::lock_guard<std::mutex> lock(s_mu);
            s_pending.clear(); // 放弃尚未开始的 SD 字体请求
        }
        ApplyOnUi(r);
        return true;
    }
    return StartBuild(norm, true);
}

SystemFontState SystemFontStatus() {
    return s_state.load();
}

std::string SystemFontChoiceId() {
    std::lock_guard<std::mutex> lock(s_mu);
    return s_choice;
}

void SystemFontCancel() {
    s_cancel.store(true);
}

bool SystemFontBusy() {
    return s_busy.load() || s_scan_busy.load();
}

void SystemFontOnResume() {
    if (!s_resume.exchange(false)) return;
    std::string id;
    {
        std::lock_guard<std::mutex> lock(s_mu);
        id = s_choice;
    }
    if (sysfont::ParseChoice(id).kind != sysfont::FontKind::kBuiltin) StartBuild(id, false);
}

void SystemFontRequestScan() {
    if (s_scan_busy.exchange(true)) return;
    s_scan_ready.store(false);
    if (xTaskCreatePinnedToCoreWithCaps(ScanWorker, "sys_font_scan", 8 * 1024, nullptr, 2, nullptr, tskNO_AFFINITY,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        s_scan_busy.store(false);
        ESP_LOGW(TAG, "scan task create failed");
    }
}

bool SystemFontScanDone(std::vector<SystemFontEntry>* out) {
    if (!s_scan_ready.exchange(false)) return false;
    std::lock_guard<std::mutex> lock(s_mu);
    if (out != nullptr) *out = s_scan;
    return true;
}
