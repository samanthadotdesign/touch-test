# ESP32 screen prototype

The screen-only companion to the [tuning studio](../sandbox/index.html). Open
the display through a local server from the repository root:

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

The reading body's layout can vary; the poem or book title and author remain
centred at the bottom, as in the [Figma references](https://www.figma.com/design/nVvkRzyAbBU2QTXQGdhttv/motion?node-id=68-1884).
Only `workTitle` appears on screen. The collection's `title` stays an editor
label; untitled quotes show only their author. Long work titles wrap upwards.

Tap the invisible bottom-right 44 × 44 corner to reveal the atmosphere sheet. Choose **Night clouds** (white text), **Paper clouds** (black text), or **Colour fields** (black text). The sheet hides after six seconds without interaction. Its **Enable mic** control requests microphone permission. While fragments are moving, blow gently to push them and the clouds outward from the centre; stronger sound moves them farther and faster. Enabling the mic does not interrupt reading. The studio also has **Blow strength preview** and a **Threshold** control, initially 10% and adjustable from 5% to 95%, for trying and tuning the response without a microphone. Sound above the threshold for 0.06 seconds acts like a tap: reveal the moving poem, then wait in reading. A new sound or tap starts the next animated poem. There is no automatic advance. Release into quiet for 0.5 seconds before another trigger, with at least 1.6 seconds between triggers, so one sustained sound only triggers once. Audio is measured locally and is never recorded, stored, or uploaded. Turning the mic off releases it.

The grainy drifting background uses the [Figma colour-field shader](https://www.figma.com/design/nVvkRzyAbBU2QTXQGdhttv/motion?node-id=33-31). Its motion has wider independent travel and an outward radial force while blowing. Edit the colour palettes in Figma, then manually refresh the sandbox snapshot; palette changes do not sync automatically. Rendering requires WebGPU and HTTPS or localhost. Unsupported browsers report the limitation; there is no substitute background animation. This interaction is a browser prototype and is not included in ESP32 firmware.

Both views use the same `sandbox/engine.js`, `sandbox/display.js`, device bitmap
font, and generated `sandbox/poetry-data.js`. The browser scales the display;
it does not emulate ESP32 hardware timing or touch electronics.

When served on the same origin, the prototype reads the studio's saved poetry
and animation library. Saving a poem or style in another open studio tab also
reloads the prototype's library and starts a fresh moving scene. Unsaved slider
changes remain a studio preview. The prototype never writes to the saved poetry library. It saves the last starting colourway under a separate browser storage entry. Each reload opens Colour fields with a different starting colourway; Night/Paper choices last for the current session.
For direct `file://` use, browser storage sharing depends on the browser; use
the local server for reliable sharing.

## Figma Code Layer

[Current device-screen Code Layer in the motion file](https://www.figma.com/design/nVvkRzyAbBU2QTXQGdhttv/motion?node-id=48-9&autoplay-code-preview=48-9).
Imported on 16 November 2026 onto the **code layers** page as
**Airplane Mode · Device screen**. The import includes the complete screen-only
runtime: grainy drifting backgrounds, the hidden atmosphere menu, radial breath
response, and the blow/tap poem sequence. Source import and configuration were
confirmed; live Figma playback and microphone permissions were not verified
because the browser requires a signed-in session. The local prototype and
tuning studio passed all 64 checks. The earlier 14 November Code Layer remains
at node `14:9`.

The checked-in `sandbox/background.bundle.js` includes the Figma shader runtime. After editing `sandbox/background.tsx` or the authored shader, install dependencies and rebuild before packaging:

```sh
npm ci
npm run build:background
```

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
