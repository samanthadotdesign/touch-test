(function () {
  'use strict';
  const {Experience, validateLibrary} = PoetryEngine;
  const canvas = document.querySelector('#lcd');
  let experience;
  const atmosphere = PoetryAtmosphere.createController(document.querySelector('#poetry-screen'), {
    onBlow: () => touch()
  });
  PoetryBackground.mount(document.querySelector('.poetry-background'), atmosphere);
  const render = PoetryDisplay.createRenderer(canvas, POETRY_FONT, {transparent: true, getTheme: () => atmosphere.theme});

  function loadLibrary() {
    const source = validateLibrary(structuredClone(POETRY_DATA));
    try {
      // Read the studio's saved library without writing or migrating its storage.
      const storage = {getItem: key => localStorage.getItem(key), setItem() {}};
      return validateLibrary(PoetryLibraryStore.loadLibrary(source, POETRY_SOURCE_REVISION, storage).library);
    } catch (error) {
      return source;
    }
  }

  experience = new Experience(loadLibrary());
  let last = performance.now();
  let lastMode = '';

  function syncAccessibility() {
    if (lastMode === experience.mode) return;
    const mode = experience.mode;
    const reading = mode === 'READING';
    const transitioning = mode === 'ASSEMBLING' || mode === 'DISSOLVING';
    const description = reading
      ? (experience.poem.workTitle?.trim() ? experience.poem.workTitle.trim() + '. ' : '') + experience.poem.text + ' By ' + experience.poem.by + '. Touch or blow again for another poem.'
      : transitioning ? 'The words are moving into place.' : 'Touch to assemble the poem, or blow above the threshold with the mic enabled.';
    canvas.setAttribute('aria-label', 'Poetry display. ' + description);
    canvas.setAttribute('aria-disabled', String(transitioning));
    lastMode = mode;
  }

  function touch() {
    const accepted = experience.touch();
    syncAccessibility();
    return accepted;
  }

  canvas.addEventListener('pointerdown', event => {
    if (event.isPrimary === false || event.button !== 0) return;
    event.preventDefault();
    canvas.focus({preventScroll: true});
    touch();
  });
  canvas.addEventListener('keydown', event => {
    if (event.key !== 'Enter' && event.key !== ' ') return;
    event.preventDefault();
    if (!event.repeat) touch();
  });
  canvas.addEventListener('contextmenu', event => event.preventDefault());

  // Saved poems and styles appear in an already-open prototype on the same origin.
  window.addEventListener('storage', event => {
    if (event.key !== PoetryLibraryStore.STORAGE_KEY && event.key !== null) return;
    experience = new Experience(loadLibrary());
    lastMode = '';
    last = performance.now();
    syncAccessibility();
  });
  document.addEventListener('visibilitychange', () => { last = performance.now(); });

  function frame(now) {
    const seconds = (now - last) / 1000;
    experience.step(seconds, atmosphere.step(seconds), atmosphere.blowForce);
    last = now;
    render(experience);
    syncAccessibility();
    requestAnimationFrame(frame);
  }

  render(experience);
  syncAccessibility();
  requestAnimationFrame(frame);
})();
