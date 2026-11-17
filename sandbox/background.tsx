/* The Figma shader in lib/ uses wider blob travel; this adapter supplies live controls. */
import React, {useEffect} from 'react';
import {createRoot} from 'react-dom/client';
import {ShaderFill, useWebGPUDevice, type Shader} from '@figma/expressive-components';
import {setup, render, manifest} from './lib/drifting-colour-fields';

const gradient = (stops: [number, number, number, number][]) => ({
  stops: stops.map(([position, r, g, b]) => ({position, color: {r, g, b, a: 1}}))
});
// Figma source: https://www.figma.com/design/nVvkRzyAbBU2QTXQGdhttv/motion?node-id=33-31
// Palette 1-3 are active; Palette 4 is retained below but is not in the cycle.
const colour = {
  palette: gradient([[0, .96, .93, .83], [.32, .88, .8, .44], [.65, .64, .43, .57], [1, .65, .78, .87]]),
  palette2: gradient([[0, .88, .39, .37], [.5, .72, .61, .81], [1, .98, .9, .68]]),
  palette3: gradient([[0, .76, .53, .22], [.5, .61, .4, .53], [1, .96, .93, .84]]),
  palette4: gradient([[0, 0, 0, 0], [.32, .52, .56, .3], [.65, .65, .79, .88], [1, .97, .94, .84]])
};
const themes = {
  colour: {...colour, activePaletteCount: 3},
  dark: {palette: gradient([[0, .015, .015, .018], [.32, .055, .055, .065], [.65, .22, .23, .25], [1, .56, .57, .6]]), activePaletteCount: 1},
  light: {palette: gradient([[0, 1, 1, .99], [.32, .96, .96, .95], [.65, .85, .85, .85], [1, .69, .7, .72]]), activePaletteCount: 1}
};

// One stable shader object keeps GPU state and integrated phases when volume changes.
function mount(host: HTMLElement, controls: any) {
  let painted = false, previousTime: number | undefined, clock = 0;
  const colourway = Number.isInteger(controls.colourway) && controls.colourway >= 0 && controls.colourway < 3
    ? controls.colourway : 0;
  const palettes = [colour.palette, colour.palette2, colour.palette3];
  // Rotate the three active Figma palettes by a whole step. Their stops and the
  // shader's independent per-blob transition phases stay unchanged.
  const selectedThemes = {...themes, colour: {...themes.colour,
    palette: palettes[colourway],
    palette2: palettes[(colourway + 1) % 3],
    palette3: palettes[(colourway + 2) % 3]
  }};
  const params = {blobCount: 7, softness: .72, intensity: 1, blobSize: 65,
    colourSpeed: 0, seed: 12, grainAmount: .16, grainSize: .8, motionSpeed: .18, paletteTransitionSpeed: .6};
  const shader: Shader = {
    setup, manifest,
    render(device, frame) {
      const dt = previousTime == null ? 0 : Math.max(0, Math.min(100, frame.time - previousTime));
      previousTime = frame.time;
      if (!document.hidden) clock += dt;
      render(device, {...frame, time: clock, params: {...params,
        ...selectedThemes[controls.theme as keyof typeof selectedThemes],
        motionSpeed: .18 * controls.motionScale,
        blowForce: controls.blowForce
      }});
      if (!painted) { painted = true; controls.setShaderStatus(true); }
    }
  };
  function Background() {
    const {error} = useWebGPUDevice();
    useEffect(() => { if (error) controls.setShaderStatus(false); }, [error]);
    return <ShaderFill className="poetry-shader" shader={shader}/>;
  }
  // Runtime validation/device-loss failures remove the canvas. Surface that limit.
  let sawCanvas = false;
  const observer = new MutationObserver(() => {
    if (host.querySelector('canvas')) sawCanvas = true;
    else if (sawCanvas) controls.setShaderStatus(false);
  });
  observer.observe(host, {childList: true});
  createRoot(host).render(<Background/>);
}

(window as any).PoetryBackground = {mount};
