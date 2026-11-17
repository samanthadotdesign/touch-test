const {test}=require('node:test');
const assert=require('node:assert/strict');
const {STORAGE_KEY,BACKUP_KEY,loadLibrary,saveLibrary}=require('../sandbox/library-store.js');
const poem=(id,text='Your poem',by='Author')=>({id,title:id,text,by,layout:'random'});
const base=()=>({version:4,poems:Array.from({length:5},(_,i)=>poem('database-'+i,'Database poem '+i)),animations:[{id:'orbit',name:'Orbit',speed:.84},{id:'cloud',name:'Cloud',speed:.68}]});
function storage(initial){const values=new Map(Object.entries(initial||{}));return{getItem:key=>values.has(key)?values.get(key):null,setItem:(key,value)=>values.set(key,value),values};}
const clone=value=>JSON.parse(JSON.stringify(value));
test('legacy samples yield to the five database entries and the old value is backed up',()=>{
 const old={poems:[...require('./fixtures/poems.json'),poem('between-rain','Between the rain\nand the window,\na quiet world\nlearns to shimmer.','Original sample'),poem('unhurried','Let the day\nopen slowly.\nEven the flower\ntakes its time.','Original sample'),poem('afterwards','When words leave,\ntheir warmth stays.\nAn empty page\nis also a sky.','Original sample')],animations:[{id:'orbit',name:'Orbit',speed:.2}]};
 const raw=JSON.stringify(old),s=storage({[STORAGE_KEY]:raw}),b=base(),before=clone(b),result=loadLibrary(b,'revision-b',s);
 assert.equal(result.library.poems.length,5);assert.deepEqual(result.library.poems,b.poems);assert.equal(result.migrated,true);assert.equal(s.getItem(BACKUP_KEY),raw);assert.equal(s.getItem(STORAGE_KEY),raw);assert.deepEqual(b,before);assert.equal(result.library.animations[0].speed,.68);assert.equal(result.library.animations.length,2);
});
test('custom poems, edited demos and saved animation variations survive migration',()=>{
 const saved={poems:[poem('mine'),poem('small-light','My edited sample','Original sample'),poem('database-0','Old canonical text')],animations:[{id:'custom',name:'Slow drift',speed:.1}]};
 const result=loadLibrary(base(),'new',storage({[STORAGE_KEY]:JSON.stringify(saved)}));
 assert.equal(result.library.poems.length,7);assert.equal(result.library.poems[0].text,'Database poem 0');assert.ok(result.library.poems.some(p=>p.id==='mine'));assert.ok(result.library.poems.some(p=>p.text==='My edited sample'));assert.ok(result.library.animations.some(a=>a.id==='custom'));
});
test('renamed legacy quote identifiers do not duplicate matching database text',()=>{
 const b=base();b.poems[0]=poem('new-id',"Others' lives.",'Author');const saved={poems:[poem('old-id','Others’ lives.','Author')],animations:b.animations};
 assert.equal(loadLibrary(b,'new',storage({[STORAGE_KEY]:JSON.stringify(saved)})).library.poems.length,5);
});
test('same source revision preserves explicit local edits after saving',()=>{
 const s=storage(),edited=base();edited.poems[0].text='Edited in this browser';saveLibrary(edited,'same',s);
 const result=loadLibrary(base(),'same',s);assert.equal(result.library.poems[0].text,'Edited in this browser');assert.equal(result.migrated,false);assert.equal(s.getItem(BACKUP_KEY),null);result.library.poems[0].text='Changed after load';assert.equal(JSON.parse(s.getItem(STORAGE_KEY)).library.poems[0].text,'Edited in this browser');
});
test('a changed source revision refreshes canonical entries and keeps custom extras',()=>{
 const s=storage(),old=base();old.poems[0].text='Old source override';old.poems.push(poem('custom'));saveLibrary(old,'old',s);const raw=s.getItem(STORAGE_KEY),next=base();next.poems[0].text='Updated database';
 const result=loadLibrary(next,'new',s);assert.equal(result.library.poems[0].text,'Updated database');assert.ok(result.library.poems.some(p=>p.id==='custom'));assert.equal(s.getItem(BACKUP_KEY),raw);assert.equal(s.getItem(STORAGE_KEY),raw);
});
test('malformed JSON and malformed library objects recover the source and retain raw backups',()=>{
 for(const raw of ['{broken',JSON.stringify({poems:[],animations:[]}),JSON.stringify({sourceRevision:'r',library:{}})]){const s=storage({[STORAGE_KEY]:raw}),result=loadLibrary(base(),'r',s);assert.deepEqual(result.library,base());assert.equal(result.migrated,true);assert.equal(s.getItem(BACKUP_KEY),raw);assert.equal(s.getItem(STORAGE_KEY),raw);assert.match(result.notice,/unreadable/);}
});
test('backup failure leaves the primary storage untouched and does not request persistence',()=>{
 const raw=JSON.stringify(base()),s=storage({[STORAGE_KEY]:raw});s.setItem=()=>{throw Error('quota');};const result=loadLibrary(base(),'next',s);assert.equal(result.migrated,false);assert.equal(s.getItem(STORAGE_KEY),raw);assert.match(result.notice,/backup failed/);assert.throws(()=>saveLibrary(base(),'next',s));
});
test('empty storage and read failures still show the source database',()=>{
 assert.deepEqual(loadLibrary(base(),'r',storage()).library,base());const result=loadLibrary(base(),'r',{getItem(){throw Error('denied');}});assert.deepEqual(result.library,base());assert.equal(result.migrated,false);assert.match(result.notice,/could not be read/);
});
test('untouched old built-ins adopt new defaults once, including with the same source revision',()=>{
 const current=clone(require('../data/poetry.json'));
 const oldSpeeds={orbit:.55,helix:.5,wave:.65,vortex:.6,drift:.45,tumble:.5,cloud:.45,ribbon:.55};
 for(const oldRevision of ['old-revision','current-revision']){
  const previous=clone(current);previous.animations.forEach(a=>a.speed=oldSpeeds[a.id]);
  if(oldRevision==='current-revision')previous.poems[0].text='An explicit local poem edit';
  const s=storage();saveLibrary(previous,oldRevision,s);const raw=s.getItem(STORAGE_KEY),before=clone(current);
  const result=loadLibrary(current,'current-revision',s);
  assert.deepEqual(result.library.animations,current.animations);assert.equal(result.migrated,true);
  assert.equal(s.getItem(BACKUP_KEY),raw);assert.equal(s.getItem(STORAGE_KEY),raw);assert.deepEqual(current,before);
  assert.equal(result.library.poems[0].text,oldRevision==='current-revision'?'An explicit local poem edit':current.poems[0].text);
  saveLibrary(result.library,'current-revision',s);
  const again=loadLibrary(current,'current-revision',s);assert.equal(again.migrated,false);assert.deepEqual(again.library,result.library);
 }
});
test('edited built-ins and custom styles keep their parameters apart from the speed floor',()=>{
 const current=clone(require('../data/poetry.json')),previous=clone(current);
 const edited=previous.animations[0];edited.speed=.55;edited.spread=82;edited.name='My edited orbit';
 previous.animations[1].speed=1.25;
 previous.animations.push({...current.animations[0],id:'my-slow-style',name:'Slow variation',speed:.15});
 previous.animations.push({...current.animations[0],id:'my-fast-style',name:'Fast variation',speed:1.6});
 const s=storage();saveLibrary(previous,'older-source',s);
 const result=loadLibrary(current,'newer-source',s).library;
 assert.deepEqual(result.animations.find(a=>a.id==='orbit'),{...edited,speed:.68});
 assert.deepEqual(result.animations.find(a=>a.id==='helix'),previous.animations[1]);
 assert.deepEqual(result.animations.find(a=>a.id==='my-slow-style'),{...previous.animations.at(-2),speed:.68});
 assert.deepEqual(result.animations.find(a=>a.id==='my-fast-style'),previous.animations.at(-1));
});
