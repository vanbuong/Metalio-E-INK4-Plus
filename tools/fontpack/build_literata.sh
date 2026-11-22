#!/usr/bin/env bash
# Literata 生产 fontpack（拉丁 + 越南语，无汉字）+ Noto Emoji 30@2 + Latin Modern Math 18/28/36@2。
# 18@2 先放数学字形（与旧包一致），再用 Literata 补齐 18@2 的拉丁 / 越南语字符。
#
# 用法（在 tools/fontpack/ 下）：
#   ./build_literata.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
export PYTHONUTF8=1

PY="${ROOT}/.venv/bin/python"
[[ -x "$PY" ]] || PY="${ROOT}/.venv/Scripts/python.exe"
if [[ ! -x "$PY" ]]; then
  echo "venv missing; run: python3 -m venv .venv && .venv/bin/pip install -r requirements.txt" >&2
  exit 1
fi

LIT="../ttf-fonts/Literata"
BOOK="${LIT}/LiterataBook.otf"
SEMI="${LIT}/LiterataBook-Semibold.otf"
CHARS="charset_latin_vi.txt"
EMOJI_TTF="../ttf-fonts/Noto_Emoji/static/NotoEmoji-Regular.ttf"
MATH_TTF="../ttf-fonts/LatinModernMath/latinmodern-math.otf"
OUT_DIR="fonts/Literata"
FINAL="${OUT_DIR}/font_literata.fontpack"

for f in "$BOOK" "$SEMI" "$CHARS" "$EMOJI_TTF" "$MATH_TTF"; do
  [[ -f "$f" ]] || { echo "missing: $f" >&2; exit 1; }
done
mkdir -p "$OUT_DIR"

echo "==> 1/4 Literata base (25@2 / 30@2 Book + 30@4 Semibold)"
"$PY" build_fontpack.py --chars-file "$CHARS" \
  --variant-map "25:2=${BOOK},30:2=${BOOK},30:4=${SEMI}" --add-cps 0x20,0xA0,0x23 \
  -o "${OUT_DIR}/.b1.fontpack"

echo "==> 2/4 merge Noto Emoji 30@2"
"$PY" merge_emoji_into_fontpack.py --base "${OUT_DIR}/.b1.fontpack" \
  --emoji-ttf "$EMOJI_TTF" -o "${OUT_DIR}/.b2.fontpack"

echo "==> 3/4 merge Latin Modern Math 18/28/36@2"
"$PY" merge_math_into_fontpack.py --base "${OUT_DIR}/.b2.fontpack" \
  --math-ttf "$MATH_TTF" --chars-file "charset_math.txt" \
  --sizes 18,28,36 --bpp 2 -o "${OUT_DIR}/.b3.fontpack"

echo "==> 4/4 Literata 18@2 for characters math did not cover"
"$PY" merge_math_into_fontpack.py --base "${OUT_DIR}/.b3.fontpack" \
  --math-ttf "$BOOK" --chars-file "$CHARS" --add-cps 0xA0,0x23 \
  --sizes 18 --bpp 2 -o "$FINAL"

rm -f "${OUT_DIR}"/.b?.fontpack
# 固件合并 / idf.py flash 默认读 main/assets/fonts/font.fontpack
cp -f "$FINAL" "../../main/assets/fonts/font.fontpack"
"$PY" check_literata_pack.py "$FINAL"
echo "done: $(wc -c < "$FINAL") bytes"
