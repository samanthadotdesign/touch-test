const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const {Experience,layout,pose,fieldPose,particleProgress,particleRandom,FORMATIONS,LAYOUTS,buildUnits,project,validateLibrary,displayText,shade,ASSEMBLE_SECONDS,CHANGE_SECONDS}=require('../sandbox/engine.js');
const actualLibrary=JSON.parse(fs.readFileSync(require('node:path').join(__dirname,'../data/poetry.json'),'utf8'));
const source={...actualLibrary,poems:require('./fixtures/poems.json')};
const clone=()=>JSON.parse(JSON.stringify(source));
function advance(e,seconds){for(let t=0;t<Math.ceil(seconds*100);t++)e.step(.01);}
const close=(a,b,epsilon=1e-5)=>assert.ok(Math.abs(a-b)<epsilon,`${a} != ${b}`);
test('moving and reading stay touch-driven; transitions complete in .55/.65 seconds',()=>{
 const e=new Experience(clone(),()=>.4);advance(e,3600);assert.equal(e.mode,'MOVING');
 assert.equal(e.touch(),true);assert.equal(e.touch(),false);advance(e,.54);assert.equal(e.mode,'ASSEMBLING');advance(e,.02);assert.equal(e.mode,'READING');
 advance(e,3600);assert.equal(e.mode,'READING');const before=[e.poem.id,e.motion.id];e.touch();assert.equal(e.mode,'DISSOLVING');assert.notEqual(e.next.poem.id,e.poem.id);assert.equal(e.touch(),false);
 advance(e,.64);assert.equal(e.mode,'DISSOLVING');advance(e,.02);assert.equal(e.mode,'MOVING');assert.notEqual(e.poem.id,before[0]);assert.notEqual(e.motion.id,before[1]);
 assert.equal(ASSEMBLE_SECONDS,.55);assert.equal(CHANGE_SECONDS,.65);
});
test('mixed words and letters fill the field, echoes disappear from the reading corpus',()=>{
 const e=new Experience(clone());assert.equal(e.motion.unit,'mixed');const units=e.units(),real=units.filter(u=>!u.echo);assert.equal(units.length,180);assert.ok(real.some(u=>u.text.length>1));assert.ok(real.some(u=>u.text.length===1));assert.equal(real.map(u=>u.text).join(''),e.poem.text.replace(/[ \n]/g,''));assert.ok(units.some(u=>u.echo));
 e.motion.unit='letters';assert.equal(e.units().filter(u=>!u.echo).length,e.poem.text.replace(/[ \n]/g,'').length);
 e.motion.unit='words';assert.equal(e.units().filter(u=>!u.echo).length,e.poem.text.trim().split(/\s+/).length);
});
test('assembly carries motion through touch and settles without residual transforms',()=>{
 const e=new Experience(clone(),()=>.5);advance(e,10);const count=e.units().length,t=e.time,m=e.motion;
 const before=e.poses();e.touch();const atTouch=e.poses();for(let i=0;i<count;i++){for(const key of ['x','y','z'])close(before[i][key],atTouch[i][key]);}
 e.step(.0001);const after=e.poses(),natural=fieldPose(0,count,t+.0001,m,e.units()[0].width,e.seed);for(const key of ['x','y','z'])close(after[0][key],natural[key],1e-6);
 advance(e,.56);assert.equal(e.mode,'READING');for(const [i,p] of e.poses().entries()){close(p.x,e.units()[i].x);close(p.y,e.units()[i].y);for(const key of ['z','rx','ry','rz'])assert.equal(p[key],0);}
});
test('poem changes overlap in one 3D field and preserve incoming motion time',()=>{
 const e=new Experience(clone(),()=>.3);e.touch();advance(e,.6);e.touch();
 let layers=e.layers();assert.equal(layers.length,2);assert.equal(layers[0].opacity,1);assert.equal(layers[1].opacity,0);
 for(let i=0;i<64;i++){e.step(.01);layers=e.layers();assert.equal(layers.length,2);close(layers[0].opacity+layers[1].opacity,1);assert.ok(layers[0].opacity>0&&layers[1].opacity>0);assert.ok(layers.every(l=>l.poses.length>0));}
 const incoming=e.next;e.step(.01);assert.equal(e.mode,'MOVING');assert.equal(e.poem.id,incoming.poem.id);close(e.time,.65);assert.deepEqual(e.poses(),e.moving(incoming));
});
test('random picks avoid immediate repeats and single-entry libraries still work',()=>{
 const e=new Experience(clone(),()=>.999999);assert.equal(e.poemIndex,source.poems.length-1);assert.equal(e.animationIndex,source.animations.length-1);
 for(let i=0;i<50;i++){const last=[e.poem.id,e.motion.id];e.pick();assert.notEqual(e.poem.id,last[0]);assert.notEqual(e.motion.id,last[1]);}
 const data=clone();data.poems=data.poems.slice(0,1);data.animations=data.animations.slice(0,1);const single=new Experience(data);single.touch();advance(single,.6);single.touch();advance(single,.7);assert.equal(single.mode,'MOVING');
});
test('small font, explicit blank lines, and all three reading layouts',()=>{
 const text='a b\n\nthree';const centered=layout(text);assert.equal(centered.size,1);assert.equal(centered.words.at(-1).y-centered.words[0].y,28);
 for(const mode of ['centered','left','stepped']){const l=layout('small\na longer line\nlast',mode),rows=[0,1,2].map(i=>l.words.filter(w=>w.line===i));
  const starts=rows.map(r=>r[0].x-r[0].width/2);
  if(mode==='centered')rows.forEach(r=>close(r[0].x-r[0].width/2,-r.at(-1).x-r.at(-1).width/2));
  if(mode==='left'){close(starts[0],starts[1]);close(starts[1],starts[2]);}
  if(mode==='stepped'){close(starts[1]-starts[0],8);close(starts[2]-starts[1],8);}
 }
 assert.throws(()=>layout('x'.repeat(29)));assert.throws(()=>layout(Array(20).fill('word').join('\n')));
 for(const p of source.poems){const l=layout(p.text,p.layout);assert.equal(l.size,1);assert.ok(l.words.every(w=>w.x-w.width/2>=-108&&w.x+w.width/2<=108));}
});
test('all eight motion families project safely and depth shades are perceptibly different',()=>{
 for(const motion of source.animations){assert.notDeepEqual(pose(3,60,0,motion),pose(3,60,10,motion));
  for(const t of [0,10,1000])for(let i=0;i<60;i++){const p=pose(i,60,t,{...motion,depth:180,tilt:1.2,spin:1});for(const x of [-3,3])for(const y of [-4,4]){const projected=project(x,y,p,240);assert.ok(projected===null||projected.every(Number.isFinite));}}
 }
 close(shade(-130,130),.12);close(shade(130,130),.96);assert.ok(shade(0,130)>.4&&shade(0,130)<.5);
 assert.ok(shade(-65,130)<shade(0,130));assert.ok(shade(65,130)>shade(0,130));
 const p={x:40,y:0,z:100,rx:0,ry:0,rz:0};assert.ok(project(0,0,p,240)[0]>project(0,0,p,800)[0]);assert.equal(project(0,0,{...p,z:230},240),null);
});
test('legacy libraries migrate, valid exports round trip, and invalid content is rejected',()=>{
 const data=clone();delete data.version;for(const p of data.poems)delete p.layout;for(const a of data.animations)delete a.unit;
 validateLibrary(data);assert.equal(data.version,4);assert.equal(data.poems[0].layout,'random');assert.equal(data.animations[0].unit,'mixed');
 data.poems.push({id:'custom-poem',title:'A quote',text:'Be here.\nBe still.',by:'Your name',layout:'stepped'});data.animations.push({...data.animations[0],id:'custom-style',name:'Slow orbit',speed:.15,depth:180,unit:'words'});
 const exported=validateLibrary(JSON.parse(JSON.stringify(data)));assert.equal(exported.poems.at(-1).layout,'stepped');assert.equal(exported.animations.at(-1).unit,'words');
 for(const mutate of [d=>d.poems[0].text='Unsupported 🌙 symbol',d=>d.poems[0].id='Invalid ID',d=>d.animations[0].name='é',d=>d.animations[0].perspective=0,d=>d.animations[0].kind=8,d=>d.animations[0].depth=181,d=>d.animations[0].unit='paragraphs',d=>d.poems[0].layout='unknown',d=>d.animations[0].speed=NaN,d=>d.poems.push(d.poems[0]),d=>d.version=5]){const invalid=clone();mutate(invalid);assert.throws(()=>validateLibrary(invalid));}
});
test('maximum supported body expands to bounded independent glyphs',()=>{
 const text=Array(19).fill('a'.repeat(26)).join('\n');assert.equal(text.length,512);const l=layout(text);assert.equal(l.letters.length,494);assert.ok(l.authorY+8<=304);
});

