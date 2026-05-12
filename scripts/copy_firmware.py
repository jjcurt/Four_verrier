"""
Post-build script PlatformIO : copie automatique du firmware dans binaires/

Déclenché après chaque compilation réussie (pio run, IDE, upload…).
Le dossier binaires/ est gitignored et sert de source pour le téléchargement
OTA via l'interface web (accès navigateur).
"""

import shutil
import os
import re

Import("env")  # noqa: F821  (injecté par PlatformIO)


def _read_firmware_version(project_dir):
    config_path = os.path.join(project_dir, "include", "config.h")
    try:
        with open(config_path) as f:
            for line in f:
                m = re.search(r'#define FIRMWARE_VERSION "([^"]+)"', line)
                if m:
                    return m.group(1)
    except OSError:
        pass
    return None


def copy_firmware_to_binaires(source, target, env):
    build_dir    = env.subst("$BUILD_DIR")
    project_dir  = env.subst("$PROJECT_DIR")

    firmware_src = os.path.join(build_dir, "firmware.bin")
    binaires_dir = os.path.join(project_dir, "binaires")

    if not os.path.exists(firmware_src):
        print("[copy_firmware] WARN: firmware.bin introuvable, copie ignorée")
        return

    os.makedirs(binaires_dir, exist_ok=True)

    # Toujours mettre à jour firmware.bin (utilisé par l'interface OTA)
    dest = os.path.join(binaires_dir, "firmware.bin")
    shutil.copy2(firmware_src, dest)
    size_kb = os.path.getsize(dest) // 1024
    print(f"[copy_firmware] → binaires/firmware.bin ({size_kb} Ko)")

    # Copie versionnée (archive du build courant)
    version = _read_firmware_version(project_dir)
    if version:
        versioned_name = f"Four_Verrier_v{version}.bin"
        shutil.copy2(firmware_src, os.path.join(binaires_dir, versioned_name))
        print(f"[copy_firmware] → binaires/{versioned_name}")


env.AddPostAction("$BUILD_DIR/firmware.bin", copy_firmware_to_binaires)
