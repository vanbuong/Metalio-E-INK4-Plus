#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

// 系统字体纯规则：文件名解析、角色表、覆盖范围、NVS 选择串。
// 不依赖 IDF / LVGL，供固件与主机测试共用。

namespace sysfont {

enum class TtfStyle : uint8_t { kRegular, kBold, kOther };
enum class FontKind : uint8_t { kBuiltin, kTtf, kEf };

struct TtfName {
    bool ok = false;
    std::string family;
    TtfStyle style = TtfStyle::kRegular;
};

struct EfName {
    bool ok = false;
    std::string family;
    int px = 0;
    int bpp = 0;
};

struct TtfFiles {
    std::string regular; // 常规字重文件名，空表示该族不可用
    std::string bold;    // 粗体文件名，可为空
};

struct Choice {
    FontKind kind = FontKind::kTtf;
    std::string family;
};

struct Role {
    uint16_t size;
    uint16_t bpp;
};

// 系统字体角色：正文 30@2、粗体 30@4、小号 25@2、最小 18@2（与内置 MiSans 规格一致）
inline constexpr Role kRoles[] = {{30, 2}, {30, 4}, {25, 2}, {18, 2}};
inline constexpr int kRoleCount = 4;
inline constexpr char kDefaultFamily[] = "Bookerly";

inline std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline bool EndsWithNoCase(const std::string& s, const char* suffix) {
    const std::string tail = suffix;
    return s.size() > tail.size() && Lower(s.substr(s.size() - tail.size())) == tail;
}

/** @brief 解析 TTF/OTF 文件名：族名 + 字重；未识别的末尾词视为族名的一部分 */
inline TtfName ParseTtfName(const std::string& name) {
    TtfName out;
    if (!EndsWithNoCase(name, ".ttf") && !EndsWithNoCase(name, ".otf")) return out;
    std::string stem = name.substr(0, name.size() - 4);
    const size_t cut = stem.find_last_of(" -_");
    const std::string token = Lower(cut == std::string::npos ? std::string() : stem.substr(cut + 1));
    const std::string head = cut == std::string::npos ? stem : stem.substr(0, cut);
    if (token == "regular" || token == "book" || token == "roman") {
        out.style = TtfStyle::kRegular;
        stem = head;
    } else if (token == "bold" || token == "semibold" || token == "demibold") {
        out.style = TtfStyle::kBold;
        stem = head;
    } else if (token.find("italic") != std::string::npos || token.find("oblique") != std::string::npos ||
               token == "light" || token == "thin" || token == "medium" || token == "black" ||
               token == "heavy" || token == "extralight" || token == "extrabold" || token == "ultralight" ||
               token == "ultrabold" || token == "hairline") {
        out.style = TtfStyle::kOther;
        stem = head;
    }
    while (!stem.empty() && (stem.back() == ' ' || stem.back() == '-' || stem.back() == '_')) stem.pop_back();
    if (stem.empty()) return out;
    out.family = stem;
    out.ok = true;
    return out;
}

/** @brief 在文件名列表中为族挑选常规与粗体文件（族名不区分大小写） */
inline TtfFiles PickTtfFiles(const std::vector<std::string>& names, const std::string& family) {
    TtfFiles out;
    const std::string want = Lower(family);
    for (const auto& n : names) {
        const TtfName t = ParseTtfName(n);
        if (!t.ok || Lower(t.family) != want) continue;
        if (t.style == TtfStyle::kRegular && out.regular.empty()) out.regular = n;
        if (t.style == TtfStyle::kBold && out.bold.empty()) out.bold = n;
    }
    return out;
}

/** @brief 解析 {family}_{px}_{bpp}.ef */
inline EfName ParseEfName(const std::string& name) {
    EfName out;
    if (!EndsWithNoCase(name, ".ef")) return out;
    const std::string stem = name.substr(0, name.size() - 3);
    const size_t b = stem.rfind('_');
    if (b == std::string::npos || b == 0) return out;
    const size_t p = stem.rfind('_', b - 1);
    if (p == std::string::npos || p == 0) return out;
    const std::string px = stem.substr(p + 1, b - p - 1), bpp = stem.substr(b + 1);
    if (px.empty() || bpp.empty() || px.find_first_not_of("0123456789") != std::string::npos ||
        bpp.find_first_not_of("0123456789") != std::string::npos) {
        return out;
    }
    out.family = stem.substr(0, p);
    out.px = std::atoi(px.c_str());
    out.bpp = std::atoi(bpp.c_str());
    out.ok = out.px > 0 && out.bpp > 0;
    return out;
}

/** @brief 内置 MiSans 已覆盖的族，不在 SD 列表中重复显示 */
inline bool IsBuiltinFamily(const std::string& family) {
    return Lower(family).find("misans") != std::string::npos;
}

inline std::string EfRoleFile(const std::string& family, int px, int bpp) {
    return family + "_" + std::to_string(px) + "_" + std::to_string(bpp) + ".ef";
}

/** @brief (size,bpp) 对应的角色序号；非角色规格返回 -1 */
inline int RoleIndex(uint16_t size, uint16_t bpp) {
    for (int i = 0; i < kRoleCount; ++i) {
        if (kRoles[i].size == size && kRoles[i].bpp == bpp) return i;
    }
    return -1;
}

/** @brief 快照覆盖的码位：拉丁基础 / 补充 / 扩展 A、越南语、常用标点与符号；其余回退 MiSans */
inline bool InCoverage(uint32_t cp) {
    static constexpr uint32_t kRanges[][2] = {
        {0x0020, 0x007E}, {0x00A0, 0x017F}, {0x01A0, 0x01A1}, {0x01AF, 0x01B0},
        {0x1EA0, 0x1EF9}, {0x2010, 0x2027}, {0x2030, 0x203A}, {0x20AC, 0x20AC}, {0x2122, 0x2122},
    };
    for (const auto& r : kRanges) {
        if (cp >= r[0] && cp <= r[1]) return true;
    }
    return false;
}

// 首页大时钟：160px，只显示 "HH:MM"
inline constexpr uint16_t kClockSize = 160;

/** @brief 时钟快照覆盖的码位：数字、冒号、空格；其余回退内置 160px 字体 */
inline bool InClockCoverage(uint32_t cp) {
    return (cp >= '0' && cp <= '9') || cp == ':' || cp == ' ';
}

/** @brief NVS 选择串：builtin | ttf:<族> | ef:<族>；缺失或非法时为默认 Bookerly */
inline Choice ParseChoice(const std::string& stored) {
    Choice c{FontKind::kTtf, kDefaultFamily};
    if (stored == "builtin") return Choice{FontKind::kBuiltin, ""};
    if (stored.rfind("ttf:", 0) == 0 && stored.size() > 4) return Choice{FontKind::kTtf, stored.substr(4)};
    if (stored.rfind("ef:", 0) == 0 && stored.size() > 3) return Choice{FontKind::kEf, stored.substr(3)};
    return c;
}

inline std::string FormatChoice(const Choice& c) {
    switch (c.kind) {
        case FontKind::kBuiltin: return "builtin";
        case FontKind::kEf: return "ef:" + c.family;
        default: return "ttf:" + c.family;
    }
}

} // namespace sysfont