test('all twelve static layouts preserve every word and stay within the screen',()=>{
 for(const poem of source.poems)for(const name of LAYOUTS){const a=layout(poem.text,name);assert.equal(a.words.map(w=>w.text).join(' '),poem.text.trim().split(/\s+/).join(' '));for(const w of a.words){assert.ok(w.x-w.width/2>=-112.01,`${name} left`);assert.ok(w.x+w.width/2<=112.01,`${name} right`);assert.ok(w.y-4>=-152,`${name} top`);assert.ok(w.y+4<128,`${name} bottom`);}
  for(let i=0;i<a.words.length;i++)for(let j=i+1;j<a.words.length;j++){const x=a.words[i],y=a.words[j];const overlapX=Math.min(x.x+x.width/2,y.x+y.width/2)-Math.max(x.x-x.width/2,y.x-y.width/2),overlapY=Math.min(x.y+4,y.y+4)-Math.max(x.y-4,y.y-4);assert.ok(overlapX<=.001||overlapY<=.001,`${name} overlaps ${x.text}/${y.text}`);}}
});
test('random compositions are fixed within a scene and change on the next scene',()=>{
 const e=new Experience(clone(),()=>.6);const selected=e.arrangement.layoutName;assert.ok(LAYOUTS.includes(selected));advance(e,1);assert.equal(e.arrangement.layoutName,selected);e.touch();advance(e,.6);assert.equal(e.arrangement.layoutName,selected);e.touch();const next=e.next.arrangement.layoutName;assert.notEqual(next,selected);advance(e,.7);assert.equal(e.arrangement.layoutName,next);
 const dense=Array(19).fill('a'.repeat(26)).join('\n');const lib=clone();lib.poems=[{id:'dense',title:'Dense',text:dense,by:'Sample',layout:'random'}];const long=new Experience(lib);assert.ok(LAYOUTS.includes(long.arrangement.layoutName));assert.doesNotThrow(()=>layout(dense,long.arrangement.layoutName));assert.equal(long.arrangement.letters.length,494);
});
test('reference distributions create dense formations with sparse outliers',()=>{
 const m=source.animations[0],samples={};
 for(const formation of FORMATIONS){const p=Array.from({length:180},(_,i)=>project(0,0,fieldPose(i,180,2,{...m,formation},6,123456),m.perspective));samples[formation]=p;assert.ok(p.every(([x,y])=>x>=0&&x<240&&y>=0&&y<320));}
 const cluster=samples.cluster,core=cluster.filter((_,i)=>particleRandom(123456,i,13)<.74),cx=core.reduce((s,p)=>s+p[0],0)/core.length,cy=core.reduce((s,p)=>s+p[1],0)/core.length;assert.ok(core.filter(([x,y])=>Math.hypot(x-cx,y-cy)<55).length>=100);assert.ok(cluster.some(([x,y])=>Math.hypot(x-cx,y-cy)>90));
 const stream=samples.stream,ys=stream.map(p=>p[1]);assert.ok(Math.max(...ys)-Math.min(...ys)>250);assert.ok(stream.filter(([x,y])=>y<100).length>25);assert.ok(stream.filter(([x,y])=>y>220).length>25);
 const patches=samples.patches;assert.ok(patches.filter(([x])=>x<110).length>50);assert.ok(patches.filter(([x])=>x>130).length>50);assert.ok(patches.some(([,y])=>y<65)&&patches.some(([,y])=>y>255));
 assert.notDeepEqual(samples.cluster,samples.stream);assert.notDeepEqual(samples.stream,samples.patches);
});
test('excess blank lines are rejected consistently with the firmware parser',()=>{assert.throws(()=>layout('\n'.repeat(97)+'word','constellation'));const a=layout('\n'.repeat(96)+'word','constellation');assert.equal(a.words.length,1);});

