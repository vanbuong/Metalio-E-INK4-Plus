#include "system_font.h"

#include "application.h"
#include "epd_refresh.h"
#include "epdfont.h"
#include "fontpack_lvgl.h"
#include "home_screen/home_screen.h"
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
constexpr size_t kScratchBytes = 160 * 160; // 单字 A8 暂存上限（含 160px 时钟数字）
constexpr size_t kMaxScanEntries = 16;
constexpr uint32_t kRetireFreeMs = 1000; // 替换后延迟释放旧快照，等待可能的离线测量结束

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
};

struct RoleSlot {
    lv_font_t font{};                       // 地址固定，供界面长期持有
    const lv_font_t* builtin = nullptr;     // 同规格内置 Literata，亦为回退
    std::atomic<const SysFace*> face{nullptr};
    const SysFace* retired = nullptr;       // 被替换下来的快照，稍后释放
    std::atomic<const SysFace*>* source = nullptr; // 字形来源：角色自身快照或共享的时钟快照
    std::atomic<bool> inited{false};
};

RoleSlot s_roles[sysfont::kRoleCount];
// 首页大时钟：两个变体（经典 / 斜杠卡片行高不同）共享一个 160px 数字快照
RoleSlot s_clock_slots[2];
std::atomic<const SysFace*> s_clock_face{nullptr};
const SysFace* s_clock_retired = nullptr;
std::mutex s_role_mutex;

struct BuildResult {
    std::string id;
    const SysFace* faces[sysfont::kRoleCount] = {};
    const SysFace* clock = nullptr;
    SystemFontState state = SystemFontState::kFailed;
    bool from_select = false;
};

std::mutex s_mu;                 // 保护以下字符串、请求与回调
std::string s_choice = "builtin";      // 用户当前选择（含正在加载的）
std::string s_settled_id = "builtin";  // 最近一次已生效或已确定结果的选择
SystemFontState s_settled_state = SystemFontState::kBuiltin;
std::string s_pending;           // 待构建的选择串，空表示无
bool s_pending_from_select = false;
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
    const SysFace* face = slot != nullptr && slot->source != nullptr ? slot->source->load(std::memory_order_acquire) : nullptr;
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
    const SysFace* face = slot != nullptr && slot->source != nullptr ? slot->source->load(std::memory_order_acquire) : nullptr;
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

void InitSlot(RoleSlot& slot, const lv_font_t* builtin, std::atomic<const SysFace*>* source);

RoleSlot* EnsureRole(int index) {
    if (index < 0 || index >= sysfont::kRoleCount) return nullptr;
    RoleSlot& slot = s_roles[index];
    if (slot.inited.load(std::memory_order_acquire)) return &slot;
    std::lock_guard<std::mutex> lock(s_role_mutex);
    if (slot.inited.load(std::memory_order_relaxed)) return &slot;
    const sysfont::Role role = sysfont::kRoles[index];
    const lv_font_t* builtin = fontpack_lv_font_builtin(role.size, role.bpp);
    if (builtin == nullptr) return nullptr;
    InitSlot(slot, builtin, &slot.face);
    return &slot;
}

// 以内置字体为回退和行度量初始化固定地址的字体
void InitSlot(RoleSlot& slot, const lv_font_t* builtin, std::atomic<const SysFace*>* source) {
    slot.builtin = builtin;
    slot.source = source;
    slot.font.get_glyph_dsc = GetDsc;
    slot.font.get_glyph_bitmap = GetBitmap;
    slot.font.release_glyph = nullptr;
    slot.font.subpx = LV_FONT_SUBPX_NONE;
    slot.font.kerning = LV_FONT_KERNING_NONE;
    slot.font.static_bitmap = 0;
    // 行高与基线固定沿用内置字体：界面布局与阅读页脚（进而分页缓存）不随系统字体变化
    slot.font.line_height = builtin->line_height;
    slot.font.base_line = builtin->base_line;
    slot.font.underline_position = builtin->underline_position;
    slot.font.underline_thickness = builtin->underline_thickness;
    slot.font.dsc = &slot;
    slot.font.fallback = builtin;
    slot.font.user_data = nullptr;
    slot.inited.store(true, std::memory_order_release);
}

bool Aborted() {
    return s_cancel.load() || sd_access::IsQuiescing();
}

