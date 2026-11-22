#!/usr/bin/env python3
"""Prepare a StarDict dictionary for the Metalio E-Ink4-Plus reader.

The device reads uncompressed, plain-text StarDict files plus a `.pos` offset table so it can
binary-search the index straight from the SD card without loading it into RAM.

Usage:
    python tools/dict/prepare_stardict.py <source.ifo> --lang en-vi|en-en --out <dir>

Writes <dir>/<name>/<name>.{ifo,idx,dict,pos}. Copy <dir>/<name> to /dict/<name> on the SD card.
Output contract (read by main/reader/dictionary.cc):
  .ifo   keeps bookname; wordcount/idxfilesize updated; sametypesequence=m; lang=<lang>
  .idx   StarDict 2.4.2 entries: word NUL, u32 BE offset, u32 BE size; StarDict sort order
  .dict  uncompressed UTF-8 plain text
  .pos   u32 LE byte offset of each .idx entry, in index order
"""
import argparse
import gzip
import html
import re
import struct
import sys
from pathlib import Path

TEXT_TYPES = set("mlgxhw")      # converted to plain text
SKIP_TYPES = set("tykr")        # lowercase, NUL-terminated, dropped


def fail(msg: str) -> None:
    print(f"prepare_stardict: {msg}", file=sys.stderr)
    sys.exit(1)


def read_maybe_gz(base: Path, plain_suffix: str, gz_suffixes) -> bytes:
    plain = Path(str(base) + plain_suffix)
    if plain.exists():
        return plain.read_bytes()
    for suffix in gz_suffixes:
        packed = Path(str(base) + suffix)
        if packed.exists():
            return gzip.decompress(packed.read_bytes())
    fail(f"missing {plain.name} (or compressed variant)")
    return b""


def parse_ifo(path: Path) -> dict:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or not lines[0].startswith("StarDict's dict ifo file"):
        fail(f"{path.name} is not a StarDict .ifo")
    return dict(line.split("=", 1) for line in lines[1:] if "=" in line)


def parse_idx(idx: bytes):
    entries, i = [], 0
    while i < len(idx):
        end = idx.index(b"\0", i)
        offset, size = struct.unpack(">II", idx[end + 1:end + 9])
        entries.append((idx[i:end], offset, size))
        i = end + 9
    return entries


def markup_to_text(raw: str) -> str:
    text = re.sub(r"(?i)<br\s*/?>|</(p|div|li|tr|h[1-6])\s*>", "\n", raw)
    text = re.sub(r"<[^>]*>", "", text)
    text = html.unescape(text)
    lines = (re.sub(r"[ \t\r\f\v]+", " ", line).strip() for line in text.split("\n"))
    return "\n".join(line for line in lines if line)


def entry_text(blob: bytes, sametypesequence: str) -> str:
    """Extract the plain-text parts of one .dict entry."""
    parts = []
    pos = 0

    def take_field(kind: str, last: bool):
        nonlocal pos
        if kind.isupper():
            if last:
                data, pos = blob[pos:], len(blob)
            else:
                (size,) = struct.unpack(">I", blob[pos:pos + 4])
                data, pos = blob[pos + 4:pos + 4 + size], pos + 4 + size
            return None
        if last:
            data, pos = blob[pos:], len(blob)
        else:
            end = blob.find(b"\0", pos)
            end = len(blob) if end < 0 else end
            data, pos = blob[pos:end], end + 1
        if kind in TEXT_TYPES:
            return data.rstrip(b"\0").decode("utf-8", "replace")
        return None

    if sametypesequence:
        for n, kind in enumerate(sametypesequence):
            parts.append(take_field(kind, n == len(sametypesequence) - 1))
    else:
        while pos < len(blob):
            kind = chr(blob[pos])
            pos += 1
            parts.append(take_field(kind, False))
    texts = [markup_to_text(p) for p in parts if p]
    return "\n".join(t for t in texts if t)


def stardict_key(word: bytes):
    return (word.lower(), word)  # bytes.lower() folds ASCII only, like g_ascii_strcasecmp


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ifo", type=Path)
    ap.add_argument("--lang", required=True, choices=["en-vi", "en-en"])
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    info = parse_ifo(args.ifo)
    if info.get("idxoffsetbits", "32") != "32":
        fail("idxoffsetbits=64 is not supported (dictionary too large)")
    base = Path(str(args.ifo)[:-len(".ifo")]) if args.ifo.name.endswith(".ifo") else args.ifo
    idx = read_maybe_gz(base, ".idx", [".idx.gz"])
    data = read_maybe_gz(base, ".dict", [".dict.dz", ".dict.gz"])
    sametypesequence = info.get("sametypesequence", "")

    words = []
    for word, offset, size in parse_idx(idx):
        text = entry_text(data[offset:offset + size], sametypesequence)
        if text:
            words.append((word, text.encode("utf-8")))
    words.sort(key=lambda w: stardict_key(w[0]))

    name = base.name
    out = args.out / name
    out.mkdir(parents=True, exist_ok=True)
    out_idx, out_dict, out_pos = bytearray(), bytearray(), bytearray()
    for word, text in words:
        out_pos += struct.pack("<I", len(out_idx))
        out_idx += word + b"\0" + struct.pack(">II", len(out_dict), len(text))
        out_dict += text
    (out / f"{name}.idx").write_bytes(out_idx)
    (out / f"{name}.dict").write_bytes(out_dict)
    (out / f"{name}.pos").write_bytes(out_pos)
    ifo = ["StarDict's dict ifo file", "version=2.4.2",
           f"bookname={info.get('bookname', name)}",
           f"wordcount={len(words)}",
           f"idxfilesize={len(out_idx)}",
           "sametypesequence=m",
           f"lang={args.lang}"]
    (out / f"{name}.ifo").write_text("\n".join(ifo) + "\n", encoding="utf-8", newline="\n")
    print(f"{name}: {len(words)} entries -> {out}")


if __name__ == "__main__":
    main()
