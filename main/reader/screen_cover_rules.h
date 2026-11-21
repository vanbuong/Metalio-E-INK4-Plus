#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

// 待机 / 关机书籍封面的纯规则：整屏等比适配、灰度缩放、封面归属标记比对。
// 不依赖 IDF，供固件与主机测试共用。

namespace reader {

/** @brief 等比放大或缩小到不超过面板的最大尺寸；结果至少 1×1，无效输入返回 false */
inline bool FitToPanel(int width, int height, int max_width, int max_height,
                       int* fitted_width, int* fitted_height) {
    if (width <= 0 || height <= 0 || max_width <= 0 || max_height <= 0 ||
        fitted_width == nullptr || fitted_height == nullptr) {
        return false;
    }
    int64_t w = max_width;
    int64_t h = static_cast<int64_t>(height) * max_width / width;
    if (h > max_height) {
        h = max_height;
        w = static_cast<int64_t>(width) * max_height / height;
    }
    *fitted_width = static_cast<int>(std::max<int64_t>(1, std::min<int64_t>(w, max_width)));
    *fitted_height = static_cast<int>(std::max<int64_t>(1, std::min<int64_t>(h, max_height)));
    return true;
}

/** @brief 双线性缩放 L8 灰度图，四角像素对齐源图四角 */
inline void ResizeL8Bilinear(const uint8_t* src, int src_w, int src_h, uint8_t* dst, int dst_w, int dst_h) {
    if (src == nullptr || dst == nullptr || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;
    for (int y = 0; y < dst_h; ++y) {
        const int64_t fy = dst_h > 1 ? (static_cast<int64_t>(y) * (src_h - 1) << 16) / (dst_h - 1) : 0;
        const int y0 = static_cast<int>(fy >> 16);
        const int y1 = std::min(y0 + 1, src_h - 1);
        const int64_t wy = fy & 0xFFFF;
        for (int x = 0; x < dst_w; ++x) {
            const int64_t fx = dst_w > 1 ? (static_cast<int64_t>(x) * (src_w - 1) << 16) / (dst_w - 1) : 0;
            const int x0 = static_cast<int>(fx >> 16);
            const int x1 = std::min(x0 + 1, src_w - 1);
            const int64_t wx = fx & 0xFFFF;
            const int64_t a = src[static_cast<size_t>(y0) * src_w + x0], b = src[static_cast<size_t>(y0) * src_w + x1];
            const int64_t c = src[static_cast<size_t>(y1) * src_w + x0], d = src[static_cast<size_t>(y1) * src_w + x1];
            const int64_t top = a * (0x10000 - wx) + b * wx;
            const int64_t bottom = c * (0x10000 - wx) + d * wx;
            const int64_t value = (top * (0x10000 - wy) + bottom * wy + (int64_t{1} << 31)) >> 32;
            dst[static_cast<size_t>(y) * dst_w + x] = static_cast<uint8_t>(std::clamp<int64_t>(value, 0, 255));
        }
    }
}

/** @brief 封面归属标记（书籍绝对路径，可带行尾换行）是否与给定书籍一致 */
inline bool ScreenCoverTagMatches(const char* tag, size_t tag_len, const char* book_path) {
    if (tag == nullptr || book_path == nullptr || book_path[0] == '\0') return false;
    while (tag_len > 0 && (tag[tag_len - 1] == '\n' || tag[tag_len - 1] == '\r')) --tag_len;
    return tag_len > 0 && tag_len == std::strlen(book_path) && std::memcmp(tag, book_path, tag_len) == 0;
}

/** @brief 整屏封面可用：属于最近书籍，且 SD 仍是准备封面时的同一次挂载 */
inline bool ScreenCoverUsable(const char* ready_book, const char* latest_book, bool sd_mounted,
                              unsigned ready_mount_generation, unsigned mount_generation) {
    return ready_book != nullptr && latest_book != nullptr && ready_book[0] != '\0' &&
           std::strcmp(ready_book, latest_book) == 0 && sd_mounted &&
           ready_mount_generation == mount_generation;
}

} // namespace reader
