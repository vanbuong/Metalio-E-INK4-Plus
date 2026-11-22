#pragma once

#include <lvgl.h>

// 阅读取词：长按正文单词，在底部卡片显示英越、英英释义（词典在 SD 卡 /dict）。
// 卡片独立于 ReadChrome；打开时由点按、按键、自动翻页入口检查 DictSheetVisible()。

/** @brief 正文 LV_EVENT_LONG_PRESSED 回调（LVGL 线程） */
void OnReadContentLongPressed(lv_event_t* e);
/** @brief 关闭取词卡片并作废进行中的查询（LVGL 线程） */
void CloseDictSheet();
/** @brief 取词卡片是否显示；任意线程可读 */
bool DictSheetVisible();
