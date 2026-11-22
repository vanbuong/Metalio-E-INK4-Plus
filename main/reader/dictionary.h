#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace reader {

// 阅读取词词典：SD 卡上经 tools/dict/prepare_stardict.py 处理的 StarDict（纯文本、未压缩、带 .pos）。
// 只用标准文件接口；每次查询自行打开并关闭文件，SD 访问许可由调用方持有。

enum class DictLang : uint8_t { kEnVi, kEnEn, kOther };

struct DictInfo {
    std::string dir;      // 词典目录（含 <name>.ifo/.idx/.dict/.pos）
    std::string name;     // 目录名，即文件基名
    std::string bookname; // .ifo 中的显示名
    DictLang lang = DictLang::kOther;
    uint32_t wordcount = 0;
};

struct DictEntry {
    std::string bookname;   // 来源词典显示名
    std::string headword;   // 实际命中的词条
    std::string definition; // 纯文本释义，最多 kDictDefinitionMax 字节
};

constexpr size_t kDictDefinitionMax = 8192;

/** @brief 列出 root 下各子目录中有效的已处理词典；英越在前、英英其次，同类按目录名 */
std::vector<DictInfo> ScanDictionaries(const std::string& root);

/**
 * @brief 按 StarDict 顺序二分查找词条；大小写不敏感，优先完全同形
 * @return 命中返回 true；未命中或读文件出错返回 false
 */
bool DictLookupExact(const DictInfo& dict, std::string_view word, DictEntry& out);

/** @brief 依次用 DictLookupCandidates(word) 在每本词典中查找，每本至多一条 */
std::vector<DictEntry> DictLookup(const std::vector<DictInfo>& dicts, std::string_view word);

}  // namespace reader
