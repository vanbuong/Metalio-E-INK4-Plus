#include "dictionary.h"

#include <dirent.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

#include "dict_word_rules.h"

namespace reader {
namespace {

constexpr size_t kIdxProbeMax = 256 + 1 + 8; // StarDict 词条最长 255 字节 + NUL + 偏移/长度

struct FileCloser {
    void operator()(FILE* f) const { if (f) std::fclose(f); }
};
using File = std::unique_ptr<FILE, FileCloser>;

File Open(const std::string& path) { return File(std::fopen(path.c_str(), "rb")); }

std::string PathOf(const DictInfo& dict, const char* ext) { return dict.dir + "/" + dict.name + ext; }

int FoldCompare(std::string_view a, std::string_view b) {
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        unsigned ca = static_cast<unsigned char>(a[i]), cb = static_cast<unsigned char>(b[i]);
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return ca < cb ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

uint32_t ReadBe32(const unsigned char* p) {
    return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | uint32_t{p[3]};
}

struct IdxEntry {
    std::string word;
    uint32_t offset = 0, size = 0;
};

bool ReadIdxEntry(FILE* pos, FILE* idx, uint32_t index, IdxEntry& out) {
    unsigned char le[4];
    if (std::fseek(pos, static_cast<long>(index) * 4, SEEK_SET) != 0 || std::fread(le, 1, 4, pos) != 4) return false;
    const uint32_t at = uint32_t{le[0]} | (uint32_t{le[1]} << 8) | (uint32_t{le[2]} << 16) | (uint32_t{le[3]} << 24);
    unsigned char buf[kIdxProbeMax];
    if (std::fseek(idx, static_cast<long>(at), SEEK_SET) != 0) return false;
    const size_t got = std::fread(buf, 1, sizeof(buf), idx);
    const auto* nul = static_cast<const unsigned char*>(std::memchr(buf, 0, got));
    if (nul == nullptr || static_cast<size_t>(nul - buf) + 9 > got) return false;
    out.word.assign(reinterpret_cast<const char*>(buf), static_cast<size_t>(nul - buf));
    out.offset = ReadBe32(nul + 1);
    out.size = ReadBe32(nul + 5);
    return true;
}

bool ParseIfo(const std::string& path, DictInfo& info) {
    File f = Open(path);
    if (!f) return false;
    char line[512];
    bool header = false, plain = false;
    while (std::fgets(line, sizeof(line), f.get())) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (!header) { header = s.rfind("StarDict's dict ifo file", 0) == 0; if (!header) return false; continue; }
        const size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = s.substr(0, eq), value = s.substr(eq + 1);
        if (key == "bookname") info.bookname = value;
        else if (key == "wordcount") info.wordcount = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        else if (key == "sametypesequence") plain = value == "m";
        else if (key == "lang") info.lang = value == "en-vi" ? DictLang::kEnVi : value == "en-en" ? DictLang::kEnEn : DictLang::kOther;
    }
    return header && plain && info.wordcount > 0;
}

long FileSize(FILE* f) {
    if (std::fseek(f, 0, SEEK_END) != 0) return -1;
    return std::ftell(f);
}

void TrimUtf8(std::string& s, size_t max) {
    if (s.size() <= max) return;
    size_t cut = max;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
    s.resize(cut);
}

}  // namespace

std::vector<DictInfo> ScanDictionaries(const std::string& root) {
    std::vector<DictInfo> out;
    DIR* dir = opendir(root.c_str());
    if (dir == nullptr) return out;
    while (const dirent* ent = readdir(dir)) {
        if (ent->d_name[0] == '.') continue;
        DictInfo info;
        info.name = ent->d_name;
        info.dir = root + "/" + info.name;
        if (!ParseIfo(PathOf(info, ".ifo"), info)) continue;
        File pos = Open(PathOf(info, ".pos"));
        File idx = Open(PathOf(info, ".idx"));
        File data = Open(PathOf(info, ".dict"));
        if (!pos || !idx || !data || FileSize(pos.get()) != static_cast<long>(info.wordcount) * 4) continue;
        if (info.bookname.empty()) info.bookname = info.name;
        out.push_back(std::move(info));
    }
    closedir(dir);
    std::sort(out.begin(), out.end(), [](const DictInfo& a, const DictInfo& b) {
        return a.lang != b.lang ? a.lang < b.lang : a.name < b.name;
    });
    return out;
}

bool DictLookupExact(const DictInfo& dict, std::string_view word, DictEntry& out) {
    if (word.empty() || word.size() > 255 || dict.wordcount == 0) return false;
    File pos = Open(PathOf(dict, ".pos"));
    File idx = Open(PathOf(dict, ".idx"));
    if (!pos || !idx) return false;
    // 下界：第一个折叠后不小于 word 的词条
    uint32_t lo = 0, hi = dict.wordcount;
    IdxEntry entry;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        if (!ReadIdxEntry(pos.get(), idx.get(), mid, entry)) return false;
        if (FoldCompare(entry.word, word) < 0) lo = mid + 1;
        else hi = mid;
    }
    IdxEntry best;
    bool found = false;
    for (uint32_t i = lo; i < dict.wordcount; ++i) {
        if (!ReadIdxEntry(pos.get(), idx.get(), i, entry) || FoldCompare(entry.word, word) != 0) break;
        if (!found || entry.word == word) { best = entry; found = true; }
        if (entry.word == word) break;
    }
    if (!found) return false;
    File data = Open(PathOf(dict, ".dict"));
    if (!data) return false;
    const size_t want = std::min<size_t>(best.size, kDictDefinitionMax + 4);
    std::string text(want, '\0');
    if (std::fseek(data.get(), static_cast<long>(best.offset), SEEK_SET) != 0 ||
        std::fread(text.data(), 1, want, data.get()) != want) return false;
    TrimUtf8(text, kDictDefinitionMax);
    out.bookname = dict.bookname;
    out.headword = best.word;
    out.definition = std::move(text);
    return true;
}

std::vector<DictEntry> DictLookup(const std::vector<DictInfo>& dicts, std::string_view word) {
    std::vector<DictEntry> out;
    const auto candidates = DictLookupCandidates(word);
    for (const auto& dict : dicts) {
        DictEntry entry;
        for (const auto& candidate : candidates) {
            if (DictLookupExact(dict, candidate, entry)) { out.push_back(std::move(entry)); break; }
        }
    }
    return out;
}

}  // namespace reader
