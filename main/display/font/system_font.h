#pragma once

#include "lvgl.h"

#include <stdint.h>

// 系统字体：除阅读正文外的全部界面字体。四个角色（30@2 正文、30@4 粗体、25@2、18@2）
// 的 lv_font_t 地址固定；所选字体的常用拉丁字形在后台一次栅格化到 PSRAM 快照，
// 缺字逐字回退内置 MiSans。默认字体为 SD 卡上的 Bookerly，缺失时即为 MiSans。

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 角色字体：size/bpp 为系统字体角色时返回固定地址的字体，否则 nullptr */
const lv_font_t* SystemFontRole(uint16_t size, uint16_t bpp);

#ifdef __cplusplus
}

#include <string>
#include <vector>

enum class SystemFontState : uint8_t {
    kBuiltin, // 使用内置 MiSans
    kLoading, // 正在后台生成快照
    kActive,  // SD 字体已生效
    kFailed,  // 读取或内存失败，使用 MiSans
    kMissing, // 未找到所选字体文件，使用 MiSans
};

struct SystemFontEntry {
    std::string id;    // NVS 选择串，如 "ttf:Bookerly"
    std::string label; // 列表显示名
};

/** @brief 读取 NVS 中的系统字体选择；在 UI 初始化线程（可访问 NVS）调用，不访问 SD */
void SystemFontInitAtBoot();
/** @brief SD 挂载后开始加载开机选择的字体（后台，完成后在 LVGL 线程生效） */
void SystemFontStartBoot();
/**
 * @brief 选择系统字体（"builtin" | "ttf:<族>" | "ef:<族>"）；仅在 LVGL 线程调用
 * @param done 生效或失败后在 LVGL 线程回调一次；SD 字体成功生效后才写入 NVS
 */
bool SystemFontSelect(const char* id, void (*done)(bool ok));
SystemFontState SystemFontStatus();
/** @brief 当前选择串（含尚在加载或缺失的选择） */
std::string SystemFontChoiceId();
/** @brief 关机前取消后台任务；SystemFontBusy 为 false 后方可断电 */
void SystemFontCancel();
bool SystemFontBusy();
/** @brief 待机恢复 SD 后重试被中断的加载 */
void SystemFontOnResume();
/** @brief 后台扫描 SD 字体族；SystemFontScanDone 在完成后返回 true 并取出结果 */
void SystemFontRequestScan();
bool SystemFontScanDone(std::vector<SystemFontEntry>* out);
#endif
