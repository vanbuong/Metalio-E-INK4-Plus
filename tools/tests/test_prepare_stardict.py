"""Host test for tools/dict/prepare_stardict.py.

Run from the repository root: python tools/tests/test_prepare_stardict.py
Builds a tiny StarDict dictionary, prepares it, and checks the on-device file contract.
"""
import gzip
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/dict/prepare_stardict.py"


def write_source(folder: Path, name: str, entries, sametypesequence="h", gzip_dict=True, gzip_idx=False):
    """entries: list of (word, payload-bytes) in the given (possibly unsorted) order."""
    folder.mkdir(parents=True, exist_ok=True)
    data = b""
    idx = b""
    for word, payload in entries:
        idx += word.encode("utf-8") + b"\0" + struct.pack(">II", len(data), len(payload))
        data += payload
    (folder / f"{name}.dict.dz" if gzip_dict else folder / f"{name}.dict").write_bytes(
        gzip.compress(data) if gzip_dict else data)
    (folder / f"{name}.idx.gz" if gzip_idx else folder / f"{name}.idx").write_bytes(
        gzip.compress(idx) if gzip_idx else idx)
    ifo = ["StarDict's dict ifo file", "version=2.4.2", f"bookname={name} book",
           f"wordcount={len(entries)}", f"idxfilesize={len(idx)}"]
    if sametypesequence:
        ifo.append(f"sametypesequence={sametypesequence}")
    (folder / f"{name}.ifo").write_text("\n".join(ifo) + "\n", encoding="utf-8")
    return folder / f"{name}.ifo"


def read_prepared(folder: Path, name: str):
    ifo = dict(line.split("=", 1) for line in (folder / f"{name}.ifo").read_text(encoding="utf-8").splitlines()
               if "=" in line)
    idx = (folder / f"{name}.idx").read_bytes()
    data = (folder / f"{name}.dict").read_bytes()
    pos = (folder / f"{name}.pos").read_bytes()
    offsets = list(struct.unpack(f"<{len(pos) // 4}I", pos))
    entries = []
    for off in offsets:
        end = idx.index(b"\0", off)
        word = idx[off:end].decode("utf-8")
        d_off, d_size = struct.unpack(">II", idx[end + 1:end + 9])
        entries.append((word, data[d_off:d_off + d_size].decode("utf-8")))
    return ifo, entries, len(idx)


failures = 0


def check(cond, msg):
    global failures
    if not cond:
        print("CHECK failed:", msg)
        failures += 1


def run_tool(*args):
    return subprocess.run([sys.executable, str(TOOL), *map(str, args)], capture_output=True, text=True)


with tempfile.TemporaryDirectory(prefix="metalio-stardict-") as tmp:
    tmp = Path(tmp)

    # 1. HTML entries, unsorted, .dict.dz — the main contract.
    src = write_source(tmp / "src", "fruit", [
        ("run", b"<i>verb</i> move fast"),
        ("banana", b"yellow &amp; long"),
        ("apple", b"<b>fruit</b><br>red"),
        ("zebra", b"<div>animal</div><div>striped</div>"),
        ("Apple", b"company"),
    ])
    r = run_tool(src, "--lang", "en-vi", "--out", tmp / "out")
    check(r.returncode == 0, f"tool failed: {r.stderr}")
    ifo, entries, idx_size = read_prepared(tmp / "out/fruit", "fruit")
    check([w for w, _ in entries] == ["Apple", "apple", "banana", "run", "zebra"], f"order {entries}")
    check(dict(entries)["apple"] == "fruit\nred", repr(dict(entries)["apple"]))
    check(dict(entries)["banana"] == "yellow & long", repr(dict(entries)["banana"]))
    check(dict(entries)["zebra"] == "animal\nstriped", repr(dict(entries)["zebra"]))
    check(dict(entries)["run"] == "verb move fast", repr(dict(entries)["run"]))
    check(ifo.get("lang") == "en-vi", ifo)
    check(ifo.get("sametypesequence") == "m", ifo)
    check(ifo.get("wordcount") == "5", ifo)
    check(ifo.get("idxfilesize") == str(idx_size), ifo)
    check(ifo.get("bookname") == "fruit book", ifo)

    # 2. No sametypesequence: per-entry type prefixes; 't' (phonetic) dropped, 'm' kept.
    src2 = write_source(tmp / "src2", "plain", [
        ("cat", b"t" + "kæt".encode() + b"\0" + b"m" + b"small animal" + b"\0"),
        ("dog", b"m" + b"barks" + b"\0"),
    ], sametypesequence=None, gzip_dict=False, gzip_idx=True)
    r = run_tool(src2, "--lang", "en-en", "--out", tmp / "out")
    check(r.returncode == 0, f"tool failed: {r.stderr}")
    ifo2, entries2, _ = read_prepared(tmp / "out/plain", "plain")
    check(entries2 == [("cat", "small animal"), ("dog", "barks")], entries2)
    check(ifo2.get("lang") == "en-en", ifo2)

    # 3. Empty entries are dropped and wordcount follows.
    src3 = write_source(tmp / "src3", "gaps", [("a", b"<br>"), ("b", b"bee")])
    r = run_tool(src3, "--lang", "en-en", "--out", tmp / "out")
    ifo3, entries3, _ = read_prepared(tmp / "out/gaps", "gaps")
    check(entries3 == [("b", "bee")], entries3)
    check(ifo3.get("wordcount") == "1", ifo3)

    # 3a. Headwords over StarDict's 255-byte limit are dropped (the device reads at most 255).
    src6 = write_source(tmp / "src6", "long", [("x" * 256, b"too long"), ("ok", b"fine")])
    r = run_tool(src6, "--lang", "en-en", "--out", tmp / "out")
    ifo6, entries6, _ = read_prepared(tmp / "out/long", "long")
    check(entries6 == [("ok", "fine")], entries6)

    # 3b. Dictionary names containing dots keep their full name.
    src5 = write_source(tmp / "src5", "wn.3.0", [("a", b"letter")])
    r = run_tool(src5, "--lang", "en-en", "--out", tmp / "out")
    check(r.returncode == 0, f"dotted name failed: {r.stderr}")
    check((tmp / "out/wn.3.0/wn.3.0.pos").exists(), "dotted name output missing")

    # 4. 64-bit index offsets are rejected with a clear error.
    src4 = write_source(tmp / "src4", "big", [("a", b"x")])
    src4.write_text(src4.read_text(encoding="utf-8") + "idxoffsetbits=64\n", encoding="utf-8")
    r = run_tool(src4, "--lang", "en-en", "--out", tmp / "out")
    check(r.returncode != 0 and "idxoffsetbits" in r.stderr, (r.returncode, r.stderr))

if failures:
    print(f"{failures} check(s) failed")
    sys.exit(1)
print("prepare_stardict: PASS")
