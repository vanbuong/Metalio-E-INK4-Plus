"""Host tests for the reader dictionary: word hit-testing, lookup rules and StarDict reading.

Run from the repository root: python tools/tests/test_reader_dict.py
Needs a C++17 compiler: g++ on PATH, or set CXX. No device or ESP-IDF required.
LVGL and FatFS are replaced by the minimal stand-ins in tools/tests/stubs.
"""
import os
import struct
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
    ROOT / "main/reader/page_word_hit.cc",
    ROOT / "main/reader/dictionary.cc",
]


def write_source(folder: Path, name: str, entries):
    """Write a raw StarDict dictionary (html entries, unsorted) for the prep tool."""
    folder.mkdir(parents=True, exist_ok=True)
    data, idx = b"", b""
    for word, text in entries:
        payload = text.encode("utf-8")
        idx += word.encode("utf-8") + b"\0" + struct.pack(">II", len(data), len(payload))
        data += payload
    (folder / f"{name}.dict").write_bytes(data)
    (folder / f"{name}.idx").write_bytes(idx)
    (folder / f"{name}.ifo").write_text(
        f"StarDict's dict ifo file\nversion=2.4.2\nbookname={name} book\nwordcount={len(entries)}\n"
        f"idxfilesize={len(idx)}\nsametypesequence=h\n", encoding="utf-8")
    return folder / f"{name}.ifo"


def prepare(src: Path, lang: str, out: Path):
    subprocess.run([sys.executable, str(ROOT / "tools/dict/prepare_stardict.py"), str(src),
                    "--lang", lang, "--out", str(out)], check=True, stdout=subprocess.DEVNULL)


def build_fixtures(out_dir: Path) -> Path:
    """Prepared dictionaries for the reader tests; returns the directory passed to the binary.

    <fixtures>/dict/zz_envi   en-vi: Apple, apple, banana, run, zebra
    <fixtures>/dict/aa_wordnet en-en: book, run
    <fixtures>/dict/nopos     prepared but .pos removed (must be skipped)
    <fixtures>/trunc/trunc    en-en with a truncated .dict
    """
    fixtures = out_dir / "fixtures"
    src = out_dir / "src"
    prepare(write_source(src / "zz_envi", "zz_envi", [
        ("run", "<b>chạy</b>"), ("banana", "quả chuối"), ("apple", "quả táo"),
        ("zebra", "ngựa vằn"), ("Apple", "công ty Apple")]), "en-vi", fixtures / "dict")
    prepare(write_source(src / "aa_wordnet", "aa_wordnet", [
        ("run", "move fast by using one's feet"), ("book", "a written work")]), "en-en", fixtures / "dict")
    prepare(write_source(src / "nopos", "nopos", [("cat", "animal")]), "en-en", fixtures / "dict")
    (fixtures / "dict/nopos/nopos.pos").unlink()
    prepare(write_source(src / "trunc", "trunc", [("alpha", "first letter"), ("beta", "second letter")]),
            "en-en", fixtures / "trunc")
    dict_file = fixtures / "trunc/trunc/trunc.dict"
    dict_file.write_bytes(dict_file.read_bytes()[:3])
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
