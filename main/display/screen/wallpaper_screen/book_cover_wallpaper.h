#pragma once

#include <cstddef>

// 待机 / 关机书籍封面：为当前书籍准备整屏封面文件，并供解码任务解析路径。
// 准备在低优先级后台任务中进行，打开书籍与关机流程都不等待它。

/** @brief 为该书准备整屏封面；任意任务可调用，两项封面选项均关闭时不做事，新请求取代旧请求。 */
void BookCoverWallpaper_RequestPrepare(const char* book_abs_path);

/** @brief 最近打开的书籍已有可用整屏封面；只读内存，可在 LVGL 任务中调用。 */
bool BookCoverWallpaper_Ready();

/** @brief 解析整屏封面路径；仅在解码等后台任务中调用，无可用封面返回 false。 */
bool BookCoverWallpaper_TryResolvePath(char* out_path, size_t out_len);
