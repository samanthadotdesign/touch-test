// @ts-nocheck

// Property metadata is only used in Figma Design, so this is a no-op.
function defineProperties(
  _component: unknown,
  _properties: unknown,
): void {}

export default function Effect() { }
export function setup(device, frame) {
    var wgsl = `
diagnostic(off,derivative_uniformity);
struct Palette {
  colors: array<vec4f, 8>,
  positions: array<vec4f, 2>,
  count: vec4f,
};
struct Uniforms {
  frameData: vec4f,
  inputDimsData: vec4f,
  palettes: array<Palette, 4>,
  motionSpeed: vec4f,
  colourSpeed: vec4f,
  blobSize: vec4f,
  blobCount: vec4f,
  softness: vec4f,
  grainAmount: vec4f,
  grainSize: vec4f,
  intensity: vec4f,
  seed: vec4f,
  paletteData: vec4f,
  blowData: vec4f,
};
@group(0) @binding(0) var<uniform> u: Uniforms;

struct VsIn {
  @location(0) pos: vec2f,
  @location(1) uv: vec2f,
};
struct VsOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

fn paletteStopPos(paletteIndex: u32, i: u32) -> f32 {
  let lane = i & 3u;
  return u.palettes[paletteIndex].positions[i / 4u][lane];
}

fn paletteRamp(paletteIndex: u32, t: f32) -> vec4f {
  var colors = u.palettes[paletteIndex].colors;
  let n = u32(clamp(u.palettes[paletteIndex].count.x, 0.0, 8.0));
  if (n == 0u) { return vec4f(0.0); }
  if (n == 1u || t <= paletteStopPos(paletteIndex, 0u)) { return colors[0]; }
  let last = n - 1u;
  if (t >= paletteStopPos(paletteIndex, last)) { return colors[last]; }
  for (var i = 0u; i < 7u; i = i + 1u) {
    if (i >= last) { break; }
    let p1 = paletteStopPos(paletteIndex, i + 1u);
    if (t <= p1) {
      let p0 = paletteStopPos(paletteIndex, i);
      return mix(colors[i], colors[i + 1u], (t - p0) / max(p1 - p0, 1e-5));
    }
  }
  return colors[last];
}

fn evolvingPalette(t: f32, phase: f32, rate: f32) -> vec3f {
  let count = u32(clamp(u.paletteData.x, 1.0, 4.0));
  if (count == 1u) { return paletteRamp(0u, t).rgb; }
  let progress = u.paletteData.y * rate + phase;
  let cycle = progress - floor(progress / f32(count)) * f32(count);
  let current = min(u32(floor(cycle)), count - 1u);
  let next = (current + 1u) % count;
  // Zero-slope endpoints make the last-to-first wrap as soft as every other step.
  let blend = smoothstep(0.0, 1.0, fract(cycle));
  return mix(paletteRamp(current, t).rgb, paletteRamp(next, t).rgb, blend);
}

fn fieldHash(p: vec2f) -> f32 {

  let q = fract(p * vec2f(123.34, 456.21));

  let r = q + dot(q, q + 45.32);

  return fract(r.x * r.y);

}

fn fieldNoise(p: vec2f) -> f32 {

  let cell = floor(p);

  let f = fract(p);

  let s = f * f * (3.0 - 2.0 * f);

  return mix(mix(fieldHash(cell), fieldHash(cell + vec2f(1.0, 0.0)), s.x),

    mix(fieldHash(cell + vec2f(0.0, 1.0)), fieldHash(cell + vec2f(1.0, 1.0)), s.x), s.y);

}

@vertex fn vs_main(in: VsIn) -> VsOut {
  var out: VsOut;
  out.position = vec4f(in.pos, 0.0, 1.0);
  out.uv = in.uv;
  return out;
}

@fragment fn fs_main(@location(0) outputUv_in: vec2f) -> @location(0) vec4f {
  let outputUv = outputUv_in;
  let dims = max(u.frameData.yz, vec2f(1.0));
  let inputDims = max(u.inputDimsData.xy, vec2f(1.0));
  let inputTexel = vec2f(1.0) / inputDims;
  let uv = outputUv;
  // texel and aspect are input-relative to match uv. Zoom fills rescale texel
  // alongside their uv redefinition below (see zoomLocal).
  let texel = inputTexel;
  let aspect = inputDims.x / max(inputDims.y, 1.0);
  let blobSize = u.blobSize.x;
  let blobCount = u.blobCount.x;
  let softness = u.softness.x;
  let grainAmount = u.grainAmount.x;
  let grainSize = u.grainSize.x;
  let intensity = u.intensity.x;
  let seed = u.seed.x;
  let blowForce = clamp(u.blowData.x, 0.0, 1.0);
  let metric = dims / max(min(dims.x, dims.y), 1.0);
  let p = (uv - 0.5) * metric;
  // Independent accumulated phases, not absolute time multiplied by current speed.
  // The CPU integrates in seconds; zero speed holds the current phase exactly.
  let drift = u.motionSpeed.x;
  let evolution = u.colourSpeed.x;
  let variation = floor(seed + 0.5);
  let warp = vec2f(
    fieldNoise(p * 1.8 + vec2f(variation * 0.17, drift * 0.15)),
    fieldNoise(p * 1.8 + vec2f(8.3 + drift * 0.12, variation * 0.23))
  ) - 0.5;
  let organic = p + warp * 0.26;
  let radius = max(blobSize * 0.01, 0.05);
  let falloff = mix(8.0, 2.7, softness);
  var weighted = vec3f(0.0);
  var total = 0.0;
  for (var i = 0; i < 16; i = i + 1) {
    if (f32(i) >= floor(blobCount + 0.5)) { break; }
    let index = f32(i);
    let r1 = fieldHash(vec2f(index + 0.7, variation + 1.0));
    let r2 = fieldHash(vec2f(index + 8.2, variation + 2.9));
    let r3 = fieldHash(vec2f(index + 3.1, variation + 6.1));
    let phase = r3 * 6.2831853;
    // Every blob travels across the field on its own smooth path. Scale travel
    // to the screen, independently of blob size; there is no fixed anchor.
    let travel = vec2f(
      sin(drift * (0.38 + r1 * 0.22) + phase) + 0.2 * sin(drift * (0.19 + r3 * 0.08) + r2 * 9.0),
      cos(drift * (0.34 + r2 * 0.24) + phase * 1.37) + 0.2 * sin(drift * (0.23 + r1 * 0.07) + r1 * 8.0)
    );
    let restingCentre = travel * (0.46 / 1.2);
    let outwardVector = restingCentre + vec2f(cos(phase), sin(phase)) * 0.02;
    let outward = outwardVector / max(length(outwardVector), 0.001);
    // Breath pressure parts the colour field from the middle, independently
    // for every blob; the grain and staggered colour clocks stay unchanged.
    let centre = (restingCentre + outward * blowForce * 0.34) * metric;
    let delta = organic - centre;
    let localRadius = radius * mix(0.73, 1.18, r3);
    let weight = exp(-dot(delta, delta) * falloff / (localRadius * localRadius));
    let palettePhase = evolution * (0.57 + r1 * 0.76) + phase + index * 1.91;
    let palettePosition = 0.5 + 0.5 * sin(palettePhase + 0.28 * sin(palettePhase * 0.43 + r2 * 6.0));
    weighted += evolvingPalette(palettePosition, r2 * u.paletteData.x + index * 0.381966, 0.88 + r3 * 0.24) * weight;
    total += weight;
  }
  let baseColour = evolvingPalette(0.12, 0.0, 1.0);
  let colour = (weighted + baseColour * 0.012) / (total + 0.012);
  let luminance = dot(colour, vec3f(0.2126, 0.7152, 0.0722));
  let graded = mix(vec3f(luminance), colour, intensity);
  let grainCoordinate = uv * dims / max(grainSize, 0.1);
  let grain = (fieldNoise(grainCoordinate * 1.7 + vec2f(variation * 17.0, variation * 11.0)) - 0.5) * 1.6;
  let result = clamp(graded + grain * grainAmount * 0.16, vec3f(0.0), vec3f(1.0));
  return vec4f(result, 1.0);
}
`;
    frame.state.module = device.createShaderModule({ code: wgsl });
    frame.state.pipeline = null;
    frame.state.pipelineFormat = null;
    frame.state.quad = device.createBuffer({
        size: 6 * 4 * 4,
        usage: GPUBufferUsage.VERTEX,
        mappedAtCreation: true,
    });
    new Float32Array(frame.state.quad.getMappedRange()).set([
        -1, -1, 0, 1,
        1, -1, 1, 1,
        -1, 1, 0, 0,
        -1, 1, 0, 0,
        1, -1, 1, 1,
        1, 1, 1, 0,
    ]);
    frame.state.quad.unmap();
    frame.state.uniformBuf = device.createBuffer({
        // Two headers + four 11-vec4 gradients + nine controls + palette and breath data.
        size: 912,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });
}
export function render(device, frame) {
    var params = frame.params || {};
    function finiteNumber(value, fallback) {
        if (value == null)
            return fallback;
        var num = Number(value);
        return Number.isFinite(num) ? num : fallback;
    }
    function numberParam(name, fallback) {
        return finiteNumber(params[name], fallback);
    }
    function boolParam(name, fallback) {
        if (typeof params[name] === 'boolean')
            return params[name] ? 1 : 0;
        return fallback ? 1 : 0;
    }
    function selectParam(name, count, fallbackIndex) {
        // A select is a numeric dropdown: params[name] is the chosen option's index.
        var index = Math.round(Number(params[name]));
        return Number.isFinite(index) && index >= 0 && index < count ? index : fallbackIndex;
    }
    function colorParam(name, fallback) {
        var value = params[name] || {};
        return [
            finiteNumber(value.r, fallback[0]),
            finiteNumber(value.g, fallback[1]),
            finiteNumber(value.b, fallback[2]),
            finiteNumber(value.a, fallback[3]),
        ];
    }
    function pointParam(name, fallback) {
        var value = params[name] || {};
        return [
            finiteNumber(value.x, fallback[0]),
            finiteNumber(value.y, fallback[1]),
        ];
    }
    function pointRadiusParam(name, fallback) {
        var value = params[name] || {};
        return [
            finiteNumber(value.x, fallback[0]),
            finiteNumber(value.y, fallback[1]),
            finiteNumber(value.radius, fallback[2]),
        ];
    }
    function pointPointLineParam(name, fallback) {
        var value = params[name] || {};
        return [
            finiteNumber(value.x, fallback[0]),
            finiteNumber(value.y, fallback[1]),
            finiteNumber(value.x2, fallback[2]),
            finiteNumber(value.y2, fallback[3]),
        ];
    }
    function pointAngleRadiusParam(name, fallback) {
        var value = params[name] || {};
        return [
            finiteNumber(value.x, fallback[0]),
            finiteNumber(value.y, fallback[1]),
            finiteNumber(value.radius, fallback[2]),
            finiteNumber(value.angle, fallback[3]),
        ];
    }
    function colorPointParam(name, fallback) {
        var value = params[name] || {};
        var color = value.color || {};
        return [
            finiteNumber(value.x, fallback[0]),
            finiteNumber(value.y, fallback[1]),
            0,
            0,
            finiteNumber(color.r, fallback[2]),
            finiteNumber(color.g, fallback[3]),
            finiteNumber(color.b, fallback[4]),
            finiteNumber(color.a, fallback[5]),
        ];
    }
    function gradientParam(name, fallback) {
        var value = params[name] || {};
        var stops = Array.isArray(value.stops) && value.stops.length > 0 ? value.stops : fallback;
        // Layout: 8 stop colors (rgba), then 8 positions packed 4-per-vec4, then the live count.
        var out = new Array(44).fill(0);
        var count = Math.min(stops.length, 8);
        for (var i = 0; i < count; i++) {
            var stop = stops[i] || {};
            var color = stop.color || {};
            out[i * 4] = finiteNumber(color.r, 0);
            out[i * 4 + 1] = finiteNumber(color.g, 0);
            out[i * 4 + 2] = finiteNumber(color.b, 0);
            out[i * 4 + 3] = finiteNumber(color.a, 1);
            out[32 + i] = finiteNumber(stop.position, 0);
        }
        out[40] = count;
        return out;
    }
    var output = frame.output || {};
    // Compose in points so grain and proportions survive changes in render scale.
    var renderScale = Math.max(0.001, finiteNumber(frame.renderScale, 1));
    var width = Math.max(1, finiteNumber(output.physicalWidth, finiteNumber(output.width, 1)) / renderScale);
    var height = Math.max(1, finiteNumber(output.physicalHeight, finiteNumber(output.height, 1)) / renderScale);
    var time = finiteNumber(frame.time, 0) * 0.001;
    // Runtime contract: frame.time is milliseconds. Convert exactly once above.
    // At the existing .18/.15/.6 controls these rates produce .36 rad/s spatial,
    // .36 rad/s within-palette, and .12 palette steps/s, respectively.
    // Integrate all three independently so speed edits never reset their phases
    // and zero freezes at the current appearance rather than jumping to time zero.
    var motionSpeed = Math.max(0, Math.min(2, numberParam("motionSpeed", 0.18)));
    var colourSpeed = Math.max(0, Math.min(2, numberParam("colourSpeed", 0.15)));
    var paletteSpeed = Math.max(0, Math.min(2, numberParam("paletteTransitionSpeed", 0.6)));
    var elapsed = frame.state.animationLastTime == null ? 0 : Math.max(0, time - frame.state.animationLastTime);
    frame.state.motionClock = finiteNumber(frame.state.motionClock, 0) + elapsed * motionSpeed * 2.0;
    frame.state.colourClock = finiteNumber(frame.state.colourClock, 0) + elapsed * colourSpeed * 2.4;
    frame.state.paletteClock = finiteNumber(frame.state.paletteClock, 0) + elapsed * paletteSpeed * 0.2;
    frame.state.animationLastTime = time;
    var inputWidth = width;
    var inputHeight = height;
    device.queue.writeBuffer(frame.state.uniformBuf, 0, new Float32Array([
        time, width, height, 0,
        inputWidth, inputHeight, 0, 0,
        ...gradientParam("palette", [{ "position": 0, "color": { "r": 0.96, "g": 0.93, "b": 0.83, "a": 1 } }, { "position": 0.32, "color": { "r": 0.88, "g": 0.8, "b": 0.44, "a": 1 } }, { "position": 0.65, "color": { "r": 0.64, "g": 0.43, "b": 0.57, "a": 1 } }, { "position": 1, "color": { "r": 0.65, "g": 0.78, "b": 0.87, "a": 1 } }]),
        ...gradientParam("palette2", [{ "position": 0, "color": { "r": 0.88, "g": 0.39, "b": 0.37, "a": 1 } }, { "position": 0.5, "color": { "r": 0.72, "g": 0.61, "b": 0.81, "a": 1 } }, { "position": 1, "color": { "r": 0.98, "g": 0.9, "b": 0.68, "a": 1 } }]),
        ...gradientParam("palette3", [{ "position": 0, "color": { "r": 0.76, "g": 0.53, "b": 0.22, "a": 1 } }, { "position": 0.5, "color": { "r": 0.61, "g": 0.4, "b": 0.53, "a": 1 } }, { "position": 1, "color": { "r": 0.96, "g": 0.93, "b": 0.84, "a": 1 } }]),
        ...gradientParam("palette4", [{ "position": 0, "color": { "r": 0.93, "g": 0.86, "b": 0.42, "a": 1 } }, { "position": 0.32, "color": { "r": 0.52, "g": 0.56, "b": 0.3, "a": 1 } }, { "position": 0.65, "color": { "r": 0.65, "g": 0.79, "b": 0.88, "a": 1 } }, { "position": 1, "color": { "r": 0.97, "g": 0.94, "b": 0.84, "a": 1 } }]),
        frame.state.motionClock, 0, 0, 0,
        frame.state.colourClock, 0, 0, 0,
        numberParam("blobSize", 65), 0, 0, 0,
        numberParam("blobCount", 7), 0, 0, 0,
        numberParam("softness", 0.72), 0, 0, 0,
        numberParam("grainAmount", 0.16), 0, 0, 0,
        numberParam("grainSize", 0.8), 0, 0, 0,
        numberParam("intensity", 1), 0, 0, 0,
        numberParam("seed", 12), 0, 0, 0,
        Math.max(1, Math.min(4, Math.round(numberParam("activePaletteCount", 3)))), frame.state.paletteClock, 0, 0,
        Math.max(0, Math.min(1, numberParam("blowForce", 0))), 0, 0, 0,
    ]));
    var outputFormat = frame.output.format;
    if (frame.state.pipeline == null || frame.state.pipelineFormat !== outputFormat) {
        frame.state.pipeline = device.createRenderPipeline({
            layout: 'auto',
            vertex: {
                module: frame.state.module,
                entryPoint: 'vs_main',
                buffers: [{
                        arrayStride: 16,
                        attributes: [
                            { shaderLocation: 0, format: 'float32x2', offset: 0 },
                            { shaderLocation: 1, format: 'float32x2', offset: 8 },
                        ],
                    }],
            },
            fragment: {
                module: frame.state.module,
                entryPoint: 'fs_main',
                targets: [{ format: outputFormat }],
            },
            primitive: { topology: 'triangle-list' },
        });
        frame.state.pipelineFormat = outputFormat;
    }
    var bindGroup = device.createBindGroup({
        layout: frame.state.pipeline.getBindGroupLayout(0),
        entries: [
            { binding: 0, resource: { buffer: frame.state.uniformBuf } },
        ],
    });
    var encoder = device.createCommandEncoder();
    var pass = encoder.beginRenderPass({
        colorAttachments: [{
                view: frame.output.createView(),
                loadOp: 'clear',
                clearValue: { r: 0, g: 0, b: 0, a: 0 },
                storeOp: 'store',
            }],
    });
    pass.setPipeline(frame.state.pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.setVertexBuffer(0, frame.state.quad);
    pass.draw(6);
    pass.end();
    device.queue.submit([encoder.finish()]);
}
defineProperties(Effect, {
    "palette": {
        type: "gradient",
        label: "Palette 1",
        defaultValue: { "stops": [{ "position": 0, "color": { "r": 0.96, "g": 0.93, "b": 0.83, "a": 1 } }, { "position": 0.32, "color": { "r": 0.88, "g": 0.8, "b": 0.44, "a": 1 } }, { "position": 0.65, "color": { "r": 0.64, "g": 0.43, "b": 0.57, "a": 1 } }, { "position": 1, "color": { "r": 0.65, "g": 0.78, "b": 0.87, "a": 1 } }] },
    },
    "palette2": {
        type: "gradient",
        label: "Palette 2",
        defaultValue: { "stops": [{ "position": 0, "color": { "r": 0.88, "g": 0.39, "b": 0.37, "a": 1 } }, { "position": 0.5, "color": { "r": 0.72, "g": 0.61, "b": 0.81, "a": 1 } }, { "position": 1, "color": { "r": 0.98, "g": 0.9, "b": 0.68, "a": 1 } }] },
    },
    "palette3": {
        type: "gradient",
        label: "Palette 3",
        defaultValue: { "stops": [{ "position": 0, "color": { "r": 0.76, "g": 0.53, "b": 0.22, "a": 1 } }, { "position": 0.5, "color": { "r": 0.61, "g": 0.4, "b": 0.53, "a": 1 } }, { "position": 1, "color": { "r": 0.96, "g": 0.93, "b": 0.84, "a": 1 } }] },
    },
    "palette4": {
        type: "gradient",
        label: "Palette 4",
        defaultValue: { "stops": [{ "position": 0, "color": { "r": 0.93, "g": 0.86, "b": 0.42, "a": 1 } }, { "position": 0.32, "color": { "r": 0.52, "g": 0.56, "b": 0.3, "a": 1 } }, { "position": 0.65, "color": { "r": 0.65, "g": 0.79, "b": 0.88, "a": 1 } }, { "position": 1, "color": { "r": 0.97, "g": 0.94, "b": 0.84, "a": 1 } }] },
    },
    "activePaletteCount": {
        type: "number",
        label: "Active palettes (first N)",
        defaultValue: 3,
        control: "slider",
        min: 1,
        max: 4,
        step: 1,
    },
    "paletteTransitionSpeed": {
        type: "number",
        label: "Between-palette speed",
        defaultValue: 0.6,
        control: "slider",
        min: 0,
        max: 2,
        step: 0.01,
    },
    "motionSpeed": {
        type: "number",
        label: "Motion speed",
        defaultValue: 0.18,
        control: "slider",
        min: 0,
        max: 2,
        step: 0.01,
    },
    "colourSpeed": {
        type: "number",
        label: "Colour-transition speed",
        defaultValue: 0.15,
        control: "slider",
        min: 0,
        max: 2,
        step: 0.01,
    },
    "blobSize": {
        type: "number",
        label: "Blob size",
        defaultValue: 65,
        control: "slider",
        min: 15,
        max: 150,
        step: 1,
        unit: "%",
    },
    "blobCount": {
        type: "number",
        label: "Blob count",
        defaultValue: 7,
        control: "slider",
        min: 2,
        max: 16,
        step: 1,
    },
    "softness": {
        type: "number",
        label: "Blend softness",
        defaultValue: 0.72,
        control: "slider",
        min: 0,
        max: 1,
        step: 0.01,
    },
    "grainAmount": {
        type: "number",
        label: "Grain amount",
        defaultValue: 0.16,
        control: "slider",
        min: 0,
        max: 1,
        step: 0.01,
    },
    "grainSize": {
        type: "number",
        label: "Grain size",
        defaultValue: 0.8,
        control: "slider",
        min: 0.4,
        max: 5,
        step: 0.1,
        unit: "px",
    },
    "intensity": {
        type: "number",
        label: "Colour intensity",
        defaultValue: 1,
        control: "slider",
        min: 0,
        max: 1,
        step: 0.01,
    },
    "seed": {
        type: "number",
        label: "Random seed / variation",
        defaultValue: 12,
        control: "input",
        min: 0,
        max: 10000,
        step: 1,
    },
});

export const manifest = {
  "version": 3,
  "name": "Drifting colour fields",
  "isAnimated": true,
  "usesMouse": false,
  "renderScaleAware": true
}

