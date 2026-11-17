# Change the colourways in Figma

Open the [source gradient frame in motion](https://www.figma.com/design/nVvkRzyAbBU2QTXQGdhttv/motion?node-id=33-31).

The source is **code layers → Frame**, node **33:31**. Select that frame, open its **shader fill** in the right sidebar, and edit the gradient stops under **Palette 1**, **Palette 2**, and **Palette 3**. Keep each stop's position as well as its colour when syncing the design.

The frame's live shader properties were checked on 17 November 2026:

| Figma control | Property | Colours currently used |
| --- | --- | --- |
| Palette 1 | `palette` | Cream, gold, dusty pink, pale blue |
| Palette 2 | `palette2` | Coral, lilac, pale yellow |
| Palette 3 | `palette3` | Ochre, mauve, cream |

**Active palettes (first N)** is **3**. Palette 4 is present in the Figma fill but is inactive and is not part of the device's colourways.

## What happens on restart

The device always starts in **Colour fields**. Each boot chooses a different starting colourway from the previous boot, using these same three Figma palettes. The last choice is saved once per boot. Individual blobs continue changing colour at staggered times; changing the starting colourway does not introduce a whole-background swap.

Night and Paper remain available from the invisible bottom-right corner for the current session. Restarting returns to Colour fields. The blow threshold stays saved and the microphone starts off. Reloading the browser preview behaves like restarting the device; its last colourway is stored separately from the poetry library.

Remembering the last choice requires working device settings or browser storage. Clearing that storage starts a new random sequence.

## How Figma colours reach the app

Figma is the editing source. The app runs a checked-in copy of the palette values, so changing Figma does not immediately change an installed device or an already imported Code Layer.

After editing the Figma frame, ask to **sync Palette 1–3 from node 33:31 and rebuild the firmware**, or update these source locations together:

- Browser: the `colour` palette object in [background.tsx](https://github.com/samanthadotdesign/touch-test/blob/main/sandbox/background.tsx), then rebuild with `npm run build:background`.
- ESP32: `PALETTE_1`, `PALETTE_2`, and `PALETTE_3` in [atmosphere.cpp](https://github.com/samanthadotdesign/touch-test/blob/main/src/atmosphere.cpp), then build and install the firmware using [Firmware flashing](https://github.com/samanthadotdesign/touch-test/blob/main/docs/FIRMWARE_FLASHING.md).
- The shader module's palette defaults in [drifting-colour-fields.ts](https://github.com/samanthadotdesign/touch-test/blob/main/sandbox/lib/drifting-colour-fields.ts) are fallback values; the browser supplies the explicit palettes above.

Figma stop positions and RGB channels use the range 0–1. The firmware converts those channels into the screen's RGB565 format at drawing time. The palettes are compiled into the device and remain available offline.

An imported Figma Code Layer is a separate snapshot of the app. Reimport it after an app update if you want that embedded preview to match.
