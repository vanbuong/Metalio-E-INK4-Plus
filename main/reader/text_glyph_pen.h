#pragma once

#include <cstddef>
#include <cstdint>

#include "lvgl.h"
#include "text_spacing.h"

namespace reader {

/**
 * @brief 横排逐字笔位：位图绘制与取词命中共用，保证两者的字位完全一致
 * @note 与 MakeTextLineSpacing 的收紧与两端对齐规则配套；换行符不推进笔位。
 */
class HorizontalGlyphPen {
public:
    HorizontalGlyphPen(const lv_font_t* font, const char* utf8, size_t size, int x,
                       const TextLineSpacing& spacing, int letter_gap)
        : font_(font), spacing_(spacing), letter_gap_(letter_gap), pen_x_(x) {
        state_.cjk_context = TextLineCjkContext(utf8, size);
    }

    /**
     * @brief 推进一个码点
     * @param out_x 该字步进格左缘（含对齐偏移，不含 ofs_x），可为空
     * @param out_adv 该字步进（含字间距），可为空
     */
    void Step(uint32_t cp, const lv_font_glyph_dsc_t& glyph, bool known, int* out_x, int* out_adv) {
        if (cp == '\n' || cp == '\r') {
            if (out_x) *out_x = pen_x_ + Shift();
            if (out_adv) *out_adv = 0;
            return;
        }
        ObserveTextGlyph(state_, font_, cp, glyph, known, letter_gap_);
        const int advance = known ? TextAdvanceWithGap(TextGlyphAdvance(glyph), letter_gap_)
            : TextAdvanceWithGap(std::max(1, static_cast<int>(lv_font_get_glyph_width(font_, cp, 0))), letter_gap_);
        if (out_x) *out_x = pen_x_ + Shift();
        if (out_adv) *out_adv = advance;
        pen_x_ += advance;
    }

    /** @brief 行尾坐标，与位图绘制的 out_end_x 一致 */
    int EndX() const { return pen_x_ + Shift() - spacing_.trim; }

private:
    int Shift() const { return spacing_.Offset(state_.slots, state_.capacity); }

    const lv_font_t* font_;
    TextLineSpacing spacing_;
    int letter_gap_;
    int pen_x_;
    TextSpacingState state_;
};

}  // namespace reader
