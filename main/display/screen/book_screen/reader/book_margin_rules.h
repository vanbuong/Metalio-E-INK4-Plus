#pragma once

#include <algorithm>

// 阅读左右边距对称规则：纯函数，供偏好层与主机测试共用。
inline constexpr int kReaderSideMarginMin = 5; // 左右边距下限，单位像素
inline constexpr int kReaderSideMarginMax = 40; // 左右边距上限，单位像素

/** @brief 左右边距合并为同一值：取较大者并限制在 5–40 像素 */
inline int ReaderSymmetricSideMargin(int left, int right) {
    return std::clamp(std::max(left, right), kReaderSideMarginMin, kReaderSideMarginMax);
}
