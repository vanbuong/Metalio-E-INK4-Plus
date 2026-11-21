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
#endif

#ifdef __cplusplus
enum class SystemFontState : uint8_t {
    kBuiltin, // 使用内置 MiSans
    kLoading, // 正在后台生成快照
    kActive,  // SD 字体已生效
    kFailed,  // 加载失败（读取或内存），使用 MiSans
    kMissing, // 未找到所选字体文件，使用 MiSans
};

/** @brief 启动时读取 NVS 并开始加载，最多等待 wait_ms；需在内部 RAM 栈任务（LVGL 初始化前）调用 */
void SystemFontInitAtBoot(uint32_t wait_ms);
/** @brief 选择系统字体（"builtin" | "ttf:<族>" | "ef:<族>"）；仅在 LVGL 线程调用，结果异步生效 */
bool SystemFontSelect(const char* id);
SystemFontState SystemFontStatus();
/** @brief 当前生效的选择串 */
const char* SystemFontCurrentId();
/** @brief 关机前取消后台加载；SystemFontBusy 为 false 后方可断电 */
void SystemFontCancel();
bool SystemFontBusy();
/** @brief 待机恢复 SD 后重试被中断的加载 */
void SystemFontOnResume();
/** @brief 选择或启动加载完成且已应用时的一次性通知（LVGL 线程）；用于重建设置页 */
void SystemFontSetAppliedCallback(void (*cb)(bool ok));
#endif