test('seeded fragments move in many directions and at independent speeds',()=>{
 const m=source.animations[0],quadrants=[0,0,0,0],speeds=[];
 for(let i=0;i<120;i++){const a=fieldPose(i,120,3,m,6,123456),b=fieldPose(i,120,3.01,m,6,123456),p=project(0,0,a,m.perspective),q=project(0,0,b,m.perspective),dx=q[0]-p[0],dy=q[1]-p[1];quadrants[(dx<0?1:0)+(dy<0?2:0)]++;speeds.push(Math.hypot(dx,dy));assert.deepEqual(a,fieldPose(i,120,3,m,6,123456));}
 assert.ok(quadrants.every(n=>n>=12));assert.ok(Math.max(...speeds)/Math.min(...speeds)>5);assert.notDeepEqual(fieldPose(0,120,3,m,6,1),fieldPose(0,120,3,m,6,2));
 const steps=Array.from({length:120},(_,i)=>particleProgress(123456,i,.22,true));assert.ok(Math.max(...steps)-Math.min(...steps)>.35);
 for(let i=0;i<120;i++){assert.equal(particleProgress(123456,i,.55,true),1);assert.equal(particleProgress(123456,i,.65,false),1);}
});
test('mixed grouping varies with the scene seed but always retains the exact poem',()=>{
 const a=layout(source.poems[0].text),one=buildUnits(a,'mixed',1),two=buildUnits(a,'mixed',2);assert.notDeepEqual(one.filter(u=>!u.echo).map(u=>u.text),two.filter(u=>!u.echo).map(u=>u.text));
 for(const groups of [one,two]){assert.equal(groups.filter(u=>!u.echo).map(u=>u.text).join(''),source.poems[0].text.replace(/[ \n]/g,''));assert.equal(groups.length,180);assert.ok(groups.some(u=>u.text.length>1));assert.ok(groups.some(u=>u.text.length===1));}
});
test('new step and island variants have distinct arrangements and preserve source order',()=>{
 const text='one two\nthree four\nfive six\nseven eight';const positions=name=>layout(text,name).words.map(w=>[w.x,w.y]);
 assert.notDeepEqual(positions('stepped'),positions('stepped-reverse'));assert.notDeepEqual(positions('stepped'),positions('stepped-zigzag'));assert.notDeepEqual(positions('split'),positions('split-mirrored'));assert.notDeepEqual(positions('split'),positions('split-three'));
 for(const name of ['stepped-reverse','stepped-zigzag','split-mirrored','split-three'])assert.equal(layout(text,name).words.map(w=>w.text).join(' '),'one two three four five six seven eight');
});