// 把源字体覆盖范围内的字形一次栅格化为快照；源字体缺字或经其自身回退得到的字形不收录
bool BuildFace(const lv_font_t* src, SysFace& face, bool* aborted, bool (*cover)(uint32_t) = sysfont::InCoverage) {
    std::vector<uint8_t> scratch(kScratchBytes);
    for (uint32_t cp = 0x20; cp <= 0x2122; ++cp) {
        if (!cover(cp)) continue;
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

// SD 访问未获准：待机/关机中为中断（稍后重试），否则视为无卡
SystemFontState NotAdmitted(bool* aborted) {
    if (Aborted()) *aborted = true;
    return SystemFontState::kMissing;
}

// 逐角色构建，每个角色单独持有 SD 事务，避免长时间阻塞 U 盘模式与卸卡
SystemFontState BuildChoice(const sysfont::Choice& c, const SysFace* faces[], bool* aborted) {
    std::string files[sysfont::kRoleCount];
    {
        sd_access::Transaction transaction;
        if (!transaction) return NotAdmitted(aborted);
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
    }
    for (int i = 0; i < sysfont::kRoleCount; ++i) {
        if (files[i].empty()) continue;
        sd_access::Transaction transaction;
        if (!transaction) return NotAdmitted(aborted);
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
        if (!built && Aborted()) *aborted = true; // 打开期间进入待机：按中断处理，恢复后重试
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

// 首页时钟数字（仅 TTF/OTF，.ef 无 160px 档）；失败不影响其他角色
const SysFace* BuildClock(const sysfont::Choice& c, bool* aborted) {
    if (c.kind != sysfont::FontKind::kTtf) return nullptr;
    sd_access::Transaction transaction;
    if (!transaction) {
        if (Aborted()) *aborted = true;
        return nullptr;
    }
    const sysfont::TtfFiles f = sysfont::PickTtfFiles(ListDir(SD_PATH_FONT_SRC), c.family);
    if (f.regular.empty()) return nullptr;
    const std::string path = std::string(SD_PATH_FONT_SRC) + "/" + f.regular;
    TtfRuntime* rt = TtfRuntimeOpen(path.c_str(), sysfont::kClockSize);
    if (rt == nullptr) {
        if (Aborted()) *aborted = true;
        return nullptr;
    }
    auto* face = new (std::nothrow) SysFace;
    const bool built = face != nullptr && BuildFace(TtfRuntimeFont(rt), *face, aborted, sysfont::InClockCoverage);
    TtfRuntimeClose(rt);
    if (!built) {
        delete face;
        return nullptr;
    }
    ESP_LOGI(TAG, "clock %u glyphs=%u bytes=%u", sysfont::kClockSize, static_cast<unsigned>(face->glyphs.size()),
             static_cast<unsigned>(face->pixels.size()));
    return face;
}

void FreeResult(BuildResult* r) {
    for (const SysFace* f : r->faces) delete f;
    delete r->clock;
    delete r;
}

void Persist(const std::string& id) {
    Application::GetInstance().Schedule([id]() {
        Settings settings(kNvsNs, true);
        if (settings.GetString(kNvsKey) != id) settings.SetString(kNvsKey, id);
    }, Application::TaskDomain::Storage);
}

void FreeRetired(lv_timer_t* timer) {
    lv_timer_delete(timer);
    for (RoleSlot& slot : s_roles) {
        delete slot.retired;
        slot.retired = nullptr;
    }
    delete s_clock_retired;
    s_clock_retired = nullptr;
}

void ApplyOnUi(void* arg);

void ApplyRetry(lv_timer_t* timer) {
    void* arg = lv_timer_get_user_data(timer);
    lv_timer_delete(timer);
    ApplyOnUi(arg);
}

// LVGL 线程：在界面绘制空闲时替换快照；仅接受与当前选择一致的结果
void ApplyOnUi(void* arg) {
    auto* r = static_cast<BuildResult*>(arg);
    {
        std::lock_guard<std::mutex> lock(s_mu);
        if (r->id != s_choice) { // 已被更新的选择取代，丢弃且不保存
            ESP_LOGI(TAG, "drop stale %s (now %s)", r->id.c_str(), s_choice.c_str());
            FreeResult(r);
            return;
        }
    }
    const bool swap = r->state == SystemFontState::kActive || r->state == SystemFontState::kBuiltin;
    if (swap) {
        if (!EpdTryBeginUiRender()) {
            lv_timer_create(ApplyRetry, 100, r);
            return;
        }
        for (int i = 0; i < sysfont::kRoleCount; ++i) {
            RoleSlot* slot = EnsureRole(i);
            const SysFace* face = r->faces[i];
            r->faces[i] = nullptr;
            if (slot == nullptr) {
                delete face;
                continue;
            }
            delete slot->retired;
            slot->retired = slot->face.load(std::memory_order_acquire);
            slot->face.store(face, std::memory_order_release);
        }
        delete s_clock_retired;
        s_clock_retired = s_clock_face.load(std::memory_order_acquire);
        s_clock_face.store(r->clock, std::memory_order_release);
        r->clock = nullptr;
        EpdEndUiRender();
        lv_timer_create(FreeRetired, kRetireFreeMs, nullptr);
    }
    void (*done)(bool) = nullptr;
    SystemFontState state = r->state;
    {
        std::lock_guard<std::mutex> lock(s_mu);
        if (r->from_select) {
            done = s_done_cb;
            s_done_cb = nullptr;
        }
        if (swap || !r->from_select) {
            s_settled_id = r->id; // 生效，或开机选择确定为缺失/失败
            s_settled_state = r->state;
        } else {
            s_choice = s_settled_id; // 选择失败：恢复上一次确定的选择与状态
            state = s_settled_state;
        }
    }
    s_state.store(state);
    if (swap) {
        lv_obj_report_style_change(nullptr); // 字体地址与行高不变，标签按新字形重新测量
        Persist(r->id);
        if (!r->from_select && ScreenIsHome()) ScreenLoadReplace(HomeScreen::Create()); // 开机后到的字体：重建首页
    }
    ESP_LOGI(TAG, "applied %s state=%u", r->id.c_str(), static_cast<unsigned>(r->state));
    FreeResult(r);
    if (done != nullptr) done(swap);
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
        }
        bool aborted = Aborted();
        if (!aborted) r->state = BuildChoice(sysfont::ParseChoice(r->id), r->faces, &aborted);
        if (!aborted && r->state == SystemFontState::kActive) r->clock = BuildClock(sysfont::ParseChoice(r->id), &aborted);
        aborted = aborted || s_cancel.load();
        bool superseded = false;
        {
            std::lock_guard<std::mutex> lock(s_mu);
            superseded = !s_pending.empty() || r->id != s_choice;
            if (aborted && !superseded) s_done_cb = nullptr; // 中断后由恢复流程重试，不再通知原选择
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

// 恢复到上一次确定的选择与状态（启动任务失败时）
void RestoreSettled() {
    std::lock_guard<std::mutex> lock(s_mu);
    s_choice = s_settled_id;
    s_done_cb = nullptr;
    s_pending.clear();
    s_state.store(s_settled_state);
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
        RestoreSettled();
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

const lv_font_t* SystemFontClock(const lv_font_t* builtin) {
    if (builtin == nullptr) return nullptr;
    for (RoleSlot& slot : s_clock_slots) {
        if (slot.inited.load(std::memory_order_acquire) && slot.builtin == builtin) return &slot.font;
    }
    std::lock_guard<std::mutex> lock(s_role_mutex);
    for (RoleSlot& slot : s_clock_slots) {
        if (slot.inited.load(std::memory_order_relaxed) && slot.builtin == builtin) return &slot.font;
        if (!slot.inited.load(std::memory_order_relaxed)) {
            InitSlot(slot, builtin, &s_clock_face);
            return &slot.font;
        }
    }
    return builtin; // 变体超出预留数量时退回内置字体
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
    if (sysfont::ParseChoice(id).kind == sysfont::FontKind::kBuiltin) {
        std::lock_guard<std::mutex> lock(s_mu);
        s_settled_id = id;
        return;
    }
    StartBuild(id, false);
}

bool SystemFontSelect(const char* id, void (*done)(bool ok)) {
    if (id == nullptr) return false;
    const std::string norm = sysfont::FormatChoice(sysfont::ParseChoice(id));
    {
        std::lock_guard<std::mutex> lock(s_mu);
        s_choice = norm;
        s_done_cb = done;
    }
    if (sysfont::ParseChoice(norm).kind == sysfont::FontKind::kBuiltin) {
        auto* r = new (std::nothrow) BuildResult;
        if (r == nullptr) {
            RestoreSettled();
            return false;
        }
        r->id = norm;
        r->state = SystemFontState::kBuiltin;
        r->from_select = true;
        {
            std::lock_guard<std::mutex> lock(s_mu);
            s_pending.clear(); // 放弃尚未开始的 SD 字体请求
        }
        // 经 UI 队列执行，避免在按钮点击回调中重建设置页
        if (!ScreenLvAsync(ApplyOnUi, r)) {
            FreeResult(r);
            RestoreSettled();
            return false;
        }
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
