# Update the ESP32 firmware

The target is the LCDWIKI ES3C28P ESP32-S3 with 16 MB flash and 8 MB octal PSRAM. Firmware includes the three atmosphere themes, hidden corner menu, microphone and blow threshold controls, and 0.66×–1.1× moving fragment scale.

## Build and upload from main

From your existing project folder on `main`:

```sh
git pull --ff-only origin main
./pio run --target upload
```

Connect the board with a USB data cable. To choose a port, run `./pio device list`, then append `--upload-port /dev/cu.YOUR_DEVICE` to the upload command. If the board will not connect, hold BOOT, press and release RESET, release BOOT, and retry.

## Use the GitHub build

The **[ESP32 firmware workflow](https://github.com/samanthadotdesign/touch-test/actions/workflows/firmware.yml)** runs on pushes to `main` and `codex/**` branches and can also be started with **Run workflow**. It checks the browser code and generated library, builds with PlatformIO on Ubuntu, and runs four native firmware suites before uploading an artifact.

Open a successful run for the `main` commit you want and download **esp32s3-firmware** from its artifacts. Extract the download. It contains `firmware-merged.bin`, the individual bootloader/partition/application binaries, `FLASHING.md`, and `COLOURWAYS.md`. Use the individual binaries for a normal update that keeps saved settings. The merged image is a convenient first-install image at address `0x0`, but its padded gaps overwrite the saved threshold and last colourway.

This Mac currently stops the downloaded ARM64 compiler with SIGKILL / PlatformIO Error −9. A successful GitHub build provides the binary without requiring that compiler to run locally. Workflow configuration alone does not establish a successful build: check the run status and commit before using its artifact.

From the project folder, install the flashing tool if needed:

```sh
.venv/bin/python -m pip install esptool==4.9.0
```

For a normal update of this project with the same 16 MB partition layout, flash only the application. Use the actual port and the path to the extracted file:

```sh
.venv/bin/python -m esptool --chip esp32s3 --port /dev/cu.YOUR_DEVICE write_flash 0x10000 /absolute/path/to/firmware.bin
```

To also refresh the bootloader and partition files while leaving saved settings untouched, flash the four separate binaries:

```sh
.venv/bin/python -m esptool --chip esp32s3 --port /dev/cu.YOUR_DEVICE write_flash \
  0x0 /absolute/path/to/bootloader.bin \
  0x8000 /absolute/path/to/partitions.bin \
  0xe000 /absolute/path/to/boot_app0.bin \
  0x10000 /absolute/path/to/firmware.bin
```

This leaves the saved-settings partition untouched when using the same partition layout. Do not run `erase_flash` for a normal update. The microphone starts off after restart.

For a first install, or when resetting saved settings is acceptable, the merged image uses one file:

```sh
.venv/bin/python -m esptool --chip esp32s3 --port /dev/cu.YOUR_DEVICE write_flash 0x0 /absolute/path/to/firmware-merged.bin
```

Flashing this merged image overwrites the saved threshold and last colourway even without `erase_flash`, because the image includes padding across that area. A different partition layout or an explicit full erase can also remove saved settings.

## After updating

Every restart opens Colour fields with a different starting colourway from the previous boot. The colours come from [Palette 1–3 on the Figma source frame](COLOURWAYS.md). Tap the display’s invisible bottom-right 44 × 44 corner. Try Night clouds, Paper clouds, and Colour fields for the current session; the sheet hides after six seconds. Restarting returns to Colour fields. Enable the mic, then adjust the threshold with **− / +** if needed. A gentle blow spreads moving colour and text. A sustained strong blow reveals the poem; a new breath or tap starts the next animated poem. The microphone needs staged setup and a short warmup before responding.

Check that Night is dark with white text and Paper is light with black text. The ES3C28P IPS panel needs the manufacturer's normal-colour inversion setting (`INVON`, `0x21`), applied directly to the screen even when drawing through the canvas. Earlier builds used the generic panel default, which can reverse dark/light themes and turn warm colours blue or cyan. The firmware now selects the IPS panel mode; palette values and RGB565 byte order stay the same. See the [manufacturer's initialization code](https://www.lcdwiki.com/res/ES3C28P/ILI9341V_Init.txt).

Build tests do not verify the physical microphone, display speed, touch coordinates, or flashing connection. No board has been flashed or physically verified as part of this update.
