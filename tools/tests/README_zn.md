# 主机侧回归测试

**中文** | [English](README.md)

`test_lifecycle_fixes.py` 从当前固件源码提取实际生命周期函数，在主机上注入 API 成功、失败和超时结果，验证 Wi-Fi 临时扫描的初始化失败清理、所有权与重复释放，以及显示供数超时后的停机、回调撤销和资源复用边界。

在仓库根目录运行：

```powershell
python tools/tests/test_lifecycle_fixes.py
```

脚本使用 Windows MSVC Build Tools 与 Windows SDK；`VC`、`UM` 需对应本机安装目录。测试源码和可执行文件在临时目录中生成，结束或失败时自动清理。

这类测试不连接设备，不能代替 Wi-Fi 实际收发、DMA/中断时序或墨水屏显示验证。

## 阅读排版规则

`test_text_break.py` 以 `main/reader/vertical_text.h` 中的纯头文件阅读规则编译并运行 `test_text_break.cpp`，验证英文、越南语横排按词断行，空白与组合附加符悬挂，按词间距两端对齐，以及中文断点与原逐字规则一致。

```powershell
python tools/tests/test_text_break.py
```

需要 C++17 编译器：`PATH` 中的 `g++`（如 MinGW-w64），或设置 `CXX`。通过时输出 `text_break: PASS`。
