# Boot startup sound (embedded PCM)

`startup_sound.h` holds 48 kHz mono 16-bit PCM played once at boot.

## Regenerate from a new clip

```bash
python3 sounds/gen_startup_sound.py /path/to/your-audio.mp3
```

Requires macOS `afconvert` for MP3 input, or pass a WAV already at 48 kHz mono 16-bit LE.

## Flash cost

~96 KB per second of audio (~2.5 s ≈ 232 KB for the current clip).
