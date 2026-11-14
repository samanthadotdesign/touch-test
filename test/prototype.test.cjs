const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');

// Run the shipped browser scripts, with only the browser surface simulated.
// The real renderer checks every draw coordinate and the real engine owns timing.
function prototypeHarness({raw=null,blocked=false}={}) {
  const canvasEvents=new Map(),windowEvents=new Map(),documentEvents=new Map();
  const attributes=new Map();
  let pendingFrame,now=0,current,draws=0,writes=0,focuses=0;
  const ctx={};
  for(const name of ['fillRect','beginPath','lineTo','moveTo','closePath','fill']) {
    ctx[name]=(...args)=>{for(const value of args)assert.ok(Number.isFinite(value),`${name} received an invalid coordinate`);draws++;};
  }
  const canvas={
    getContext:(kind,options)=>{assert.equal(kind,'2d');assert.equal(options.alpha,false);return ctx;},
    addEventListener:(name,handler)=>canvasEvents.set(name,handler),
    setAttribute:(name,value)=>attributes.set(name,value),
    focus:()=>{focuses++;}
  };
  let seed=314159;
  const math=Object.create(Math);math.random=()=>((seed=(Math.imul(seed,1664525)+1013904223)>>>0)/4294967296);
  const environment={
    Math:math,structuredClone,
    document:{querySelector:selector=>{assert.equal(selector,'#lcd');return canvas;},addEventListener:(name,handler)=>documentEvents.set(name,handler)},
    window:{addEventListener:(name,handler)=>windowEvents.set(name,handler)},
    localStorage:{getItem:()=>{if(blocked)throw Error('Storage denied');return raw;},setItem:()=>{writes++;throw Error('Prototype must not write studio storage');}},
    performance:{now:()=>now},
    requestAnimationFrame:callback=>{pendingFrame=callback;}
  };
  vm.createContext(environment);
  for(const file of ['sandbox/poetry-data.js','sandbox/engine.js','sandbox/library-store.js','sandbox/display.js']) {
    vm.runInContext(fs.readFileSync(path.join(__dirname,'..',file),'utf8'),environment,{filename:file});
  }
  const createRenderer=environment.PoetryDisplay.createRenderer;
  environment.PoetryDisplay.createRenderer=(...args)=>{
    const render=createRenderer(...args);
    return experience=>{current=experience;render(experience);};
  };
  vm.runInContext(fs.readFileSync(path.join(__dirname,'../prototype/prototype.js'),'utf8'),environment,{filename:'prototype/prototype.js'});
  const dispatch=(events,name,properties={})=>{
    let prevented=false;
    const event={preventDefault:()=>{prevented=true;},...properties};
    events.get(name)?.(event);
    return prevented;
  };
  function frame(milliseconds=100) {
    now+=milliseconds;
    const callback=pendingFrame;pendingFrame=undefined;
    assert.equal(typeof callback,'function');callback(now);
  }
  return {
    get experience(){return current;},get draws(){return draws;},get writes(){return writes;},get focuses(){return focuses;},
    attributes,source:environment.POETRY_DATA,revision:environment.POETRY_SOURCE_REVISION,
    storageKey:environment.PoetryLibraryStore.STORAGE_KEY,
    pointer:properties=>dispatch(canvasEvents,'pointerdown',{isPrimary:true,button:0,...properties}),
    key:properties=>dispatch(canvasEvents,'keydown',{repeat:false,...properties}),
    contextmenu:()=>dispatch(canvasEvents,'contextmenu'),
    storage:(key,value)=>{raw=value;dispatch(windowEvents,'storage',{key});},
    visibility:()=>dispatch(documentEvents,'visibilitychange'),
    frame,advance:seconds=>{for(let i=0;i<Math.ceil(seconds*10);i++)frame();}
  };
}
const plain=value=>JSON.parse(JSON.stringify(value));
function savedLibrary(harness,text='Saved in the motion studio') {
  const library=plain(harness.source);
  library.poems=[{id:'saved-poem',title:'',text,by:'Studio author',layout:'centered'}];
  library.animations=[{...library.animations[0],id:'saved-style',name:'Saved style',speed:.17}];
  return {sourceRevision:harness.revision,library};
}

test('device prototype starts with the source collection and draws through the shared renderer',()=>{
  const app=prototypeHarness();
  assert.equal(app.experience.mode,'MOVING');
  assert.deepEqual(plain(app.experience.library),plain(app.source));
  assert.ok(app.draws>100);
  const before=app.draws;app.frame();assert.ok(app.draws>before);
  assert.equal(app.attributes.get('aria-disabled'),'false');
  assert.match(app.attributes.get('aria-label'),/Touch to assemble/);
  assert.equal(app.writes,0);
});

