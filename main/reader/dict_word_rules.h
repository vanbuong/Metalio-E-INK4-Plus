#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace reader {

/**
 * @brief 生成查词候选：原词、ASCII 小写，再按英文词形规则去词尾（复数、所有格、过去式、进行时、比较级）
 * @note 含非 ASCII 字符的词只做原词与小写；词干短于 2 个字母的候选丢弃；结果去重且保持顺序。
 */
inline std::vector<std::string> DictLookupCandidates(std::string_view word) {
    std::vector<std::string> out;
    auto add = [&](std::string s) {
        if (s.size() < 2 && !(out.empty() && !s.empty())) return;
        for (const auto& have : out) if (have == s) return;
        out.push_back(std::move(s));
    };
    if (word.empty()) return out;
    add(std::string(word));
    std::string lower(word);
    bool ascii = true;
    for (auto& c : lower) {
        if (static_cast<unsigned char>(c) >= 0x80) ascii = false;
        else if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    add(lower);
    if (!ascii) return out;

    auto ends = [&](std::string_view suffix) {
        return lower.size() > suffix.size() && lower.compare(lower.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    auto cut = [&](size_t n) { return lower.substr(0, lower.size() - n); };
    auto is_vowel = [](char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; };
    auto undouble = [&](const std::string& stem) {
        const size_t n = stem.size();
        if (n >= 3 && stem[n - 1] == stem[n - 2] && !is_vowel(stem[n - 1])) add(stem.substr(0, n - 1));
    };

    if (ends("'s")) add(cut(2));
    if (ends("s'")) add(cut(2));
    if (ends("ies")) add(cut(3) + "y");
    if (ends("ied")) add(cut(3) + "y");
    if (ends("es")) add(cut(2));
    if (ends("s") && !ends("ss") && !ends("'s")) add(cut(1));
    if (ends("ed")) {
        undouble(cut(2));
        add(cut(2));
        add(cut(1)); // ed→e：hoped → hope
    }
    if (ends("ing")) {
        undouble(cut(3));
        add(cut(3));
        add(cut(3) + "e");
    }
    if (ends("est")) add(cut(3));
    else if (ends("er")) add(cut(2));
    return out;
}

}  // namespace reader
