#pragma once

#include <cstring>
#include <string>

#include "reader_types.h"
#include "text_spacing.h"

namespace reader {

/** @brief 横排正文页上的一条绘制行（位图坐标） */
struct PageTextLine {
    const PageItem* item = nullptr;
    const std::string* text = nullptr; // 本行文字；章名多行时指向 walk 内部的分行缓冲
    int x = 0;          // 行首笔位
    int line_y = 0;     // 字形行框顶部
    int cell_top = 0;   // 行格顶部
    int cell_h = 0;     // 行格高度
    TextLineSpacing spacing;
    bool faux_bold = false; // 章名加粗
};

/**
 * @brief 按横排位图的行距、段距、缩进与章名居中规则逐行遍历正文页
 * @param on_line bool(const PageTextLine&)，返回 false 时终止并返回 false
 * @param on_image void(int top)，按页内插图顺序给出顶部坐标
 * @return 页面不适配当前布局或被终止时返回 false；否则返回页内是否有内容
 * @note RasterPageTextToL8 与取词命中共用，二者的行位置必须一致。
 */
template <class LineFn, class ImageFn>
bool ForEachPageTextLine(const Page& page, const lv_font_t* font, int spacing_percent, lv_coord_t para_gap,
                         int page_w, int page_h, const TextStyle& style, LineFn&& on_line, ImageFn&& on_image) {
    if (font == nullptr || page_w <= 0 || page_h <= 0) return false;
    const auto layout = MakeTextLineLayout(font, page_h, spacing_percent, false, style.line_gap);
    const lv_coord_t indent_px = ParaIndentPadPx(font, style.indent_chars);
    lv_coord_t y = 0;
    int row = 0;
    bool any = false, previous_title = false;
    std::string title_line;

    for (const auto& item : page.items) {
        if (item.kind != ContentKind::kText) {
            on_image(static_cast<int>(y));
            y += item.image_h;
            previous_title = false;
            any = true;
            continue;
        }
        if (item.text.empty()) continue;
        if (item.para_gap_before && any) {
            y = static_cast<lv_coord_t>(y + ReaderParagraphGap(para_gap, previous_title));
        }
        if (row >= layout.count || y + layout.CellHeight(row) > page_h) return false;

        PageTextLine line;
        line.item = &item;
        // 章名可能含「第N章\\n章节名」：按行居中并垂直推进
        if (item.chapter_title && item.text.find('\n') != std::string::npos) {
            const char* p = item.text.c_str();
            while (p != nullptr && *p != '\0') {
                const char* nl = std::strchr(p, '\n');
                title_line = nl ? std::string(p, static_cast<size_t>(nl - p)) : std::string(p);
                line.spacing = MakeTextLineSpacing(font, title_line, page_w, false, false, style.letter_gap);
                line.x = line.spacing.width < page_w ? (page_w - line.spacing.width) / 2 : 0;
                if (row >= layout.count || y + layout.CellHeight(row) > page_h) return false;
                line.text = &title_line;
                line.cell_top = y;
                line.cell_h = layout.CellHeight(row);
                line.line_y = y + layout.TextOffset(row);
                line.faux_bold = true;
                if (!on_line(static_cast<const PageTextLine&>(line))) return false;
                y += layout.CellHeight(row++);
                p = nl ? nl + 1 : nullptr;
            }
            any = true;
            previous_title = true;
            continue;
        }
        const int available = page_w - (item.para_indent && !item.chapter_title ? indent_px : 0);
        line.spacing = MakeTextLineSpacing(font, item.text, available,
            item.justify && style.justify && !item.chapter_title, false, style.letter_gap);
        line.x = 0;
        if (item.chapter_title) {
            line.x = line.spacing.width < page_w ? (page_w - line.spacing.width) / 2 : 0;
        } else if (item.para_indent) {
            line.x = indent_px;
        }
        line.text = &item.text;
        line.cell_top = y;
        line.cell_h = layout.CellHeight(row);
        line.line_y = y + layout.TextOffset(row);
        line.faux_bold = item.chapter_title;
        if (!on_line(static_cast<const PageTextLine&>(line))) return false;
        y += layout.CellHeight(row++);
        any = true;
        previous_title = item.chapter_title;
    }
    return any;
}

template <class LineFn>
bool ForEachPageTextLine(const Page& page, const lv_font_t* font, int spacing_percent, lv_coord_t para_gap,
                         int page_w, int page_h, const TextStyle& style, LineFn&& on_line) {
    return ForEachPageTextLine(page, font, spacing_percent, para_gap, page_w, page_h, style,
                               static_cast<LineFn&&>(on_line), [](int) {});
}

}  // namespace reader
