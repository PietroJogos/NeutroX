"""Lista caracteres nao-ASCII usados em literais de string do src/ que nao tem glifo na fonte propria.

Uso: python tools/glyph_check.py .
Sai com codigo 1 se algum caractere estiver faltando.
"""
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
font = (root / "src/graphics/font.cpp").read_text(encoding="utf-8")
have = set()
# glifos definidos: {'A', ...} ou {0x00B0, ...}
for m in re.finditer(r"^\s*\{(0x[0-9A-Fa-f]+|'(?:\\.|[^'])')\s*,", font, re.M):
    tok = m.group(1)
    if tok.startswith("0x"):
        have.add(int(tok, 16))
    else:
        ch = tok[1:-1]
        have.add(ord(ch[1]) if ch.startswith("\\") else ord(ch))
# acentos compostos: {0xE1, 'a', kAcute}
for m in re.finditer(r"\{(0x[0-9A-Fa-f]+),\s*(?:'.'|0x[0-9A-Fa-f]+),\s*k", font):
    have.add(int(m.group(1), 16))

string_re = re.compile(r'"((?:\\.|[^"\\])*)"')
missing = {}
for f in sorted(list((root / "src").rglob("*.cpp")) + list((root / "src").rglob("*.h"))):
    txt = f.read_text(encoding="utf-8")
    for lit in string_re.findall(txt):
        # decodifica escapes \xNN (bytes UTF-8 escritos como escape no codigo)
        if "\\x" in lit:
            try:
                raw = re.sub(r"\\x([0-9A-Fa-f]{2})", lambda m: chr(int(m.group(1), 16)), lit)
                lit = raw.encode("latin-1", "ignore").decode("utf-8", "ignore") if all(ord(c) < 256 for c in raw) else raw
            except UnicodeError:
                pass
        for ch in lit:
            if ord(ch) > 127 and ch != "\ufeff" and ord(ch) not in have:
                missing.setdefault(ch, set()).add(f.name)
for ch, files in sorted(missing.items()):
    print(repr(ch), hex(ord(ch)), sorted(files))
print("glifos conhecidos:", len(have), "| faltando:", len(missing))
sys.exit(1 if missing else 0)
