"""Check a Literata system fontpack: required glyphs present, no Han.

Usage: python check_literata_pack.py <pack>   (exit 1 on failure)
"""
import struct
import sys


def keys(path):
    d = open(path, "rb").read()
    n = struct.unpack_from("<I", d, 8)[0]
    index = struct.unpack_from("<Q", d, 12)[0]
    return {struct.unpack_from("<Q", d, index + 16 * i)[0] for i in range(n)}


def main():
    k = keys(sys.argv[1])

    def has(cp, size, bpp):
        return ((cp << 32) | (size << 16) | bpp) in k

    roles = [(18, 2), (25, 2), (30, 2), (30, 4)]
    checks = {
        "space": all(has(0x20, s, b) for s, b in roles),
        "nbsp": all(has(0xA0, s, b) for s, b in roles),
        "ascii": all(has(c, s, b) for c in range(0x21, 0x7F) for s, b in roles),
        "vietnamese": all(has(c, s, b) for c in range(0x1EA0, 0x1EFA) for s, b in roles),
        "o/u horn": all(has(c, s, b) for c in (0x1A0, 0x1A1, 0x1AF, 0x1B0) for s, b in roles),
        "ellipsis": all(has(0x2026, s, b) for s, b in roles),
        # UI 符号：多选勾、翻页箭头、温度、比较符、增量
        "ui symbols": all(has(c, s, b) for c in (0x221A, 0x25B2, 0x25BC, 0xB0, 0x2264, 0x2265, 0x394)
                          for s, b in roles),
        "emoji 30@2": has(0x1F600, 30, 2),
        "math 28@2": has(0x03C0, 28, 2),
        "no Han": not any(has(0x4E2D, s, b) for s, b in roles),
    }
    for name, ok in checks.items():
        print(f"{'ok  ' if ok else 'FAIL'} {name}")
    print(f"items={len(k)}")
    sys.exit(0 if all(checks.values()) else 1)


if __name__ == "__main__":
    main()
