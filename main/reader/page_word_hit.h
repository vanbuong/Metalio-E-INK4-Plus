#pragma once

#include <string>
#include <string_view>

#include "reader_types.h"

namespace reader {

/** @brief 取词结果；word 为空表示触点下没有单词 */
struct PageWordHit {
    std::string word;
};

/**
 * @brief 求横排位图正文页上 (x, y) 处的单词
 * @param page_w 位图宽度（即正文栏宽），其余参数与 RasterPageTextToL8 一致
 * @param x,y 相对位图左上角的坐标
 * @return 命中单词时返回 true
 */
bool PageWordAt(const Page& page, const lv_font_t* font, int spacing_percent, lv_coord_t para_gap,
                int page_w, int page_h, const TextStyle& style, int x, int y, PageWordHit& out);

/**
 * @brief 把 UTF-8 行内某字节处扩展为所在单词
 * @note 字母含 ASCII、拉丁扩展、越南文及组合附加符；撇号与连字符仅在两侧均为字母时保留；
 *       U+2019 统一为 '\''。不在字母上时返回空串。
 */
std::string WordAround(std::string_view line, size_t byte_offset);

}  // namespace reader
