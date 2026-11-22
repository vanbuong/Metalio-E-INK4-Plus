// Minimal LVGL stand-in for host tests of reader layout code.
// Only the symbols the reader headers under test use; layouts match LVGL 9 field names.
#pragma once

#include <cstddef>
#include <cstdint>

typedef int32_t lv_coord_t;

struct lv_font_t;

struct lv_font_glyph_dsc_t {
    const lv_font_t* resolved_font = nullptr;
    uint16_t adv_w = 0;
    uint16_t box_w = 0;
    uint16_t box_h = 0;
    int16_t ofs_x = 0;
    int16_t ofs_y = 0;
};

struct lv_font_t {
    bool (*get_glyph_dsc)(const lv_font_t*, lv_font_glyph_dsc_t*, uint32_t letter, uint32_t letter_next) = nullptr;
    void (*release_glyph)(const lv_font_t*, lv_font_glyph_dsc_t*) = nullptr;
    int32_t line_height = 0;
    int32_t base_line = 0;
    const void* dsc = nullptr;
};

inline bool lv_font_get_glyph_dsc(const lv_font_t* font, lv_font_glyph_dsc_t* dsc, uint32_t letter,
                                  uint32_t letter_next) {
    *dsc = {};
    if (font == nullptr || font->get_glyph_dsc == nullptr) return false;
    const bool ok = font->get_glyph_dsc(font, dsc, letter, letter_next);
    if (ok && dsc->resolved_font == nullptr) dsc->resolved_font = font;
    return ok;
}

inline uint16_t lv_font_get_glyph_width(const lv_font_t* font, uint32_t letter, uint32_t letter_next) {
    lv_font_glyph_dsc_t g;
    return lv_font_get_glyph_dsc(font, &g, letter, letter_next) ? g.adv_w : 0;
}

#define LV_IMAGE_HEADER_MAGIC 0x19
enum { LV_COLOR_FORMAT_L8 = 0x06, LV_COLOR_FORMAT_A8 = 0x0E };

struct lv_image_header_t {
    uint32_t magic = 0, cf = 0, flags = 0, w = 0, h = 0, stride = 0;
};

struct lv_image_dsc_t {
    lv_image_header_t header;
    uint32_t data_size = 0;
    const uint8_t* data = nullptr;
};
