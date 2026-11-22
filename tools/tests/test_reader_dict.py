"""Host tests for the reader dictionary: word hit-testing, lookup rules and StarDict reading.

Run from the repository root: python tools/tests/test_reader_dict.py
Needs a C++17 compiler: g++ on PATH, or set CXX. No device or ESP-IDF required.
LVGL and FatFS are replaced by the minimal stand-ins in tools/tests/stubs.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CXX = os.environ.get("CXX", "g++")
INCLUDES = [ROOT / "tools/tests/stubs", ROOT / "main/reader"]
SOURCES = [
    ROOT / "tools/tests/test_reader_dict.cpp",
    ROOT / "main/reader/text_encoding.cc",
]


def build_fixtures(out_dir: Path) -> Path:
    """Hook for dictionary fixtures; returns the directory passed to the test binary."""
    fixtures = out_dir / "fixtures"
    fixtures.mkdir()
    return fixtures


with tempfile.TemporaryDirectory(prefix="metalio-reader-dict-") as output:
    out = Path(output)
    exe = out / "reader_dict.exe"
    subprocess.run([CXX, "-std=c++17", "-Wall", "-Werror",
                    *("-I" + str(p) for p in INCLUDES),
                    *(str(s) for s in SOURCES), "-o", str(exe)], check=True)
    fixtures = build_fixtures(out)
    result = subprocess.run([str(exe), str(fixtures)])
    if result.returncode != 0:
        sys.exit(result.returncode)
    print("reader_dict: PASS")
