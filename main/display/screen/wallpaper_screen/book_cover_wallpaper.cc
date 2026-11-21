#include "wallpaper_screen/book_cover_wallpaper.h"
#include "wallpaper_screen/wallpaper_active.h"
#include "book_cover_sidecar.h"
#include "book_home_snapshot.h"
#include "config.h"
#include "image_util.h"
#include "reader_types.h"
#include "screen_cover_rules.h"
#include "sd_access.h"
#include "sd_paths.h"
#include "SdCardManager.hpp"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <new>
#include <string>
#include <vector>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr const char* TAG = "BookCoverWp";
constexpr uint32_t kPrepareStack = 16 * 1024; // EPUB 解压与 JPEG 解码，栈放 PSRAM
constexpr size_t kTagMaxBytes = SD_FILE_PATH_CAPACITY;

std::mutex s_mu; // 保护待处理请求、运行标志与就绪书籍
std::string s_pending; // 最新请求的书籍绝对路径，空表示无待处理
std::string s_ready_book; // 已就绪整屏封面所属书籍
unsigned s_ready_mount_generation = 0; // 准备封面时的 SD 挂载代次
bool s_running = false; // 准备任务是否存活
std::atomic<uint32_t> s_generation{0}; // 每次请求递增，旧任务据此放弃写入

bool FileNonEmpty(const char* path) {
    struct stat st {};
    return stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

void SetReady(const std::string& book) {
    std::lock_guard<std::mutex> lock(s_mu);
    s_ready_book = book;
    s_ready_mount_generation = sd_access::MountGeneration();
}

void ClearReady() {
    std::lock_guard<std::mutex> lock(s_mu);
    s_ready_book.clear();
}

bool TagMatches(const std::string& book) {
    FILE* fp = std::fopen(SD_PATH_SCREEN_COVER_TAG, "rb");
    if (fp == nullptr) return false;
    char buf[kTagMaxBytes] = {};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
    std::fclose(fp);
    return reader::ScreenCoverTagMatches(buf, n, book.c_str());
}

bool Stale(uint32_t generation) {
    return s_generation.load(std::memory_order_acquire) != generation;
}

void PrepareLocked(const std::string& book, uint32_t generation);

// 整屏缓冲约 0.8 MB：PSRAM 不足时放弃本次准备，回退壁纸，不让任务异常终止
void Prepare(const std::string& book, uint32_t generation) {
    try {
        PrepareLocked(book, generation);
    } catch (const std::bad_alloc&) {
        ESP_LOGW(TAG, "screen cover skipped reason=no_memory");
    }
}

void PrepareLocked(const std::string& book, uint32_t generation) {
    sd_access::Transaction access;
    if (!access || sd_access::IsQuiescing()) {
        ESP_LOGW(TAG, "screen cover skipped reason=sd_busy");
        return;
    }
    if (TagMatches(book) && FileNonEmpty(SD_PATH_SCREEN_COVER)) {
        SetReady(book);
        ESP_LOGI(TAG, "screen cover ready reason=cached");
        return;
    }
    reader::RasterImage src;
    if (!reader::DecodeBookCoverToL8(book.c_str(), DISPLAY_WIDTH, DISPLAY_HEIGHT, src)) {
        // 无封面（TXT 等）：清除归属标记，待机与关机回退壁纸
        unlink(SD_PATH_SCREEN_COVER_TAG);
        ClearReady();
        ESP_LOGI(TAG, "screen cover none");
        return;
    }
    if (Stale(generation)) return;
    int fit_w = 0, fit_h = 0;
    if (!reader::FitToPanel(src.width, src.height, DISPLAY_WIDTH, DISPLAY_HEIGHT, &fit_w, &fit_h)) return;
    reader::RasterImage panel;
    if (fit_w == src.width && fit_h == src.height) {
        panel = std::move(src);
    } else {
        panel.pixels.resize(static_cast<size_t>(fit_w) * static_cast<size_t>(fit_h));
        reader::ResizeL8Bilinear(src.pixels.data(), src.width, src.height, panel.pixels.data(), fit_w, fit_h);
        panel.width = static_cast<uint16_t>(fit_w);
        panel.height = static_cast<uint16_t>(fit_h);
        std::vector<uint8_t>().swap(src.pixels); // 编码前释放源图，降低 PSRAM 峰值
        src.Reset();
    }
    std::vector<uint8_t> bytes;
    if (!reader::EncodeL8ToA2l8(panel, bytes)) {
        ESP_LOGW(TAG, "screen cover encode failed width=%d height=%d", fit_w, fit_h);
        return;
    }
    if (Stale(generation)) return;
    // 先撤销归属标记，写入过程中断时不会把旧书封面当作新书
    unlink(SD_PATH_SCREEN_COVER_TAG);
    ClearReady();
    if (!reader::WriteFileAtomic(SD_PATH_SCREEN_COVER, bytes.data(), bytes.size()) ||
        !reader::WriteFileAtomic(SD_PATH_SCREEN_COVER_TAG, reinterpret_cast<const uint8_t*>(book.data()),
                                 book.size())) {
        ESP_LOGW(TAG, "screen cover write failed bytes=%u", static_cast<unsigned>(bytes.size()));
        return;
    }
    SetReady(book);
    ESP_LOGI(TAG, "screen cover ready width=%d height=%d bytes=%u", fit_w, fit_h,
             static_cast<unsigned>(bytes.size()));
}

void PrepareWorker(void*) {
    for (;;) {
        std::string book;
        uint32_t generation = 0;
        {
            std::lock_guard<std::mutex> lock(s_mu);
            if (s_pending.empty()) {
                s_running = false;
                break;
            }
            book.swap(s_pending);
            generation = s_generation.load(std::memory_order_acquire);
        }
        Prepare(book, generation);
    }
    vTaskDeleteWithCaps(nullptr);
}

} // namespace

