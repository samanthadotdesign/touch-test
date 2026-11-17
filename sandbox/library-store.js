/* Browser persistence: database updates replace stale source copies, not custom work. */
(function (root) {
  'use strict';
  const STORAGE_KEY='airplane-poetry-library-v1';
  const BACKUP_KEY='airplane-poetry-library-backup-v1';
  const MIN_SPEED=.68;
  const LEGACY_PRESET_SPEEDS={orbit:.55,helix:.5,wave:.65,vortex:.6,drift:.45,tumble:.5,cloud:.45,ribbon:.55};
  const clone=value=>JSON.parse(JSON.stringify(value));
  const DEMOS={
    'small-light':'A small light\nleans into dusk.\nThe room remembers\nhow to be still.',
    'between-rain':'Between the rain\nand the window,\na quiet world\nlearns to shimmer.',
    'tidal':'The sea folds\nits blue letters.\nI leave a space\nfor the tide.',
    'passing':'Clouds carry\nno luggage.\nStill, they arrive\nwith everything.',
    'unhurried':'Let the day\nopen slowly.\nEven the flower\ntakes its time.',
    'afterwards':'When words leave,\ntheir warmth stays.\nAn empty page\nis also a sky.'
  };
  function contentKey(poem){
    const text=(poem.text+'\n'+poem.by).replace(/[\u2018\u2019\u201a\u201b]/g,"'")
      .replace(/[\u201c\u201d\u201e\u201f]/g,'"').replace(/[\u2010-\u2014\u2212]/g,'-')
      .replace(/\u2026/g,'...').replace(/\s+/g,' ').trim();
    return text;
  }
  function hasLibraryShape(data){
    return data&&typeof data==='object'&&Array.isArray(data.poems)&&data.poems.length>0
      &&Array.isArray(data.animations)&&data.animations.length>0
      &&data.poems.every(p=>p&&typeof p.id==='string'&&typeof p.text==='string'&&typeof p.by==='string')
      &&data.animations.every(a=>a&&typeof a.id==='string');
  }
  function reconcile(base,saved){
    const result=clone(base),ids=new Set(result.poems.map(p=>p.id)),contents=new Set(result.poems.map(contentKey));
    for(const poem of saved.poems){
      const unchangedDemo=poem.by==='Original sample'&&DEMOS[poem.id]===poem.text;
      if(ids.has(poem.id)||contents.has(contentKey(poem))||unchangedDemo)continue;
      result.poems.push(clone(poem));ids.add(poem.id);contents.add(contentKey(poem));
    }
    const animations=new Map(result.animations.map(a=>[a.id,a]));
    for(const animation of saved.animations)animations.set(animation.id,{...animations.get(animation.id),...clone(animation)});
    result.animations=[...animations.values()];
    return result;
  }
  function updateSavedMotion(base,saved){
    const library=clone(saved),canonical=new Map(base.animations.map(animation=>[animation.id,animation]));
    let changed=false;
    library.animations=library.animations.map(animation=>{
      const current=canonical.get(animation.id);
      if(current&&Object.prototype.hasOwnProperty.call(LEGACY_PRESET_SPEEDS,animation.id)){
        const legacy={...current,speed:LEGACY_PRESET_SPEEDS[animation.id]},keys=Object.keys(legacy);
        // Only untouched old built-ins inherit new defaults. Saved variations
        // and edited built-ins retain all their parameters, subject to the floor.
        if(keys.length===Object.keys(animation).length&&keys.every(key=>animation[key]===legacy[key])){
          if(keys.some(key=>animation[key]!==current[key])){changed=true;return clone(current);}
        }
      }
      if(typeof animation.speed==='number'&&Number.isFinite(animation.speed)&&animation.speed>=.1&&animation.speed<MIN_SPEED){
        animation.speed=MIN_SPEED;changed=true;
      }
      return animation;
    });
    return{library,changed};
  }
  function loadLibrary(base,revision,storage){
    const fallback=clone(base);let raw;
    try{raw=storage.getItem(STORAGE_KEY);}catch(error){return{library:fallback,notice:'Loaded your database. Browser storage could not be read.',migrated:false};}
    if(raw===null||raw===undefined)return{library:fallback,notice:'',migrated:false};
    let library=fallback,notice='Loaded your database; previous browser library is backed up.';
    try{
      const parsed=JSON.parse(raw),wrapped=parsed&&Object.prototype.hasOwnProperty.call(parsed,'sourceRevision');
      const saved=wrapped?parsed.library:parsed;
      if(!hasLibraryShape(saved))throw Error('Invalid library structure');
      const motion=updateSavedMotion(base,saved);
      if(wrapped&&parsed.sourceRevision===revision){
        if(!motion.changed)return{library:motion.library,notice:'',migrated:false};
        library=motion.library;
      }else library=reconcile(base,motion.library);
    }catch(error){notice='Loaded your database; the unreadable browser library is backed up.';}
    try{storage.setItem(BACKUP_KEY,raw);}catch(error){return{library,notice:'Loaded your database in memory. Browser backup failed; the previous saved library was left intact.',migrated:false};}
    // The caller validates before saving the reconciled value to the primary key.
    return{library,notice,migrated:true};
  }
  function saveLibrary(library,revision,storage){
    storage.setItem(STORAGE_KEY,JSON.stringify({sourceRevision:revision,library}));
  }
  const api={STORAGE_KEY,BACKUP_KEY,loadLibrary,saveLibrary};
  if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.PoetryLibraryStore=api;
})(globalThis);
