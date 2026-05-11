"""
Post-build script PlatformIO : copie automatique du firmware dans binaires/

Déclenché après chaque compilation réussie (pio run, IDE, upload…).
Le dossier binaires/ est gitignored et sert de source pour le téléchargement
OTA via l'interface web (accès navigateur).
"""

import shutil
import os

Import("env")  # noqa: F821  (injecté par PlatformIO)


def copy_firmware_to_binaires(source, target, env):
    build_dir    = env.subst("$BUILD_DIR")
    project_dir  = env.subst("$PROJECT_DIR")
    version      = env.GetProjectOption("build_flags", "")

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


env.AddPostAction("$BUILD_DIR/firmware.bin", copy_firmware_to_binaires)
