"""PlatformIO post-build: package a complete ESP32 image using its real flash offsets."""
from pathlib import Path
import json
import shutil
import subprocess

Import("env")


def package_firmware(source, target, env):
    build = Path(env.subst("$BUILD_DIR"))
    destination = build / "firmware-package"
    destination.mkdir(exist_ok=True)
    segments = list(env.get("FLASH_EXTRA_IMAGES", []))
    segments.append((env.get("ESP32_APP_OFFSET", "0x10000"), str(build / (env.subst("${PROGNAME}") + ".bin"))))
    command = [env.subst("$PYTHONEXE"),
               str(Path(env.PioPlatform().get_package_dir("tool-esptoolpy")) / "esptool.py"),
               "--chip", env.BoardConfig().get("build.mcu", "esp32s3"), "merge_bin",
               "--flash_size", env.BoardConfig().get("upload.flash_size", "16MB"),
               "-o", str(destination / "firmware-merged.bin")]
    for offset, filename in segments:
        image = Path(env.subst(str(filename)))
        if not image.is_file():
            raise RuntimeError("Missing flash image: " + str(image))
        shutil.copy2(image, destination / image.name)
        command.extend([hex(offset) if isinstance(offset, int) else str(offset), str(image)])
    subprocess.run(command, check=True)
    (destination / "FLASHING.md").write_text(
        "# ESP32-S3 LCDwiki ES3C28P firmware\n\n"
        "For an existing installation of this project with its 16 MB partition layout, "
        "update only firmware.bin at 0x10000. This keeps the saved threshold and last colourway.\n\n"
        "Connect the board with a USB data cable, then run in a Python environment:\n\n"
        "```sh\npython -m pip install esptool==4.9.0\n"
        "python -m esptool --chip esp32s3 --port /dev/cu.YOUR_DEVICE "
        "write_flash 0x10000 firmware.bin\n```\n\n"
        "Replace the port with the board's serial port (COMx on Windows). "
        "If needed, hold BOOT while pressing/releasing RESET, then release BOOT and retry. "
        "For a complete installation, use the four separate images (this skips the "
        "NVS settings region):\n\n"
        "```sh\npython -m esptool --chip esp32s3 --port /dev/cu.YOUR_DEVICE write_flash "
        "0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin "
        "0x10000 firmware.bin\n```\n\n"
        "firmware-merged.bin is an alternative complete image at 0x0. Flashing it "
        "also writes its padded gaps and RESETS the saved threshold and last colourway. "
        "It is intended for first installation or an intentional settings reset.\n\n"
        "Tap the invisible bottom-right 44 x 44 corner for themes, microphone, "
        "and blow threshold. The menu closes after six seconds. Every restart opens "
        "Colour fields with a different starting colourway; Night/Paper last for "
        "the current session. See COLOURWAYS.md for the Figma palette source. Mic starts off. "
        "The microphone uses a reactive 10% threshold (adjustable 5-95%). "
        "Soft sounds quickly accelerate and spread the clouds and fragments, up to 9x speed. The "
        "first boot replaces the older saved threshold; later adjustments persist. "
        "Enable it and blow gently to spread the scene. Sound above the threshold "
        "for 0.06 seconds acts like a tap, revealing the poem. Release into quiet "
        "before another sound advances to the next moving poem. "
        "Reading keeps the poem/book title and author centred at the bottom. "
        "Database labels stay in the editor; only workTitle is displayed. "
        "Fragment and letter motion starts at 1.36x. Reading includes scattered "
        "and alternating phrase groups with varied near-edge and inset margins.\n",
        encoding="utf-8")
    project = Path(env.subst("$PROJECT_DIR"))
    shutil.copy2(project / "docs" / "COLOURWAYS.md", destination / "COLOURWAYS.md")
    for name in ("LICENSE", "NOTICE"):
        shutil.copy2(project / "third_party" / "es8311" / name,
                     destination / ("ES8311-" + name))
    font = json.loads((project / "data" / "font5x7.json").read_text(encoding="utf-8"))
    (destination / "FONT-LICENSE.txt").write_text(font["license"] + "\n", encoding="utf-8")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", package_firmware)
