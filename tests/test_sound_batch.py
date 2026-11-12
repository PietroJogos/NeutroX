"""Regressões do lote com ZIPs e WAVs sintéticos."""
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import wave
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def pcm(amplitude):
    stream = io.BytesIO()
    with wave.open(stream, "wb") as wav:
        wav.setparams((1, 2, 44100, 0, "NONE", ""))
        wav.writeframes(struct.pack("<32h", *([amplitude, -amplitude] * 16)))
    return stream.getvalue()


class BatchTests(unittest.TestCase):
    def setUp(self):
        build = ROOT / "build"
        build.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=build)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.dest = self.root / "local_audio"

    def zip_with(self, label, members):
        path = self.root / label
        with zipfile.ZipFile(path, "w") as z:
            for name, data in members.items():
                z.writestr(name, data)
        return path

    def run_batch(self, *args):
        cmd = [sys.executable, "-B", "-X", "utf8", str(ROOT / "tools/sound_extract.py"),
               "--destino", str(self.dest), *map(str, args)]
        result = subprocess.run(cmd, capture_output=True, encoding="utf-8", timeout=90)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads((self.dest / "manifesto.json").read_text(encoding="utf-8"))

    def test_duplicate_and_conflicting_versions(self):
        a = self.zip_with("a.zip", {"ui/btn_focus.wav": pcm(1000), "ui/btn_select.wav": pcm(2000)})
        b = self.zip_with("b.zip", {"btn_focus.wav": pcm(1000), "btn_select.wav": pcm(3000)})
        result = self.run_batch("--recursos", f"6717={a}", "--recursos", f"6717={b}",
                                "--recursos", f"9199={a}")
        blades, nxe = result["versoes"][:2]
        self.assertEqual(len(blades["arquivos"]), 3)
        self.assertEqual(len(nxe["arquivos"]), 2)
        self.assertIsNotNone(blades["efeitos"]["navegar"])
        self.assertIsNone(blades["efeitos"]["confirmar"])
        self.assertIsNotNone(nxe["efeitos"]["confirmar"])
        self.assertEqual(len(next(x for x in blades["arquivos"] if x["usado_para"])["proveniencia"]), 2)
        self.assertEqual(len(list(self.dest.glob("*/ausentes.ini"))), 5)

    def test_lua_playback_call(self):
        sys.path.insert(0, str(ROOT / "tools"))
        from audio_lua import audio_calls
        pack = lambda *numbers: struct.pack(f">{len(numbers)}I", *numbers)
        names = b"".join(b"\x04" + pack(len(name) + 1) + name + b"\x00"
                         for name in (b"PlaySound", b"btn_select"))
        code = pack(5, 1 | 1 << 6 | 1 << 14, 28 | 2 << 23 | 1 << 14, 30 | 1 << 23)
        data = (b"\x1bLuaQ\x00\x00\x04\x04\x04\x08\x00" + pack(0, 1, 2) + b"\x00" * 4 +
                pack(4) + code + pack(2) + names + pack(0, 0, 0, 0))
        self.assertEqual(audio_calls(data), [("0", 1, 2, "PlaySound", ["btn_select"])])
        self.assertEqual(audio_calls(data[:-1]), [])

    def test_origin_and_path_escape(self):
        a = self.zip_with("a.zip", {"../btn_back.wav": pcm(1000), "btn_select.wav": pcm(1000)})
        result = self.run_batch("--entrada", a)
        self.assertFalse(any(r["arquivos"] for r in result["versoes"]))
        self.assertTrue(any("nome inválido" in n for n in result["inventario"]))
        self.assertTrue(any("origem indeterminada" in n for n in result["inventario"]))


if __name__ == "__main__":
    unittest.main()
