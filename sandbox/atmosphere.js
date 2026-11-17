/* Shared browser-only atmosphere, hidden theme sheet, and local microphone input. */
(function (root) {
  'use strict';
  const THEME_KEY = 'poetry-atmosphere-theme-v1'; // Legacy key; no longer read or written.
  const COLOURWAY_KEY = 'poetry-atmosphere-colourway-v1';
  const THEMES = ['dark', 'light', 'colour'];
  const HIDE_DELAY = 6000;
  const clamp = (n, min, max) => Math.max(min, Math.min(max, n));

  function levelFromSamples(samples) {
    if (!samples.length) return 0;
    let energy = 0;
    for (const sample of samples) energy += sample * sample;
    // A gentle breath is enough to tap; retain a small floor for quiet rooms.
    return clamp((Math.sqrt(energy / samples.length) - .0015) * 30, 0, 1);
  }
  function smoothLevel(current, target, seconds) {
    const dt = clamp(Number.isFinite(seconds) ? seconds : 0, 0, .1);
    return current + (target - current) * (1 - Math.exp(-dt / (target > current ? .018 : .18)));
  }
  function smoothDetectionLevel(current, target, seconds) {
    const dt = clamp(Number.isFinite(seconds) ? seconds : 0, 0, .1);
    return current + (target - current) * (1 - Math.exp(-dt / (target > current ? .015 : .06)));
  }
  function motionScale(level, active = false) {
    return Math.min(9, (active ? 1.25 : 1) + Math.sqrt(clamp(level, 0, 1)) * 7.75);
  }

  function createBlowGate({threshold = .10, holdSeconds = .06, releaseSeconds = .5, cooldownSeconds = 1.6} = {}) {
    threshold = clamp(Number.isFinite(threshold) ? threshold : .10, .05, .95);
    let held = 0, quiet = 0, cooldown = 0, armed = true;
    return {
      get threshold() { return threshold; },
      get progress() { return clamp(held / holdSeconds, 0, 1); },
      get armed() { return armed; },
      reset() { held = quiet = cooldown = 0; armed = true; },
      setThreshold(value) { threshold = clamp(Number.isFinite(value) ? value : .10, .05, .95); held = 0; },
      update(level, seconds, enabled = true, rawLevel = level) {
        if (!enabled) { this.reset(); return false; }
        const dt = clamp(Number.isFinite(seconds) ? seconds : 0, 0, .1);
        const strength = clamp(Number.isFinite(level) ? level : 0, 0, 1);
        const rawStrength = clamp(Number.isFinite(rawLevel) ? rawLevel : 0, 0, 1);
        cooldown = Math.max(0, cooldown - dt);
        if (!armed) {
          quiet = strength <= Math.min(.25, threshold * .45) ? quiet + dt : 0;
          if (quiet + 1e-9 >= releaseSeconds && cooldown <= 1e-9) { armed = true; held = 0; }
          return false;
        }
        // The real input must remain above threshold. A brief spike's decay
        // tail can keep the visual/detection smoothing high after sound stops.
        held = strength >= threshold && rawStrength >= threshold ? held + dt : 0;
        if (held + 1e-9 < holdSeconds) return false;
        armed = false;
        held = quiet = 0;
        cooldown = cooldownSeconds;
        return true;
      }
    };
  }

  function createController(screen, {onMicStart = () => {}, onBlow = () => false} = {}) {
    let theme = 'colour';
    let previousColourway = -1;
    try {
      const saved = localStorage.getItem(COLOURWAY_KEY);
      if (/^[0-2]$/.test(saved)) previousColourway = Number(saved);
    } catch (_) {}
    const colourways = [0, 1, 2].filter(value => value !== previousColourway);
    const colourway = colourways[Math.floor(Math.random() * colourways.length)];
    try { localStorage.setItem(COLOURWAY_KEY, String(colourway)); } catch (_) {}
    screen.dataset.theme = theme;
    screen.dataset.colourway = String(colourway);
    screen.insertAdjacentHTML('beforeend', `
      <button type="button" class="atmosphere-hotspot" aria-label="Open atmosphere options" aria-haspopup="dialog" aria-expanded="false" aria-controls="atmosphere-options"></button>
      <div class="atmosphere-backdrop" hidden></div>
      <section class="atmosphere-sheet" id="atmosphere-options" role="dialog" aria-modal="true" aria-labelledby="atmosphere-title" hidden>
        <div class="sheet-handle" aria-hidden="true"></div>
        <header><h2 id="atmosphere-title">Atmosphere</h2><button type="button" class="sheet-close" aria-label="Close atmosphere options">×</button></header>
        <div class="theme-choices" role="group" aria-label="Background theme">
          <button type="button" class="theme-choice" data-theme="dark"><span class="theme-swatch" aria-hidden="true"></span><span class="theme-copy"><strong>Night clouds</strong><small>Black · soft grey · white words</small></span><span class="theme-check" aria-hidden="true">✓</span></button>
          <button type="button" class="theme-choice" data-theme="light"><span class="theme-swatch" aria-hidden="true"></span><span class="theme-copy"><strong>Paper clouds</strong><small>White · soft grey · black words</small></span><span class="theme-check" aria-hidden="true">✓</span></button>
          <button type="button" class="theme-choice" data-theme="colour"><span class="theme-swatch" aria-hidden="true"></span><span class="theme-copy"><strong>Colour fields</strong><small>Slow, independently changing colour</small></span><span class="theme-check" aria-hidden="true">✓</span></button>
        </div>
        <div class="sheet-audio"><button type="button" class="mic-toggle" data-mic-toggle aria-pressed="false">Enable mic</button><span class="mic-meter" data-mic-meter role="meter" aria-label="Sound level" aria-valuemin="0" aria-valuemax="100" aria-valuenow="0"><i></i></span></div>
        <p class="mic-status" data-mic-status role="status">Mic off · slow drift</p>
        <p class="shader-notice" data-shader-status hidden></p>
        <p class="sheet-footnote">Hides after 6 seconds of inactivity</p>
      </section>`);

    const hotspot = screen.querySelector('.atmosphere-hotspot');
    const sheet = screen.querySelector('.atmosphere-sheet');
    const backdrop = screen.querySelector('.atmosphere-backdrop');
    const choices = [...sheet.querySelectorAll('.theme-choice')];
    const micButtons = [...document.querySelectorAll('[data-mic-toggle]')];
    const micStatuses = [...document.querySelectorAll('[data-mic-status]')];
    const meters = [...document.querySelectorAll('[data-mic-meter]')];
    const preview = document.querySelector('#sound-preview');
    const levelOutput = document.querySelector('#sound-level');
    const thresholdInput = document.querySelector('#blow-threshold');
    const thresholdOutput = document.querySelector('#blow-threshold-value');
    const thresholdMarker = document.querySelector('.blow-threshold-marker');
    const blowFeedback = document.querySelector('#blow-feedback');
    const blowGate = createBlowGate();
    let timer, restoreFocus, micState = 'off', micMessage = 'Mic off · slow drift';
    let level = 0, detectionLevel = 0, previewLevel = 0, previewEngaged = false, stream, audioContext, analyser, source, samples;
    let requestId = 0, lastPercent = -1, disposed = false;

    function armTimer() {
      clearTimeout(timer);
      if (!disposed && !sheet.hidden && micState !== 'starting') timer = setTimeout(close, HIDE_DELAY);
    }
    function close() {
      clearTimeout(timer);
      const focusInside = sheet.contains(document.activeElement);
      sheet.hidden = backdrop.hidden = true;
      hotspot.setAttribute('aria-expanded', 'false');
      if (focusInside) (restoreFocus || hotspot).focus({preventScroll: true});
    }
    function open() {
      restoreFocus = document.activeElement;
      sheet.hidden = backdrop.hidden = false;
      hotspot.setAttribute('aria-expanded', 'true');
      choices.find(choice => choice.dataset.theme === theme).focus({preventScroll: true});
      armTimer();
    }
    function syncTheme() {
      screen.dataset.theme = theme;
      choices.forEach(choice => choice.setAttribute('aria-pressed', String(choice.dataset.theme === theme)));
    }
    function setTheme(value) {
      if (!THEMES.includes(value)) return;
      theme = value;
      syncTheme();
      armTimer();
    }
    choices.forEach(choice => choice.addEventListener('click', () => setTheme(choice.dataset.theme)));
    hotspot.addEventListener('click', open);
    backdrop.addEventListener('pointerdown', event => { event.preventDefault(); close(); });
    sheet.querySelector('.sheet-close').addEventListener('click', close);
    sheet.addEventListener('pointerdown', armTimer);
    sheet.addEventListener('input', armTimer);
    sheet.addEventListener('keydown', event => {
      armTimer();
      if (event.key === 'Escape') { event.preventDefault(); close(); }
      if (event.key === 'Tab') {
        const buttons = [...sheet.querySelectorAll('button:not(:disabled)')];
        const index = buttons.indexOf(document.activeElement);
        if (event.shiftKey && index <= 0) { event.preventDefault(); buttons.at(-1).focus(); }
        else if (!event.shiftKey && index === buttons.length - 1) { event.preventDefault(); buttons[0].focus(); }
      }
    });

    function syncMic() {
      const active = micState === 'on';
      micButtons.forEach(button => {
        button.textContent = micState === 'starting' ? 'Cancel mic' : active ? 'Disable mic' : 'Enable mic';
        button.setAttribute('aria-pressed', String(active));
      });
      micStatuses.forEach(status => { status.textContent = micMessage; });
      if (preview) preview.disabled = active || micState === 'starting';
    }
    function releaseAudio() {
      if (stream) stream.getTracks().forEach(track => track.stop());
      stream = undefined;
      if (source) source.disconnect();
      if (analyser) analyser.disconnect();
      source = analyser = samples = undefined;
      if (audioContext && audioContext.state !== 'closed') audioContext.close().catch(() => {});
      audioContext = undefined;
    }
    function stopMic(message = 'Mic off · slow drift') {
      requestId++;
      releaseAudio();
      blowGate.reset();
      detectionLevel = 0;
      previewEngaged = false;
      micState = 'off';
      micMessage = message;
      syncMic();
      armTimer();
    }
    async function toggleMic() {
      if (micState === 'on' || micState === 'starting') { stopMic(); return; }
      if (!navigator.mediaDevices?.getUserMedia || !(root.AudioContext || root.webkitAudioContext)) {
        micMessage = 'Mic needs localhost or HTTPS in a supported browser. Try the studio’s volume preview.';
        syncMic();
        armTimer();
        return;
      }
      const id = ++requestId;
      blowGate.reset();
      detectionLevel = 0;
      previewLevel = 0;
      previewEngaged = false;
      if (preview) preview.value = '0';
      micState = 'starting';
      micMessage = 'Allow microphone access to move with sound.';
      clearTimeout(timer);
      syncMic();
      try {
        // Created from this click so browsers can unlock audio immediately.
        const Context = root.AudioContext || root.webkitAudioContext;
        const context = new Context();
        audioContext = context;
        const resume = context.resume().then(() => null, error => error);
        const incoming = await navigator.mediaDevices.getUserMedia({audio: {echoCancellation: false, noiseSuppression: false, autoGainControl: false}, video: false});
        if (id !== requestId || disposed) { incoming.getTracks().forEach(track => track.stop()); return; }
        stream = incoming;
        const resumeError = await resume;
        if (id !== requestId || disposed) return;
        if (resumeError) throw resumeError;
        analyser = context.createAnalyser();
        analyser.fftSize = 1024;
        samples = new Float32Array(analyser.fftSize);
        source = context.createMediaStreamSource(incoming);
        source.connect(analyser); // No destination: sound is never played or recorded.
        incoming.getAudioTracks().forEach(track => track.addEventListener('ended', () => { if (id === requestId && micState === 'on') stopMic('Mic disconnected · slow drift'); }));
        previewLevel = 0;
        if (preview) preview.value = '0';
        micState = 'on';
        micMessage = 'Listening · blow to spread the colours and words';
        onMicStart();
        syncMic();
        armTimer();
      } catch (error) {
        if (id !== requestId || disposed) return;
        stopMic(error.name === 'NotAllowedError' || error.name === 'SecurityError'
          ? 'Mic permission blocked. Enable it in browser settings, or try the studio’s volume preview.'
          : 'Microphone unavailable. Try the studio’s volume preview.');
      }
    }
    micButtons.forEach(button => button.addEventListener('click', toggleMic));
    if (preview) preview.addEventListener('input', () => {
      const previous = previewLevel;
      previewLevel = clamp(Number(preview.value) / 100, 0, 1);
      if (previewLevel > 0) previewEngaged = true;
      if (previewLevel > 0 && previous === 0) onMicStart();
      micMessage = previewLevel > 0 ? 'Previewing sound · microphone off' : 'Mic off · slow drift';
      syncMic();
    });
    function syncThreshold() {
      const percent = Math.round(blowGate.threshold * 100);
      if (thresholdInput) thresholdInput.value = String(percent);
      if (thresholdOutput) thresholdOutput.textContent = percent + '%';
      if (thresholdMarker) thresholdMarker.style.left = percent + '%';
    }
    if (thresholdInput) thresholdInput.addEventListener('input', () => {
      blowGate.setThreshold(Number(thresholdInput.value) / 100);
      syncThreshold();
    });
    document.addEventListener?.('visibilitychange', () => { if (document.hidden) { blowGate.reset(); detectionLevel = 0; } });
    root.addEventListener('pagehide', () => { disposed = true; clearTimeout(timer); stopMic(); });
    root.addEventListener('pageshow', () => { disposed = false; });
    syncTheme();
    syncMic();
    syncThreshold();

    return {
      get theme() { return theme; },
      get colourway() { return colourway; },
      get level() { return level; },
      get blowForce() { return Math.sqrt(clamp(level, 0, 1)); },
      get micActive() { return micState === 'on'; },
      get motionScale() { return motionScale(level, micState === 'on'); },
      open, close, setTheme, toggleMic, stopMic,
      step(seconds) {
        let target = previewLevel;
        if (micState === 'on' && analyser) { analyser.getFloatTimeDomainData(samples); target = levelFromSamples(samples); }
        level = smoothLevel(level, target, seconds);
        // Keep the movement easing continuous while each new audio source gets
        // a fresh detection signal, so leftover preview pressure cannot advance.
        detectionLevel = smoothDetectionLevel(detectionLevel, target, seconds);
        // One held breath produces one navigation. A quiet release rearms it.
        const canBlow = (micState === 'on' || (micState === 'off' && previewEngaged)) && sheet.hidden && !document.hidden;
        const crossed = blowGate.update(detectionLevel, seconds, canBlow, target);
        if (crossed) {
          const accepted = onBlow();
          if (blowFeedback) blowFeedback.textContent = accepted === false
            ? 'Finish this transition, then release and blow again.'
            : 'Threshold reached. Release before the next blow or tap.';
        } else if (blowFeedback && blowGate.armed) {
          blowFeedback.textContent = blowGate.progress > 0
            ? 'Keep blowing to cross the threshold…'
            : 'Blow gently to spread the colours and words.';
        }
        const percent = Math.round(level * 100);
        if (percent !== lastPercent) {
          meters.forEach(meter => { meter.setAttribute('aria-valuenow', percent); meter.querySelector('i').style.width = percent + '%'; });
          if (levelOutput) levelOutput.textContent = percent + '%';
          lastPercent = percent;
        }
        return this.motionScale;
      },
      setShaderStatus(ready) {
        screen.dataset.shader = ready ? 'ready' : 'unavailable';
        document.querySelectorAll('[data-shader-status]').forEach(notice => {
          notice.hidden = ready;
          notice.textContent = 'Animated clouds need WebGPU. Open this localhost preview in a WebGPU-enabled browser.';
        });
      }
    };
  }
  const api = {createController, createBlowGate, levelFromSamples, smoothLevel, motionScale, THEME_KEY, COLOURWAY_KEY, HIDE_DELAY};
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.PoetryAtmosphere = api;
})(globalThis);
