# ESP32 screen prototype

The screen-only companion to the [tuning studio](../sandbox/index.html). Open
`index.html` directly, or serve the repository root:

```sh
python3 -m http.server 8765 --bind 127.0.0.1
```

Visit `http://127.0.0.1:8765/prototype/` for the display and
`http://127.0.0.1:8765/sandbox/` for the studio.

The only interface is the 240 × 320 LCD, scaled to fit the viewport without
cropping or stretching. A 240 × 320 Figma Code Layer shows it at native size.
There is no bezel, toolbar, status label, or tuning panel. Tap/click the display
to settle the moving fragments into a poem; tap again for another poem and
motion. Enter and Space work when the display has focus. Holding a touch or
key triggers one action. Reading has no timeout.

Both views use the same `sandbox/engine.js`, `sandbox/display.js`, device bitmap
font, and generated `sandbox/poetry-data.js`. The browser scales the display;
it does not emulate ESP32 hardware timing or touch electronics.

When served on the same origin, the prototype reads the studio's saved poetry
and animation library. Saving a poem or style in another open studio tab also
reloads the prototype's library and starts a fresh moving scene. Unsaved slider
changes remain a studio preview. The prototype never writes to the saved library.
For direct `file://` use, browser storage sharing depends on the browser; use
the local server for reliable sharing.

## Figma Code Layer

[Imported Code Layer in the motion file](https://www.figma.com/design/nVvkRzyAbBU2QTXQGdhttv/motion?node-id=14-9).
The source was imported on 14 November 2026 from the local
`codex/esp32-screen-prototype` working tree. Browser verification in Figma needs
a signed-in session. The local prototype and tuning studio are verified.

Package the prototype with its current shared dependencies:

```sh
python3 scripts/package_prototype.py --output /tmp/esp32-prototype.tgz
```

The archive opens the screen-only page at its root. It contains a snapshot of
the repository's generated library, not browser-local edits. To include studio
edits, export the library, import it with `scripts/generate_poetry.py --input`,
then package again. Reimport the updated archive into Figma after source changes;
the Code Layer is a snapshot and does not automatically follow a Git branch or
share local browser storage.
