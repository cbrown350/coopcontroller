"""
Pre-build workaround for a pioarduino platform-espressif32 race bug.

The pioarduino platform's component manager (builder/frameworks/arduino.py)
attaches a `restore_pioarduino_build_py` post-action to the `checkprogsize`
target unconditionally, but only *creates* the backup file that restore reads
when LTO or picolibc is configured. In a normal (non-LTO) build no backup is
created, yet the restore still runs: it is guarded by `if os.path.exists`, so
it usually no-ops, but the restore's `os.remove(backup_path)` deletes the
backup from a prior build, so the next build's restore finds it gone and
SCons intermittently surfaces `*** [checkprogsize] pioarduino-build.py.esp32:
No such file or directory`, failing the build.

Fix: before each build, ensure the per-MCU backup file exists by copying the
canonical build script to it. The restore then always finds the backup and
recopies it (a no-op since the script is never patched without LTO/picolibc).
This is idempotent and safe — the backup is a byte-identical copy.
"""
import os
import shutil
from pathlib import Path

Import("env")  # type: ignore # noqa: F821 - provided by PlatformIO SCons

libs_mcu = None
try:
    platform = env.PioPlatform()  # type: ignore
    libs_dir = platform.get_package_dir("framework-arduinoespressif32-libs")
    mcu = env.get("BOARD_MCU") or "esp32"  # type: ignore
    if libs_dir:
        libs_mcu = str(Path(libs_dir) / mcu)
except Exception:
    libs_mcu = None

if not libs_mcu:
    candidates = [
        Path.home() / ".platformio" / "packages" / "framework-arduinoespressif32-libs" / "esp32",
    ]
    for c in candidates:
        if (c / "pioarduino-build.py").exists():
            libs_mcu = str(c)
            break

if libs_mcu:
    build_py = Path(libs_mcu) / "pioarduino-build.py"
    backup = Path(libs_mcu) / "pioarduino-build.py.esp32"
    if build_py.exists() and not backup.exists():
        try:
            shutil.copy2(build_py, backup)
        except Exception:
            pass