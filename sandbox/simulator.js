'use strict';
const $=selector=>document.querySelector(selector);
const {Experience,KINDS,LIMITS,FORMATIONS,FORMATION_LABELS,LAYOUTS,LAYOUT_LABELS,validateLibrary,project,shade}=PoetryEngine;
const STORAGE_KEY='airplane-poetry-library-v1';
let library=structuredClone(POETRY_DATA),storageNotice='';
try{
  const saved=localStorage.getItem(STORAGE_KEY);
  if(saved){
    const data=JSON.parse(saved);
    if(!data.version||data.version===1){
      // Upgrade only unchanged shipped presets; keep the user's own variations.
      const old=[['orbit',.55,76,45,.45,.15],['helix',.5,70,65,.6,.2],['wave',.65,82,35,.35,.1],['vortex',.6,75,60,.5,.25],['drift',.45,80,40,.3,.12],['tumble',.5,68,55,.7,.4]];
      for(const entry of old){const i=data.animations?.findIndex(a=>a.id===entry[0]);if(i>=0){const m=data.animations[i],base=POETRY_DATA.animations.find(a=>a.id===m.id);if(base&&m.kind===base.kind&&m.name===base.name&&['speed','spread','depth','tilt','spin'].every((key,k)=>m[key]===entry[k+1])&&m.perspective===360&&!m.unit)data.animations[i]={...POETRY_DATA.animations.find(a=>a.id===m.id)};}}
      for(const preset of POETRY_DATA.animations.filter(a=>a.kind>=6))if(!data.animations?.some(a=>a.id===preset.id))data.animations?.push({...preset});
    }
    if(!data.version||data.version<3){
      for(const p of data.poems??[]){const base=POETRY_DATA.poems.find(x=>x.id===p.id);if(base&&p.text===base.text&&p.by===base.by&&(!p.layout||p.layout==='centered'))p.layout='random';}
      for(const m of data.animations??[]){const base=POETRY_DATA.animations.find(x=>x.id===m.id);if(base&&m.kind===base.kind&&m.name===base.name&&(!m.unit||m.unit==='letters'))m.unit='mixed';}
    }
    library=validateLibrary(data);
  }
}catch(error){storageNotice='The saved library could not be loaded. Showing the original samples. Your saved copy is retained until you save or import a new library.';}
const experience=new Experience(library);
const canvas=$('#lcd'),ctx=canvas.getContext('2d',{alpha:false});
let last=performance.now(),lastMode='',lastPoem='',lastMotion='',lastCycle=experience.cycle,lastLayout='',feedbackTimeout;
function feedback(message,error=false){$('#feedback').textContent=message;$('#feedback').classList.toggle('error',error);clearTimeout(feedbackTimeout);if(!error)feedbackTimeout=setTimeout(()=>$('#feedback').textContent='',7000);}
function persist(){try{localStorage.setItem(STORAGE_KEY,JSON.stringify(library));return true;}catch(error){feedback('Browser storage is unavailable. Export your library to keep these changes.',true);return false;}}
function id(prefix){return prefix+'-'+Date.now().toString(36)+'-'+Math.random().toString(36).slice(2,6);}
function glyphs(text,callback){for(let c=0;c<text.length;c++){const glyph=POETRY_FONT[text.charCodeAt(c)-32];if(!glyph)continue;for(let col=0;col<5;col++){const bits=glyph[col];for(let row=0;row<8;){if(!(bits&(1<<row))){row++;continue;}const first=row;while(row<8&&(bits&(1<<row)))row++;callback(c*6+col,first,1,row-first);}}}}
function grey(amount){const g=Math.floor(Math.max(0,Math.min(1,amount))*255);return `rgb(${(g>>3)*255/31},${(g>>2)*255/63},${(g>>3)*255/31})`;}
function author(opacity){ctx.fillStyle=grey(.38*opacity);const text=experience.poem.by,x=(240-text.length*6)/2,y=experience.arrangement.authorY;glyphs(text,(gx,gy,w,h)=>ctx.fillRect(Math.trunc(x+gx),Math.trunc(y+gy),w,h));}
function render(){
  ctx.fillStyle='#000';ctx.fillRect(0,0,240,320);
  // Both poems share one depth-sorted space during a change.
  const layers=experience.layers();
  const order=layers.flatMap(layer=>layer.poses.map((pose,i)=>({pose,unit:experience.units(layer.scene)[i],layer}))).sort((a,b)=>a.pose.z-b.pose.z);
  for(const {pose,unit,layer} of order){const {mode,progress,scene,opacity}=layer,size=unit.size,width=unit.text.length*6*size;
    const settling=pose.progress??progress;
    let brightness=shade(pose.z,scene.motion.depth);
    if(mode==='ASSEMBLING')brightness+=(.9-brightness)*settling;
    if(mode==='READING')brightness=.9;
    if(mode==='DISSOLVING')brightness=.9+(brightness-.9)*settling;
    brightness*=opacity;
    if(unit.echo){brightness*=.78;if(mode==='ASSEMBLING')brightness*=1-settling;else if(mode==='READING')brightness=0;else if(mode==='DISSOLVING')brightness*=settling;}
    if(brightness<.003)continue;
    ctx.fillStyle=grey(brightness);
    glyphs(unit.text,(gx,gy,w,h)=>{
      const x=gx*size-width/2,y=gy*size-4*size;
      if(mode==='READING'){ctx.fillRect(Math.trunc(120+pose.x+x),Math.trunc(160+pose.y+y),w*size,h*size);return;}
      const corners=[[x,y],[x+w*size,y],[x+w*size,y+h*size],[x,y+h*size]].map(([px,py])=>project(px,py,pose,scene.motion.perspective));
      if(corners.some(p=>!p||!p.every(Number.isFinite)))return;
      if(corners.every(([x])=>x<0)||corners.every(([x])=>x>240)||corners.every(([,y])=>y<0)||corners.every(([,y])=>y>320))return;
      ctx.beginPath();corners.forEach(([x,y],i)=>i?ctx.lineTo(x,y):ctx.moveTo(x,y));ctx.closePath();ctx.fill();
    });
  }
  const current=layers[0];
  if(current.mode!=='MOVING')author(current.mode==='READING'?1:current.mode==='ASSEMBLING'?current.progress:1-current.progress);
}
const icons=[
 '<ellipse cx="17" cy="16" rx="15" ry="7" transform="rotate(-35 17 16)"/><ellipse cx="17" cy="16" rx="15" ry="7" transform="rotate(35 17 16)"/>',
 '<path d="M8 1C37 5-3 12 26 16S-3 27 26 31M26 1C-3 5 37 12 8 16S37 27 8 31"/>',
 '<path d="M1 10Q9-1 17 10T33 10M1 17Q9 6 17 17T33 17M1 24Q9 13 17 24T33 24"/>',
 '<path d="M17 16c-8-10 13-13 11-1S6 30 4 17 21-4 30 7 21 33 12 28"/>',
 '<path d="M2 23Q12 11 18 18T32 8M4 8Q16 19 26 4M12 30Q17 20 30 24"/>',
 '<path d="M5 9l16-6 10 13-16 7zM5 9v13l10 8V23m16-7v11l-16 3M21 3v13L5 22m16-6 10 11"/>',
 '<circle cx="17" cy="16" r="13"/><ellipse cx="17" cy="16" rx="6" ry="13"/><ellipse cx="17" cy="16" rx="13" ry="5"/><path d="M5 9h24M5 23h24"/>',
 '<path d="M2 11Q15-2 32 11M2 16Q15 3 32 16M2 21Q15 8 32 21M2 26Q15 13 32 26"/>'
];
function presetList(){const container=$('#presets');container.replaceChildren();library.animations.forEach((motion,i)=>{const b=document.createElement('button');b.className='preset';b.dataset.id=motion.id;b.type='button';b.innerHTML=`<svg viewBox="0 0 34 32" aria-hidden="true">${icons[motion.kind]}</svg>`;const name=document.createElement('span');name.textContent=motion.name;b.append(name);b.setAttribute('aria-label',`Preview ${motion.name}`);b.onclick=()=>{experience.selectAnimation(i);syncMotion();syncState();};container.append(b);});}
function poemList(){const select=$('#poem-select');select.replaceChildren();library.poems.forEach((poem,i)=>{const o=document.createElement('option');o.value=i;o.textContent=poem.title||poem.id;select.append(o);});$('#poem-count').textContent=library.poems.length;syncPoem();}
const parameterLabels={speed:'Speed',spread:'Spread',depth:'Depth',tilt:'3D tilt',spin:'Spin',perspective:'Perspective'};
const format=(key,value)=>key==='tilt'?Math.round(value*180/Math.PI)+'°':key==='speed'?value.toFixed(2)+'×':key==='spin'?value.toFixed(2):Math.round(value)+' px';
for(const [i,name] of FORMATIONS.entries()){const o=document.createElement('option');o.value=name;o.textContent=FORMATION_LABELS[i];$('#formation').append(o);}
for(const [i,name] of LAYOUTS.entries()){const o=document.createElement('option');o.value=name;o.textContent=LAYOUT_LABELS[i];$('#reading-layout').append(o);}
for(const [kind,name] of KINDS.entries()){const o=document.createElement('option');o.value=kind;o.textContent=name;$('#kind').append(o);}
for(const [key,[min,max]] of Object.entries(LIMITS)){const label=document.createElement('label');label.className='parameter';const text=document.createElement('span');text.textContent=parameterLabels[key];const input=document.createElement('input');input.type='range';input.id='param-'+key;input.min=min;input.max=max;input.step=['spread','depth','perspective'].includes(key)?1:.01;input.setAttribute('aria-label',parameterLabels[key]);const output=document.createElement('output');output.htmlFor=input.id;output.id='value-'+key;label.append(text,input,output);$('#parameters').append(label);input.oninput=()=>{experience.motion[key]=Number(input.value);output.textContent=format(key,Number(input.value));experience.resumeMotion();syncState();};}
function syncMotion(){const m=experience.motion;$('#kind').value=m.kind;$('#formation').value=m.formation;$('#unit').value=m.unit;for(const key of Object.keys(LIMITS)){$('#param-'+key).value=m[key];$('#value-'+key).textContent=format(key,m[key]);}document.querySelectorAll('.preset').forEach(b=>{const active=b.dataset.id===m.id;b.classList.toggle('active',active);b.setAttribute('aria-pressed',active);});$('#style-name').value='';$('#remove-style').hidden=POETRY_DATA.animations.some(a=>a.id===m.id)||library.animations.length<2;lastMotion=m.id;}
function syncPoem(){$('#poem-select').value=experience.poemIndex;const p=experience.poem;$('#poem-title').value=p.title;$('#poem-by').value=p.by;$('#poem-text').value=p.text;$('#reading-layout').value=p.layout;lastPoem=p.id;}
function syncPickedLayout(){const name=experience.arrangement.layoutName;$('#layout-picked').textContent='This time: '+LAYOUT_LABELS[LAYOUTS.indexOf(name)];lastLayout=name;}
function syncState(){syncPickedLayout();const mode=experience.mode;const messages={MOVING:['In motion','Touch the display to assemble the poem.'],ASSEMBLING:['Coming into focus','The letters settle into a poem.'],READING:['A moment to read','Stay as long as you like. Touch for another poem.'],DISSOLVING:['In between','One poem becomes another.']};$('#state').textContent=messages[mode][0];$('#hint').textContent=messages[mode][1];canvas.setAttribute('aria-label',`Poetry display. ${mode==='READING'?experience.poem.text+' By '+experience.poem.by+'. ':''}${messages[mode][1]}`);canvas.setAttribute('aria-disabled',mode==='ASSEMBLING'||mode==='DISSOLVING');lastMode=mode;}
function touch(){experience.touch();syncState();}
canvas.addEventListener('pointerdown',event=>{if(event.isPrimary===false)return;event.preventDefault();canvas.focus({preventScroll:true});touch();});canvas.addEventListener('keydown',event=>{if(event.key==='Enter'||event.key===' '){event.preventDefault();if(!event.repeat)touch();}});
$('#shuffle').onclick=()=>{experience.pick();syncMotion();syncPoem();syncState();};
$('#kind').onchange=()=>{experience.motion.kind=Number($('#kind').value);experience.resumeMotion();syncState();};
$('#formation').onchange=()=>{experience.motion.formation=$('#formation').value;experience.resumeMotion();syncState();};
$('#unit').onchange=()=>{experience.motion.unit=$('#unit').value;experience.resumeMotion();syncState();};
$('#reading-layout').onchange=()=>{try{experience.previewLayout($('#reading-layout').value);syncState();}catch(error){$('#reading-layout').value=experience.poem.layout;feedback(error.message,true);}};
$('#shuffle-layout').onclick=()=>{try{experience.previewLayout('random');$('#reading-layout').value='random';syncState();}catch(error){feedback(error.message,true);}};
$('#reset-motion').onclick=()=>{experience.selectAnimation(experience.animationIndex);syncMotion();syncState();};
$('#save-style').onclick=()=>{try{const name=$('#style-name').value.trim();if(!name)throw Error('Give your variation a name first.');const motion={...experience.motion,id:id('style'),name};validateLibrary({...library,animations:[...library.animations,motion]});library.animations.push(motion);experience.selectAnimation(library.animations.length-1);presetList();syncMotion();syncState();if(persist())feedback(`Saved “${name}”. It will appear in the random rotation.`);}catch(error){feedback(error.message,true);}};
$('#remove-style').onclick=()=>{if(library.animations.length<2)return;library.animations.splice(experience.animationIndex,1);experience.selectAnimation(0);presetList();syncMotion();syncState();if(persist())feedback('Removed the saved variation.');};
function selectTab(name){for(const panel of ['motion','library']){const active=panel===name;$('#'+panel+'-tab').classList.toggle('active',active);$('#'+panel+'-tab').setAttribute('aria-selected',active);$('#'+panel+'-panel').hidden=!active;}}
$('#motion-tab').onclick=()=>selectTab('motion');$('#library-tab').onclick=()=>selectTab('library');
$('#poem-select').onchange=()=>{experience.selectPoem(Number($('#poem-select').value));syncPoem();syncState();};
function savePoem(asNew){try{const p={id:asNew?id('poem'):experience.poem.id,title:$('#poem-title').value.trim(),by:$('#poem-by').value.trim(),text:$('#poem-text').value,layout:$('#reading-layout').value};const poems=library.poems.slice();if(asNew)poems.push(p);else poems[experience.poemIndex]=p;validateLibrary({...library,poems});library.poems=poems;experience.selectPoem(asNew?poems.length-1:experience.poemIndex);poemList();syncState();if(persist())feedback(asNew?'Added your poem to the collection.':'Saved your poem.');}catch(error){feedback(error.message,true);}}
$('#update-poem').onclick=()=>savePoem(false);$('#add-poem').onclick=()=>savePoem(true);
$('#export').onclick=()=>{const blob=new Blob([JSON.stringify(library,null,2)+'\n'],{type:'application/json'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download='poetry.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);feedback('Exported your poetry and animation library.');};
$('#import').onchange=async event=>{try{const file=event.target.files[0];if(!file)return;if(file.size>1024*1024)throw Error('Choose a library smaller than 1 MB.');const imported=validateLibrary(JSON.parse(await file.text()));library=imported;experience.library=library;experience.poemIndex=experience.animationIndex=-1;experience.pick();presetList();poemList();syncMotion();syncState();if(persist())feedback('Imported your library.');}catch(error){feedback(error.message,true);}event.target.value='';};
function frame(now){experience.step((now-last)/1000);last=now;render();if(lastCycle!==experience.cycle){syncMotion();syncPoem();lastCycle=experience.cycle;}if(lastLayout!==experience.arrangement.layoutName)syncPickedLayout();if(lastMode!==experience.mode)syncState();if(lastPoem!==experience.poem.id)syncPoem();if(lastMotion!==experience.motion.id)syncMotion();requestAnimationFrame(frame);}
presetList();poemList();syncMotion();syncState();if(storageNotice)feedback(storageNotice,true);requestAnimationFrame(frame);
