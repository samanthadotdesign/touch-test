# Airplane Mode · Poetry in motion

An offline poetry object for the LCDwiki ES3C28P ESP32-S3 touch display (240 × 320).

1. A random poem moves continuously as a mixture of whole words and individual letters, spread across the display in a random animation style.
2. Touch anywhere: the text settles into a randomly selected reading composition over 0.55 seconds.
3. Read for as long as you like.
4. Touch again: the letters separate into the moving field while a different poem and motion fade in over 0.65 seconds. The two poems overlap without a blank screen.

There are no Pomodoro timers, clocks, duration pickers, signatures, or automatic reading deadlines. Each new poem and animation excludes the immediately previous choice when the collection has more than one entry. A held touch only triggers one action; touches during transitions are ignored.

## Try it without the board

Serve the repository root for the full background and microphone preview:

```sh
python3 -m http.server 8765 --bind 127.0.0.1
```

Visit http://127.0.0.1:8765/sandbox/. Click/tap the display, or focus it and press Enter or Space. The Motion studio provides eight families: Orbit, Helix, Wave, Vortex, Drift, Tumble, Cloud, and Ribbon. Mixed words and letters are the default motion unit. Each scene randomly chooses which words stay together and which split into letters. The field uses independent speeds, directions, depth phases, and rotations for every fragment. Clicking a motion preset again creates a fresh scene. Choose **Animate as → Individual letters** or **Whole words** for an unmixed field. The moving scene contains at least 180 particles, using faint echoes of the poem to cover the display; echoes converge and fade during assembly, leaving one exact copy of the poem. All presets and saved variations run at twice the original motion tempo. The Speed control shows the effective multiplier. Adjust speed, spread, depth, 3D tilt, spin, and camera perspective; give a variation a name and **Save style** to include it in the random rotation. Changing controls restarts the moving preview. **Reset** restores the selected saved preset.

The Poetry library lets you select, edit, and add poems or quotes with an attribution. The collection stores the poem or quote text together with its attribution. Explicit line breaks are preserved and long lines wrap to a compact 168 px measure. **Random** chooses a different eligible reading composition for every scene. Twelve options include centered verse, left alignment, diagonal steps, reverse steps, zigzag steps, scattered constellations, word cascades, stanza islands, mirrored islands, three islands, wavy lines, and twin columns. The selected composition stays fixed while reading. Use **Shuffle layout** to audition another arrangement of the current poem, or choose a fixed layout and save the poem. Spatial layouts rearrange whole words in reading order; verse layouts retain explicit line breaks. The body now uses bitmap font size 1 (6 px advance, 8 px height), half the previous default size, with generous surrounding space. Words longer than 28 characters are rejected. Unrenderable content is rejected rather than truncated. The current device font supports printable ASCII glyphs plus display aliases for smart quotes, dashes, nonbreaking spaces, and ellipses. The source library retains its original Unicode punctuation, up to 512 characters / 96 words per poem, and authors up to 35 characters.

Saved poems/styles persist in this browser's local storage. Library edits are local to the browser until you export them. The main poetry collection initially loads from `data/poetry.json`, through the generated browser data; stale browser-saved demo poems cannot hide a newer database. Use **Library files & device setup → Export library** for a portable JSON backup. Import validates the entire file before replacing the browser collection. Export before switching browsers or clearing browser data. Changing the active preset or its sliders alone does not overwrite saved styles.

## Backgrounds and sound response

