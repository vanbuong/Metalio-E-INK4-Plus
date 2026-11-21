#include "system_font.h"

// 第一步：仅提供路由钩子，尚未启用角色字体（行为与原内置 MiSans 完全一致）。
const lv_font_t* SystemFontRole(uint16_t /*size*/, uint16_t /*bpp*/) {
    return nullptr;
}
