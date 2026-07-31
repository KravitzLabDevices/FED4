#!/usr/bin/env python3
"""Generate startup_sound.h from a WAV or MP3 source file.

Usage:
  python3 gen_startup_sound.py path/to/tts-audio.mp3
  python3 gen_startup_sound.py path/to/startup.wav

Requires: macOS afconvert (for MP3), or pass a 48 kHz mono 16-bit WAV.
"""

import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
OUT_HEADER = SCRIPT_DIR / "startup_sound.h"
SAMPLE_RATE = 48000


def to_wav(src: Path) -> Path:
    if src.suffix.lower() == ".wav":
        return src
    tmp = Path(tempfile.gettempdir()) / "fed4_startup.wav"
    subprocess.run(
        [
            "afconvert",
            "-f", "WAVE",
            "-d", f"LEI16@{SAMPLE_RATE}",
            "-c", "1",
            str(src),
            str(tmp),
        ],
        check=True,
    )
    return tmp


def emit_header(pcm: bytes, rate: int, out: Path) -> None:
    samples = len(pcm) // 2
    vals = struct.unpack("<" + "h" * samples, pcm)
    per_line = 12
    with out.open("w") as h:
        h.write("// Auto-generated — do not edit by hand.\n")
        h.write("// python3 sounds/gen_startup_sound.py <source.mp3|wav>\n\n")
        h.write("#pragma once\n\n")
        h.write("#include <Arduino.h>\n\n")
        h.write(f"static const uint32_t STARTUP_PCM_RATE = {rate};\n")
        h.write(f"static const size_t STARTUP_PCM_SAMPLES = {samples};\n\n")
        h.write("static const int16_t STARTUP_PCM[] PROGMEM = {\n")
        for i in range(0, samples, per_line):
            chunk = vals[i : i + per_line]
            h.write("  " + ", ".join(str(v) for v in chunk) + ",\n")
        h.write("};\n")
    print(f"Wrote {out} ({samples} samples, {len(pcm)} bytes, {samples / rate:.2f}s)")


def main() -> None:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <source.mp3|wav>", file=sys.stderr)
        sys.exit(1)
    src = Path(sys.argv[1]).resolve()
    wav = to_wav(src)
    with wave.open(str(wav), "rb") as w:
        ch, sw, rate, nframes, _, _ = w.getparams()
        if ch != 1 or sw != 2 or rate != SAMPLE_RATE:
            print(f"Expected 1ch 16-bit {SAMPLE_RATE} Hz, got {ch}ch {sw*8}-bit {rate} Hz")
            sys.exit(1)
        pcm = w.readframes(nframes)
    emit_header(pcm, rate, OUT_HEADER)


if __name__ == "__main__":
    main()
