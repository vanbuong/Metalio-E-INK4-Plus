#!/usr/bin/env bash
# 首页大时钟字体 main/display/font/font_literata_160_2.c（数字、冒号、空格；160px 2bpp）。
# Literata 默认是旧式数字（3/4/5/7/9 下沉），先把 cmap 的 0-9 换成等高数字（.lf）再转换。
# 需要 node（npx）与 tools/fontpack/.venv（fonttools）。
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
PY="${ROOT}/.venv/bin/python"
[[ -x "$PY" ]] || PY="${ROOT}/.venv/Scripts/python.exe"

TMP="fonts/Literata/.clock"
mkdir -p "$TMP"
"$PY" - "../ttf-fonts/Literata/LiterataBook.otf" "${TMP}/LiterataLining.otf" <<'EOF'
import sys
from fontTools.ttLib import TTFont
f = TTFont(sys.argv[1])
names = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"]
for table in f["cmap"].tables:
    if table.isUnicode():
        for i, n in enumerate(names):
            table.cmap[0x30 + i] = n + ".lf"
f.save(sys.argv[2])
EOF

npx --yes lv_font_conv@1.5.3 --font "${TMP}/LiterataLining.otf" -r 0x20,0x30-0x3A \
  --size 160 --bpp 2 --format lvgl --no-compress --no-kerning --lv-include lvgl.h \
  --lv-font-name font_literata_160_2 -o ../../main/display/font/font_literata_160_2.c
rm -rf "$TMP"
# 行高 / 基线沿用旧 MiSans 160 字体（214 / 46），首页布局不变
sed -i 's/\.line_height = [0-9]*,/.line_height = 214,/; s/\.base_line = [0-9]*,/.base_line = 46,/' \
  ../../main/display/font/font_literata_160_2.c
echo "wrote main/display/font/font_literata_160_2.c"