void BookCoverWallpaper_RequestPrepare(const char* book_abs_path) {
    if (book_abs_path == nullptr || book_abs_path[0] == '\0') return;
    if (!Wallpaper_BookCoverEnabled(true) && !Wallpaper_BookCoverEnabled(false)) return;
    {
        std::lock_guard<std::mutex> lock(s_mu);
        s_pending = book_abs_path;
        s_generation.fetch_add(1, std::memory_order_acq_rel);
        if (s_running) return;
        s_running = true;
    }
    if (xTaskCreatePinnedToCoreWithCaps(PrepareWorker, "scr_cover", kPrepareStack, nullptr, tskIDLE_PRIORITY + 1,
                                        nullptr, 0, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        std::lock_guard<std::mutex> lock(s_mu);
        s_running = false;
        ESP_LOGW(TAG, "screen cover task create failed");
    }
}

bool BookCoverWallpaper_Ready() {
    std::string ready;
    unsigned ready_generation = 0;
    {
        std::lock_guard<std::mutex> lock(s_mu);
        ready = s_ready_book;
        ready_generation = s_ready_mount_generation;
    }
    const std::string latest = reader::book_home_snapshot::LoadLatestAbsPath();
    return reader::ScreenCoverUsable(ready.c_str(), latest.c_str(), SdCardManager::GetInstance().IsMounted(),
                                     ready_generation, sd_access::MountGeneration());
}

bool Wallpaper_StandbyShowsImage() {
    return Wallpaper_HasStandbySelection() || (Wallpaper_BookCoverEnabled(true) && BookCoverWallpaper_Ready());
}

bool BookCoverWallpaper_TryResolvePath(char* out_path, size_t out_len) {
    if (out_path == nullptr || out_len == 0) return false;
    out_path[0] = '\0';
    if (!BookCoverWallpaper_Ready() || !FileNonEmpty(SD_PATH_SCREEN_COVER)) return false;
    const int n = std::snprintf(out_path, out_len, "%s", SD_PATH_SCREEN_COVER);
    return n > 0 && static_cast<size_t>(n) < out_len;
}