The browser sandbox adds soft, grainy drifting clouds from the [Figma colour-field reference](https://www.figma.com/design/nVvkRzyAbBU2QTXQGdhttv/motion?node-id=33-31). Blob colours evolve at staggered phases instead of swapping the whole background. Tap the display's invisible bottom-right 44 × 44 corner to open the atmosphere sheet. It offers **Night clouds** with white text, **Paper clouds** with black text, and **Colour fields** with black text. The sheet hides after six seconds without interaction. The corner remains visually hidden; it can also receive keyboard focus. Theme preference is stored separately from the poetry library.

Use **Enable mic** in the studio or atmosphere sheet, then blow gently to push the colours and word fragments outward from the centre while they are moving. Stronger blowing increases their movement. Enabling the mic does not interrupt a poem that is being read. The browser asks for microphone permission. Sound is measured locally; it is not recorded, stored, or uploaded. **Blow strength preview** auditions the response without microphone access. The studio’s **Threshold** control defaults to 75%. A strong blow held above the threshold for 0.35 seconds acts like a tap: it reveals the moving poem. Reading then waits for a new strong blow or tap, which starts the next animated poem. There is no automatic advance. Lower the level below 25% for 0.5 seconds to rearm, with at least 1.6 seconds between triggers, so one sustained breath cannot skip through poems. Turn the mic off to release it.

The Figma shader is in `sandbox/lib/drifting-colour-fields.ts` and rendered through `@figma/expressive-components`, with wider independent paths that carry each blob across the screen and a radial force that pushes colour outward while blowing. The colour palettes remain managed in Figma: edit **Palette 1–3** in the reference shader settings and manually refresh the sandbox snapshot after changes. The preview does not automatically sync with Figma. It requires WebGPU on HTTPS or localhost. If the browser cannot render it, the sandbox reports that limitation; the shader is not replaced with an imitation. The checked-in browser bundle lets the preview run without installing development tools. After changing the background source, rebuild it from this directory:

```sh
npm ci
npm run build:background
```

These backgrounds, the atmosphere sheet, and microphone response are browser prototypes. They are not included in the ESP32 firmware.

## Screen-only prototyping sandbox

Open [prototype/index.html](prototype/index.html), visit
http://127.0.0.1:8765/prototype/ with the server above, or use **Open device
prototype ↗** in the studio. This view shows only the 240 × 320 display, scaled
to fit, and uses the same engine, renderer, font, and library as the tuning
studio. Tap to read; tap again for a new scene. Saved studio poems and styles
also load here when both views share an origin; unsaved controls remain a studio
preview. The screen-only view never writes to your saved poetry library; atmosphere preferences use a separate storage entry. It includes the same hidden corner menu and sound response.

Create a self-contained Figma Code Layer archive with
`python3 scripts/package_prototype.py --output /tmp/esp32-prototype.tgz`.
See [prototype/README.md](prototype/README.md) for the source and browser-library
workflow. Figma imports a snapshot; later Git or studio changes need a new import.

## Put your collection on the ESP32

`data/poetry.json` is the shared source database. Replace it with your exported `poetry.json` (or edit it directly), then run:

```sh
python3 scripts/generate_poetry.py
./pio run
./pio run --target upload
```

The generator validates IDs, text, layout, and motion bounds, emits a source revision, and produces `src/poetry_data.h` and `sandbox/poetry-data.js`. Refresh the sandbox to load regenerated data; the source revision updates the browser collection, replacing stale source copies and unused demo poems while keeping custom poems and animation styles. The previous browser library is backed up before migration. Saved edits for the same source revision continue to load normally. The firmware database is compiled into flash and needs no network connection or external database service.

## Motion and perspective

The mixed field combines independently transformed letters and intact words, with additional faint echoes. Choose a **Distribution** independently of the motion family: **Condensed cloud** packs letters and snippets into a dense knot with a sparse diagonal plume; **Typographic stream** creates a tall winding trail with denser knots of words and side branches; **Fragment islands** creates scattered patches of broken lines. These distributions follow the supplied image references rather than spreading fragments evenly across the screen. A fresh scene seed varies the cluster details, local rates, directions, depth, and fragment selection; there is no shared grid. Randomness stays fixed for that scene, so trajectories remain continuous without frame-by-frame jitter. Perspective-aware bounds keep the field across the display while style-specific motion adds depth and rotation. Echoes include individual letters, partial words, and short phrases up to 14 characters, without crossing source line breaks. Moving fragments vary from 0.55× to 1.1× type scale before perspective is applied. Scale settles smoothly to 1× during assembly; echoes are hidden in the settled state, so every poem character appears exactly once. Both implementations use the same eight pose equations, then rotate each bitmap glyph on X, Y, and Z and project it through a camera onto the screen. All particles from both poems share one depth sort during transitions. Far letters are dark gray, near letters are light gray, and the settled poem uses a soft white. Depth shading spans 12–96% brightness rather than the previous narrower range. A lower camera distance creates stronger perspective. Glyph pixels are projected quadrilaterals. Each fragment has its own small assembly delay and duration within the 0.55-second transition. Quintic easing carries the continuously moving field into the reading layout and settles each fragment with zero velocity and acceleration. Separation is staggered within the 0.65-second scene change too. On the second touch, the next poem is chosen immediately and both fields crossfade; the incoming motion clock is preserved at handoff.

The visual direction draws on the supplied [Figma moodboard](https://www.figma.com/design/nVvkRzyAbBU2QTXQGdhttv/motion?node-id=4-228): sparse individual glyphs, spherical clouds, curved ribbons, and compact typesetting. No moodboard images are used as interface assets.

The browser uses the same 5 × 7 font bitmap and motion/layout formulas as the firmware. It recreates behavior; it does not execute the ESP32 binary or predict device frame rate. Firmware aims for 30 FPS but projected text performance must be checked on the board. Browser rendering can appear smoother. The browser-only atmosphere preview adds optional microphone input; the firmware does not use audio or microphone input.

New variations within a family need only a new animation entry in the JSON. `kind` selects 0 Orbit, 1 Helix, 2 Wave, 3 Vortex, 4 Drift, 5 Tumble, 6 Cloud, or 7 Ribbon. Each animation has `unit: "mixed"`, `"letters"`, or `"words"`; poems accept `layout: "random"`, `"centered"`, `"left"`, `"stepped"`, `"constellation"`, `"cascade"`, `"split"`, `"wave"`, `"columns"`, `"stepped-reverse"`, `"stepped-zigzag"`, `"split-mirrored"`, or `"split-three"`. Version 4 exports also include `formation: "cluster"`, `"stream"`, or `"patches"`, which can be combined with any motion family. Missing formation values in older libraries default according to the motion family. Version 4 exports include these fields; missing legacy fields default to mixed/random, while explicit imported choices are preserved. Random selection considers only layouts that fit the poem and excludes the previous composition when another fits. Specific layouts that cannot fit are rejected rather than dropping words. `speed` is a time multiplier; `spread` and `depth` are pixels; `tilt` is radians; `spin` is a style-specific rotation multiplier; `perspective` is camera distance in pixels. To add an entirely new movement equation, extend `src/poetry_motion.h`, `sandbox/engine.js`, and the kind validators/UI. Word-fragment motion uses these pose equations. The separate browser background runs the authored Figma colour-field shader.

## Build and upload tools

PlatformIO 6.2.0 is installed in `.venv`. The `./pio` wrapper stores board packages and toolchains in `.platformio`.

```sh
./pio run                     # Build without the board
./pio device list             # List serial ports
./pio run --target upload     # Connect the ESP32 with a USB data cable
./pio device monitor          # Serial log at 115200 baud
```

For an explicit port, append `--upload-port /dev/cu.YOUR_DEVICE` to the upload command. Native USB CDC is enabled. If necessary, hold BOOT while pressing and releasing RESET, then release BOOT and retry.

Dependencies: Espressif32 6.12.0, ESP32-S3 toolchains, Arduino framework, GFX Library for Arduino 1.4.9, Wire, and esptool 4.9.0. Board settings use 16 MB flash and 8 MB octal PSRAM. To reinstall:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install platformio==6.2.0
./pio run
```

On this Mac, macOS code-trust enforcement terminates the downloaded 2022 ARM64 compiler before execution (SIGKILL, PlatformIO Error -9). Unified logs report a CT signature issue. PlatformIO and esptool run, but an actual ESP32 build/upload remains unverified. No OS trust or security settings were changed.

## Validation

```sh
node --test test/*.test.cjs
python3 scripts/generate_poetry.py --check
```

See `test/README` for native C++ tests that compile the production firmware state machine and renderer using host stubs. These verify behavior without an ESP32 and do not replace an ESP32 build or hardware test.

Font attribution and BSD license are in `data/font5x7.json` and the generated browser font data.