test('fragments vary scale, contain short phrases, and settle at the original type size',()=>{
 const e=new Experience(clone(),()=>.5),units=e.units(),poses=e.poses();assert.ok(units.some(u=>u.echo&&u.text.includes(' ')));assert.ok(units.every(u=>!u.text.includes('\n')));assert.ok(poses.some(p=>p.scale<.7));assert.ok(poses.some(p=>p.scale>1));assert.ok(poses.every(p=>p.scale<=1.1));e.touch();advance(e,.56);assert.ok(e.poses().every(p=>p.scale===1));
 e.motion.unit='letters';assert.ok(e.units().every(u=>u.text.length===1));
});
test('current editable poetry library remains valid independently of the test corpus',()=>{assert.doesNotThrow(()=>validateLibrary(JSON.parse(JSON.stringify(actualLibrary))));});

test('formation settings migrate and round trip without overriding explicit choices',()=>{const d=clone();d.version=3;for(const a of d.animations)delete a.formation;validateLibrary(d);assert.deepEqual(d.animations.map(a=>a.formation),['cluster','stream','stream','cluster','patches','patches','cluster','stream']);d.animations[0].formation='patches';assert.equal(validateLibrary(JSON.parse(JSON.stringify(d))).animations[0].formation,'patches');const invalid=clone();invalid.animations[0].formation='uniform';assert.throws(()=>validateLibrary(invalid));});

test('condensed cloud centers vary between scenes while drifting continuously',()=>{const m={...source.animations[0],formation:'cluster'},centers=[];for(const seed of Array.from({length:32},(_,i)=>i)){const center=time=>{const points=Array.from({length:180},(_,i)=>i).filter(i=>particleRandom(seed,i,13)<.74).map(i=>project(0,0,fieldPose(i,180,time,m,6,seed),m.perspective));return[points.reduce((s,p)=>s+p[0],0)/points.length,points.reduce((s,p)=>s+p[1],0)/points.length];};const a=center(0),b=center(.05);assert.ok(Math.hypot(a[0]-b[0],a[1]-b[1])<1);centers.push(a);}assert.ok(Math.max(...centers.map(p=>p[0]))-Math.min(...centers.map(p=>p[0]))>70);assert.ok(Math.max(...centers.map(p=>p[1]))-Math.min(...centers.map(p=>p[1]))>100);});

test('smart punctuation renders with device glyph aliases while source text stays intact',()=>{const d=clone(),original='Others’ lives—“a story”…';d.poems[0].text=original;d.poems[0].title='';assert.doesNotThrow(()=>validateLibrary(d));assert.equal(d.poems[0].text,original);assert.equal(d.poems[0].title,'');assert.equal(displayText(original),`Others' lives-"a story"...`);assert.equal(layout(original).words.map(w=>w.text).join(' '),`Others' lives-"a story"...`);const e=new Experience(d,()=>0);e.selectPoem(0);assert.equal(e.poem.text,original);assert.ok(e.units().every(u=>/^[\x20-\x7e]+$/.test(u.text)));});
