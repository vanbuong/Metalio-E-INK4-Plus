"""Host tests for the reader's line-break, justification and margin rules.

Run from the repository root: python tools/tests/test_text_break.py
Needs a C++17 compiler: g++ on PATH, or set CXX. No device or ESP-IDF required.
"""
import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CXX = os.environ.get("CXX", "g++")
INCLUDES = [ROOT / "main/reader", ROOT / "main/display/screen/book_screen/reader", ROOT / "main/display/font"]

with tempfile.TemporaryDirectory(prefix="metalio-text-break-") as output:
    exe = Path(output) / "text_break.exe"
    subprocess.run([CXX, "-std=c++17", "-Wall", "-Werror",
                    *("-I" + str(p) for p in INCLUDES),
                    str(ROOT / "tools/tests/test_text_break.cpp"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print("text_break: PASS")
