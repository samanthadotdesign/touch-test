(function () {
  'use strict';
  const {Experience, validateLibrary} = PoetryEngine;
  const canvas = document.querySelector('#lcd');
  const render = PoetryDisplay.createRenderer(canvas, POETRY_FONT);

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

  let experience = new Experience(loadLibrary());
  let last = performance.now();
  let lastMode = '';

  function syncAccessibility() {
    if (lastMode === experience.mode) return;
    const mode = experience.mode;
    const reading = mode === 'READING';
    const transitioning = mode === 'ASSEMBLING' || mode === 'DISSOLVING';
    const description = reading
      ? experience.poem.text + ' By ' + experience.poem.by + '. Touch for another poem.'
      : transitioning ? 'The words are moving into place.' : 'Touch to assemble the poem.';
    canvas.setAttribute('aria-label', 'Poetry display. ' + description);
    canvas.setAttribute('aria-disabled', String(transitioning));
    lastMode = mode;
  }

  function touch() {
    experience.touch();
    syncAccessibility();
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
    experience.step((now - last) / 1000);
    last = now;
    render(experience);
    syncAccessibility();
    requestAnimationFrame(frame);
  }

  render(experience);
  syncAccessibility();
  requestAnimationFrame(frame);
})();
