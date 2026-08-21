#!/usr/bin/env python3
"""Generate local .clangd + compile_commands.json for Cursor/clangd (Arduino ESP32)."""
from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VARIANT = os.environ.get("FED4_CLANGD_VARIANT", "esp32s3")


def arduino15() -> Path:
    env = os.environ.get("ARDUINO15")
    if env:
        return Path(env)
    home = Path.home()
    if sys.platform == "darwin":
        return home / "Library" / "Arduino15"
    if sys.platform.startswith("win"):
        local = os.environ.get("LOCALAPPDATA", str(home / "AppData" / "Local"))
        return Path(local) / "Arduino15"
    return home / ".arduino15"


def latest_core(arduino: Path) -> Path:
    root = arduino / "packages" / "esp32" / "hardware" / "esp32"
    if not root.is_dir():
        raise SystemExit(f"error: ESP32 core not found at {root}")
    versions = sorted((p for p in root.iterdir() if p.is_dir()), key=lambda p: p.name)
    if not versions:
        raise SystemExit(f"error: no ESP32 core versions in {root}")
    return versions[-1]


# AVR/generic Arduino copies that shadow the ESP32 core (wrong SD.h, etc.)
SKIP_USER_LIBS = {
    "SD",
    "SPI",
    "Wire",
    "EEPROM",
    "SoftwareSerial",
    "USB",
    "FS",
    "Preferences",
    "Update",
}


def add_lib_dir(flags: list[str], lib_dir: Path) -> None:
    src = lib_dir / "src"
    if src.is_dir():
        flags.append(f"-I{src}")
    elif (lib_dir / "library.properties").is_file():
        flags.append(f"-I{lib_dir}")


def scan_libraries(flags: list[str], root: Path, skip: set[str] | None = None) -> None:
    if not root.is_dir():
        return
    skip = skip or set()
    for lib_dir in sorted(root.iterdir()):
        if lib_dir.is_dir() and lib_dir.name not in skip:
            add_lib_dir(flags, lib_dir)


def idf_include_flags(sdk: Path) -> list[str]:
    includes = sdk / "flags" / "includes"
    prefix = sdk / "include"
    flags: list[str] = [f"-I{sdk / 'qio_qspi' / 'include'}"]
    if not includes.is_file():
        return flags
    parts = includes.read_text().split()
    i = 0
    while i < len(parts):
        if parts[i] == "-iwithprefixbefore" and i + 1 < len(parts):
            flags.append(f"-I{prefix / parts[i + 1]}")
            i += 2
        else:
            i += 1
    return flags


def idf_defines(sdk: Path) -> list[str]:
    defines = sdk / "flags" / "defines"
    if not defines.is_file():
        return []
    return defines.read_text().split()


def toolchain_isystem(gcc: str) -> list[str]:
    try:
        proc = subprocess.run(
            [gcc, "-E", "-x", "c++", "-", "-v"],
            input="",
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return []
    flags: list[str] = []
    grab = False
    for line in (proc.stderr or "").splitlines():
        if line.startswith("#include <...> search starts here:"):
            grab = True
            continue
        if line.startswith("End of search list."):
            break
        if grab:
            flags.append(f"-isystem{line.strip()}")
    return flags


def compile_flags(core: Path, sdk: Path, gcc: str) -> list[str]:
    flags = [
        gcc,
        "-std=gnu++17",
        # Host clangd is not the Xtensa compiler; without this, xtensa/config/core.h
        # takes the #else branch and looks for ../hal.h next to the header.
        "-D__XTENSA__",
        "-D__XTENSA_EL__",
        "-D__XTENSA_WINDOWED_ABI__",
        "-DESP32",
        "-DESP_PLATFORM",
        "-DARDUINO=10819",
        "-DARDUINO_ARCH_ESP32",
        "-DARDUINO_ESP32S3_DEV",
        "-DARDUINO_USB_MODE=1",
        "-DARDUINO_USB_CDC_ON_BOOT=1",
        f"-I{core / 'cores' / 'esp32'}",
        f"-I{core / 'variants' / VARIANT}",
        f"-I{ROOT / 'src'}",
    ]
    flags.extend(idf_defines(sdk))
    flags.extend(idf_include_flags(sdk))
    flags.extend(toolchain_isystem(gcc))
    core_libs = core / "libraries"
    core_names = {p.name for p in core_libs.iterdir()} if core_libs.is_dir() else set()
    skip = core_names | SKIP_USER_LIBS
    scan_libraries(flags, core_libs)
    scan_libraries(flags, ROOT.parent, skip=skip)
    scan_libraries(flags, Path.home() / "Documents" / "Arduino" / "libraries", skip=skip)
    return flags


def source_files() -> list[Path]:
    files = sorted(ROOT.glob("src/*.cpp"))
    files.extend(sorted(ROOT.glob("examples/**/*.ino")))
    files.extend(sorted(ROOT.glob("examples/**/*.cpp")))
    return files


def write_clangd(flags: list[str]) -> None:
    del flags  # flags live in compile_commands.json
    (ROOT / ".clangd").write_text(
        "\n".join(
            [
                "# Local. Include paths are in compile_commands.json (also gitignored).",
                "CompileFlags:",
                "  CompilationDatabase: .",
                "---",
                "If:",
                "  PathMatch: .*\\.ino",
                "CompileFlags:",
                "  Add: [-xc++]",
                "",
            ]
        )
    )


def write_compile_commands(flags: list[str]) -> None:
    db = []
    for path in source_files():
        args = list(flags)
        if path.suffix == ".ino":
            args.insert(1, "-xc++")
        args.append(str(path))
        db.append({"directory": str(ROOT), "file": str(path), "arguments": args})
    (ROOT / "compile_commands.json").write_text(json.dumps(db, indent=2) + "\n")


def main() -> None:
    arduino = arduino15()
    core = latest_core(arduino)
    sdk = arduino / "packages" / "esp32" / "tools" / f"{VARIANT}-libs" / core.name
    gcc = arduino / "packages" / "esp32" / "tools" / "esp-x32"
    gcc_bins = sorted(gcc.glob("*/bin/xtensa-esp32s3-elf-g++")) if gcc.is_dir() else []
    compiler = str(gcc_bins[-1]) if gcc_bins else "clang++"
    if not sdk.is_dir():
        raise SystemExit(f"error: ESP32 libs not found at {sdk}")
    flags = compile_flags(core, sdk, compiler)
    write_clangd(flags)
    write_compile_commands(flags)
    print(
        f"Wrote {ROOT / '.clangd'} and compile_commands.json "
        f"(esp32 {core.name}, {len(source_files())} files, {len(flags)} flags)"
    )


if __name__ == "__main__":
    main()