test('pointer and key gating allow one action per press and ignore inputs during transitions',()=>{
  const app=prototypeHarness();
  assert.equal(app.pointer({isPrimary:false}),false);
  assert.equal(app.pointer({button:2}),false);
  assert.equal(app.pointer({button:1}),false);
  assert.equal(app.key({key:'Escape'}),false);
  assert.equal(app.key({key:'Enter',repeat:true}),true);
  assert.equal(app.experience.mode,'MOVING');assert.equal(app.focuses,0);
  assert.equal(app.pointer(),true);assert.equal(app.focuses,1);
  assert.equal(app.experience.mode,'ASSEMBLING');
  assert.equal(app.attributes.get('aria-disabled'),'true');
  app.advance(.3);app.pointer();app.key({key:' '});
  assert.equal(app.experience.mode,'ASSEMBLING');
  app.advance(.3);assert.equal(app.experience.mode,'READING');
  assert.equal(app.attributes.get('aria-disabled'),'false');
  assert.ok(app.attributes.get('aria-label').includes(app.experience.poem.text));
  const previousPoem=app.experience.poem.id;
  app.key({key:'Enter',repeat:true});assert.equal(app.experience.mode,'READING');
  app.key({key:'Enter'});assert.equal(app.experience.mode,'DISSOLVING');
  app.advance(.3);app.pointer();app.key({key:' ',repeat:true});
  assert.equal(app.experience.mode,'DISSOLVING');
  app.advance(.4);assert.equal(app.experience.mode,'MOVING');
  assert.notEqual(app.experience.poem.id,previousPoem);
  assert.equal(app.contextmenu(),true);
});

test('reading waits for another touch through idle time and tab visibility changes',()=>{
  const app=prototypeHarness();app.key({key:' '});app.advance(.6);
  assert.equal(app.experience.mode,'READING');
  const text=app.experience.poem.text,layout=app.experience.arrangement.layoutName;
  for(let i=0;i<30;i++)app.frame(60000);
  app.visibility();app.frame(3600000);
  assert.equal(app.experience.mode,'READING');
  assert.equal(app.experience.poem.text,text);assert.equal(app.experience.arrangement.layoutName,layout);
  app.pointer();app.advance(.7);assert.equal(app.experience.mode,'MOVING');
});

test('saved library loads read-only and blocked, malformed, or invalid storage falls back to source',()=>{
  const sourceApp=prototypeHarness(),saved=savedLibrary(sourceApp);
  const app=prototypeHarness({raw:JSON.stringify(saved)});
  assert.equal(app.experience.poem.text,'Saved in the motion studio');
  assert.equal(app.experience.motion.speed,.17);assert.equal(app.writes,0);
  for(const options of [
    {blocked:true},
    {raw:'{broken'},
    {raw:JSON.stringify({poems:[],animations:[]})},
    {raw:JSON.stringify({...saved,library:{...saved.library,poems:[{...saved.library.poems[0],text:'Unsupported 🌙'}]}})},
    {raw:JSON.stringify({...saved,sourceRevision:'older-source'})}
  ]) {
    const fallback=prototypeHarness(options);
    assert.equal(fallback.writes,0);
    if(options.raw?.includes('older-source'))assert.ok(fallback.experience.library.poems.some(poem=>poem.id==='saved-poem'));
    else assert.deepEqual(plain(fallback.experience.library),plain(fallback.source));
    assert.ok(fallback.draws>100);
  }
});

test('same-origin library events refresh an open prototype while unrelated storage events do nothing',()=>{
  const app=prototypeHarness();app.pointer();app.advance(.6);
  const readingScene=app.experience;
  app.storage('unrelated-preference',null);app.frame();assert.equal(app.experience,readingScene);
  const saved=savedLibrary(app,'Newly saved studio poem');
  app.storage(app.storageKey,JSON.stringify(saved));app.frame();
  assert.notEqual(app.experience,readingScene);assert.equal(app.experience.mode,'MOVING');
  assert.equal(app.experience.poem.text,'Newly saved studio poem');assert.equal(app.experience.motion.speed,.17);
  app.storage(app.storageKey,'{bad');app.frame();
  assert.deepEqual(plain(app.experience.library),plain(app.source));
  app.storage(app.storageKey,JSON.stringify(saved));app.frame();
  app.storage(null,null);app.frame();
  assert.deepEqual(plain(app.experience.library),plain(app.source));assert.equal(app.writes,0);
});
