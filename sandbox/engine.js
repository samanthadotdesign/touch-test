/* Motion, layout and touch behavior mirror the firmware in src/poetry_*.h. */
(function (root) {
  'use strict';
  const FORMATIONS=['cluster','stream','patches'];
  const FORMATION_LABELS=['Condensed cloud','Typographic stream','Fragment islands'];
  const KIND_FORMATIONS=['cluster','stream','stream','cluster','patches','patches','cluster','stream'];
  const KINDS = ['Orbit', 'Helix', 'Wave', 'Vortex', 'Drift', 'Tumble', 'Cloud', 'Ribbon'];
  const LIMITS = {speed:[.1,2],spread:[20,100],depth:[0,180],tilt:[0,1.2],spin:[0,1],perspective:[240,800]};
  const MOTION_TEMPO=2;
  const ASSEMBLE_SECONDS=.55, CHANGE_SECONDS=.65;
  const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
  // Zero acceleration at both ends, while the underlying field keeps moving.
  const ease=v=>{v=clamp(v,0,1);return v*v*v*(v*(v*6-15)+10);};
  const angle=v=>Math.atan2(Math.sin(v),Math.cos(v));
  const mix=(a,b,t)=>a+(b-a)*t;
  const shade=(z,depth)=>.12+.84*Math.pow(clamp(.5+z/(2*Math.max(depth,1)),0,1),1.35);
  const LAYOUTS = ['centered','left','stepped','constellation','cascade','split','wave','columns','stepped-reverse','stepped-zigzag','split-mirrored','split-three'];
  const LAYOUT_LABELS = ['Centered verse','Left aligned','Diagonal steps','Constellation','Word cascade','Stanza islands','Wavy lines','Twin columns','Reverse steps','Zigzag steps','Mirrored islands','Three islands'];
  function displayText(value){
    const aliases={"\u2018":"'","\u2019":"'","\u201a":"'","\u201b":"'","\u201c":'"',"\u201d":'"',"\u201e":'"',"\u201f":'"',"\u2010":"-","\u2011":"-","\u2012":"-","\u2013":"-","\u2014":"-","\u2212":"-","\u00a0":" ","\u202f":" ","\u2026":"..."};
    return value.replace(/[\u2018-\u201f\u2010-\u2014\u2212\u00a0\u202f\u2026]/g,c=>aliases[c]);
  }
  function layout(text,readingLayout='centered') {
    text=displayText(text);
    if(readingLayout==='random'){
      for(const candidate of LAYOUTS){try{return layout(text,candidate);}catch(error){/* try another composition */}}
      throw Error('This poem does not fit any reading layout.');
    }
    if(!LAYOUTS.includes(readingLayout))throw Error('Choose a supported reading layout.');
    if(text.length>512||text.trim().split(/\s+/).length>96)throw Error('This poem exceeds the display text limit.');
    const size=1,advance=6,pitch=14,lines=[[]];let width=0,sourceOffset=0;
    for(const [index,paragraph] of text.split('\n').entries()) {
      if(index){lines.push([]);width=0;}
      for(const match of paragraph.matchAll(/[^ ]+/g)) {
        const word=match[0],w=word.length*advance;
        if(w>168)throw Error('A word is too long for the small display (28 letters maximum).');
        if(width&&width+advance+w>168){lines.push([]);width=0;}
        const offset=width?width+advance:0;
        lines.at(-1).push({text:word,offset,width:w,sourceOffset:sourceOffset+match.index});width=offset+w;
      }
      sourceOffset+=paragraph.length+1;
    }
    const widths=lines.map(line=>line.length?line.at(-1).offset+line.at(-1).width:0);
    const words=[];lines.forEach((line,i)=>line.forEach(word=>words.push({...word,size,line:i})));
    const n=words.length,l=lines.length;if(l>97)throw Error('This poem has too many line breaks.');if(!n)throw Error('A poem needs some words.');
    const fail=()=>{throw Error('This poem does not fit this composition. Choose Random for a layout that fits.');};
    let authorY=296;
    if(['centered','left','stepped','stepped-reverse','stepped-zigzag'].includes(readingLayout)){
      const indent=i=>readingLayout==='stepped'?i*8:readingLayout==='stepped-reverse'?(l-1-i)*8:readingLayout==='stepped-zigzag'?(i%4<=2?i%4:4-i%4)*12:0;
      const groupWidth=Math.max(...widths.map((w,i)=>w+indent(i)));
      const bodyH=l*pitch-6,totalH=bodyH+28;if(totalH>288||groupWidth>216)fail();
      const top=(320-totalH)/2;authorY=top+bodyH+20;
      for(const word of words){const i=word.line,left=readingLayout==='centered'?(240-widths[i])/2:(240-groupWidth)/2+indent(i);word.x=left+word.offset+word.width/2;word.y=top+i*pitch+4;}
    }else if(readingLayout==='constellation'){
      const rows=Math.ceil(n/3),cw=[0,0,0];words.forEach((w,i)=>cw[i%3]=Math.max(cw[i%3],w.width));const totalW=cw.reduce((a,b)=>a+b,0)+36;if(rows>12||totalW>216)fail();
      const span=rows>1?Math.min(232,(rows-1)*50):0,top=(288-span)/2;
      words.forEach((w,i)=>{const col=i%3,row=Math.floor(i/3);w.x=(240-totalW)/2+cw.slice(0,col).reduce((a,b)=>a+b+18,0)+cw[col]/2+Math.sin(row*1.7+col)*4;w.y=top+(rows>1?row*span/(rows-1):0)+(col===1?8:0);});
    }else if(readingLayout==='cascade'){
      if(n>20)fail();words.forEach((w,i)=>{const u=n>1?i/(n-1):.5;w.x=clamp(120+(u-.5)*110+Math.sin(u*Math.PI*3)*18,12+w.width/2,228-w.width/2);w.y=40+u*224;});
    }else if(['split','split-mirrored'].includes(readingLayout)){
      if(l<2)fail();const first=Math.ceil(l/2),h1=first*14-6,h2=(l-first)*14-6,total=h1+48+h2;if(total>248)fail();const top=(280-total)/2+8;
      for(const w of words){const lower=w.line>=first;const right=readingLayout==='split'?lower:!lower;w.x=(right?222-widths[w.line]:18)+w.offset+w.width/2;w.y=lower?top+h1+48+(w.line-first)*14+4:top+w.line*14+4;}
    }else if(readingLayout==='split-three'){
      if(l<3)fail();const counts=[0,0,0];for(let i=0;i<l;i++)counts[Math.floor(i*3/l)]++;
      const heights=counts.map(n=>n*14-6),total=heights.reduce((a,b)=>a+b,0)+64;if(total>248)fail();const top=(280-total)/2+8;
      const starts=[0,counts[0],counts[0]+counts[1]];
      for(const w of words){const g=Math.floor(w.line*3/l),left=g===0?18:g===1?(240-widths[w.line])/2:222-widths[w.line];w.x=left+w.offset+w.width/2;w.y=top+heights.slice(0,g).reduce((a,b)=>a+b+32,0)+(w.line-starts[g])*14+4;}
    }else if(readingLayout==='wave'){
      if(l>7)fail();const body=(l-1)*32+22,top=(280-body)/2+8;
      for(const w of words){w.x=120+Math.sin(w.line*.9)*18-widths[w.line]/2+w.offset+w.width/2;w.y=top+w.line*32+11+Math.sin(((w.offset+w.width/2)/Math.max(widths[w.line],1))*2*Math.PI+w.line*.6)*7;}
    }else if(readingLayout==='columns'){
      const half=Math.ceil(n/2);if(half>17||n-half<1)fail();const left=Math.max(...words.slice(0,half).map(w=>w.width)),right=Math.max(...words.slice(half).map(w=>w.width)),total=left+24+right;if(total>216)fail();const h=(half-1)*14+8,top=(280-h)/2+8;
      words.forEach((w,i)=>{w.x=(240-total)/2+(i<half?left/2:left+24+right/2);w.y=top+(i%half)*14+4;});
    }
    const letters=[];
    for(const word of words){word.x-=120;word.y-=160;for(let c=0;c<word.text.length;c++)letters.push({text:word.text[c],size,width:6,x:word.x-word.width/2+c*6+3,y:word.y,line:word.line,sourceOffset:word.sourceOffset+c});}
    return {words,letters,authorY,size,layoutName:readingLayout,text};
  }
  function particleRandom(seed,index,channel){
    let x=(seed^Math.imul(index+1,0x9e3779b9)^Math.imul(channel+1,0x85ebca6b))>>>0;
    x^=x>>>16;x=Math.imul(x,0x7feb352d);x^=x>>>15;x=Math.imul(x,0x846ca68b);x^=x>>>16;
    return (x>>>8)/16777216;
  }
  function particleProgress(seed,index,age,assembling){
    const delay=(assembling?.08:.07)*particleRandom(seed,index,assembling?20:22);
    const duration=(assembling?.34:.40)+(assembling?.13:.17)*particleRandom(seed,index,assembling?21:23);
    return ease((age-delay)/duration);
  }
  function buildUnits(arrangement,unit,seed=0){
    arrangement.groups??={};const key=unit+':'+seed;if(arrangement.groups[key])return arrangement.groups[key];
    let base;
    if(unit==='words')base=arrangement.words.map(w=>({...w,echo:false}));
    else if(unit==='letters')base=arrangement.letters.map(w=>({...w,echo:false}));
    else{
      const whole=arrangement.words.map((w,i)=>w.text.length>1&&particleRandom(seed,i,35)>.62),eligible=arrangement.words.map((w,i)=>w.text.length>1?i:-1).filter(i=>i>=0);
      if(eligible.length>=2){if(!eligible.some(i=>whole[i]))whole[eligible[0]]=true;if(eligible.every(i=>whole[i]))whole[eligible.at(-1)]=false;}
      base=[];arrangement.words.forEach((word,i)=>{if(whole[i])base.push({...word,echo:false});else for(let c=0;c<word.text.length;c++)base.push({text:word.text[c],size:1,width:6,x:word.x-word.width/2+c*6+3,y:word.y,line:word.line,sourceOffset:word.sourceOffset+c,echo:false});});
    }
    const groups=base.slice(),realCount=base.length;
    for(let i=realCount;i<Math.max(180,realCount);i++){
      const source=base[Math.min(realCount-1,Math.floor(particleRandom(seed,i,37)*realCount))],echo={...source,echo:true};
      if(unit!=='letters'&&particleRandom(seed,i,38)<.68){
        let run=0;while(run<18&&source.sourceOffset+run<arrangement.text.length&&arrangement.text[source.sourceOffset+run]!=='\n')run++;
        const skip=Math.floor(particleRandom(seed,i,39)*Math.min(source.text.length,4));
        let offset=source.sourceOffset+skip,length=1+Math.floor(particleRandom(seed,i,40)*Math.min(run-skip,14));
        while(length>0&&arrangement.text[offset]===' '){offset++;length--;}
        while(length>0&&arrangement.text[offset+length-1]===' ')length--;
        if(length>0){echo.x+=(offset-source.sourceOffset)*6+(length-source.text.length)*3;echo.sourceOffset=offset;echo.text=arrangement.text.slice(offset,offset+length);echo.width=length*6;}
      }
      groups.push(echo);
    }
    return arrangement.groups[key]=groups;
  }
  function validateLibrary(data) {
    if(!data||!Array.isArray(data.poems)||!data.poems.length||!Array.isArray(data.animations)||!data.animations.length)throw Error('A library needs poems and animations arrays, each with at least one entry.');
    if(data.version!==undefined&&data.version!==1&&data.version!==2&&data.version!==3&&data.version!==4)throw Error('Unsupported library version. Use version 1, 2, 3, or 4.');
    for(const [name,entries] of [['poems',data.poems],['animations',data.animations]]) {
      const ids=new Set();
      for(const entry of entries){if(!entry||typeof entry.id!=='string'||!/^[a-z0-9][a-z0-9_-]{0,59}$/.test(entry.id)||ids.has(entry.id))throw Error(`${name}: each entry needs a unique lowercase id.`);ids.add(entry.id);}
    }
    for(const p of data.poems) {
      for(const key of ['id','text','by'])if(typeof p[key]!=='string'||!p[key].trim())throw Error(`Poem ${p.id}: ${key} is required.`);
      if(!/^[\x20-\x7e\n]+$/.test(displayText(p.text))||!/^[\x20-\x7e]+$/.test(displayText(p.by)))throw Error('The device font cannot render one of these characters. Smart quotes and dashes are supported.');
      if(typeof p.title!=='string'||! /^[\x20-\x7e]*$/.test(displayText(p.title)))throw Error('Title must use supported text or be empty.');
      if(displayText(p.text).length>512||displayText(p.text).trim().split(/\s+/).length>96||displayText(p.by).length>35||displayText(p.title).length>60)throw Error('Poems allow 512 characters / 96 words, titles 60 characters, and authors 35 characters.');
      if(p.layout===undefined)p.layout='random';layout(p.text,p.layout);
    }
    for(const a of data.animations){if(typeof a.name!=='string'||!a.name.trim()||a.name.length>60||!/^[\x20-\x7e]+$/.test(a.name))throw Error('Animation names need 1–60 simple Latin letters or punctuation.');
      if(!Number.isInteger(a.kind)||a.kind<0||a.kind>7)throw Error('Animation kind must be 0–7.');
      for(const [key,[min,max]] of Object.entries(LIMITS))if(typeof a[key]!=='number'||!Number.isFinite(a[key])||a[key]<min||a[key]>max)throw Error(`${a.name}: ${key} must be between ${min} and ${max}.`);
      if(a.formation===undefined)a.formation=KIND_FORMATIONS[a.kind];if(!FORMATIONS.includes(a.formation))throw Error('Choose a condensed cloud, typographic stream, or fragment islands distribution.');
      if(a.unit===undefined)a.unit='mixed';if(!['letters','words','mixed'].includes(a.unit))throw Error('Choose mixed words and letters, letters, or whole words for motion.');
    }
    data.version=4;return data;
  }
  function pose(index,count,seconds,m) {
    const u=(index+.5)/count,p=index*2.39996323,t=seconds*m.speed,s=m.spread,d=m.depth,l=m.tilt,k=m.spin;
    let x,y,z,rx,ry,rz,a,r,h,row,col,rows;
    switch(m.kind){
      case 0:a=p+t*.45;x=Math.cos(a)*s*(.35+.55*u);y=Math.sin(a*.8+p*.3)*s*.85;z=Math.sin(a)*d;rx=Math.sin(a*.7)*l;ry=Math.cos(a*.6)*l;rz=Math.sin(a*.4)*k;break;
      case 1:a=p+t*.65;x=Math.cos(a)*s*.7;y=(u*2-1)*s*1.1+Math.sin(t*.4+p)*8;z=Math.sin(a)*d;rx=Math.sin(a)*l*.4;ry=Math.sin(a)*l;rz=Math.sin(a*.6)*k;break;
      case 2:x=(2*u-1)*s;y=Math.sin(p+t)*s*.65;z=Math.cos(p+t)*d;rx=Math.cos(p+t)*l;ry=Math.sin(p+t*.7)*l;rz=Math.cos(p+t*.5)*k;break;
      case 3:a=p+t*(.25+u*.6);r=s*(.18+.72*u);x=Math.cos(a)*r;y=Math.sin(a)*r*1.25;z=Math.sin(p-t*.6)*d;rx=Math.sin(a)*l;ry=Math.cos(a)*l;rz=a*k;break;
      case 4:x=Math.sin(p+t*.23)*s;y=Math.cos(p*.7+t*.19)*s*1.15;z=Math.sin(p*.4+t*.31)*d;rx=Math.sin(p+t*.3)*l;ry=Math.cos(p+t*.27)*l;rz=Math.sin(p+t*.2)*k;break;
      case 5:x=Math.cos(p+t*.4)*s*.85;y=Math.sin(p*.7+t*.3)*s;z=Math.sin(p+t*.5)*d;rx=(t*.45+p)*l;ry=(t*.6+p)*l;rz=(t*.3+p)*k;break;
      case 6:h=1-2*u;r=Math.sqrt(Math.max(0,1-h*h));a=p+t*.28;x=Math.cos(a)*r*s;y=h*s*1.15+Math.sin(a*.7)*5;z=Math.sin(a)*r*d;rx=Math.sin(p+t*.2)*l*.35;ry=Math.sin(a)*l;rz=Math.sin(t*.2+p)*k;break;
      case 7:rows=Math.ceil(count/18);row=Math.floor(index/18);col=index%18;a=(col-8.5)*.12+t*.25;x=Math.sin(a)*s;y=(row-(rows-1)/2)*12+Math.cos(a*.6+t*.3)*s*.25;z=Math.sin(a)*d;rx=l*.35;ry=Math.sin(a)*l*.8;rz=-.18+k*Math.sin(t*.25);break;
    }
    return {x,y,z,rx,ry,rz};
  }
  function fieldPose(index,count,seconds,m,width=6,seed=0){
    seconds*=MOTION_TEMPO;
    const r=channel=>particleRandom(seed,index,channel),tau=Math.PI*2;
    const rate=.35+1.4*r(3),direction=r(4)<.5?-1:1,phaseX=r(0)*tau,phaseY=r(1)*tau,phaseZ=r(2)*tau;
    const v=pose(index,count,seconds*rate*direction+phaseX*18,m),t=seconds*m.speed;
    v.z=.65*v.z+.35*m.depth*Math.sin(t*(.15+.45*r(11))+phaseZ);
    v.rx=.45*Math.sin(v.rx)+.15*m.tilt*Math.sin(t*rate*.15+phaseZ);
    v.ry=.45*Math.sin(v.ry)+.15*m.tilt*Math.cos(t*rate*.13+phaseX);
    v.rz=.12*Math.sin(v.rz)+.08*m.spin*Math.sin(t*rate*.11+phaseY);
    const minimumScale=.55*1.20;
    v.scale=minimumScale+(1.1-minimumScale)*r(27)*r(27);
    const gx=r(14)+r(15)+r(16)-1.5,gy=r(17)+r(18)+r(19)-1.5;
    const formation=m.formation??KIND_FORMATIONS[m.kind];let sx,sy;
    if(formation==='cluster'){
      const globalPhase=particleRandom(seed,0,60)*tau,baseX=(particleRandom(seed,0,61)*2-1)*62,baseY=(particleRandom(seed,0,62)*2-1)*94;
      const cx=baseX+18*Math.sin(t*.07+globalPhase),cy=baseY+14*Math.cos(t*.05+globalPhase);
      const tailAngle=(Math.hypot(baseX,baseY)<12?particleRandom(seed,0,63)*tau:Math.atan2(-baseY,-baseX))+(particleRandom(seed,0,64)*2-1)*.4;
      if(r(13)<.74){sx=cx+gx*(10+38*r(24)*r(24));sy=cy+gy*(12+28*r(25)*r(25));}
      else if(r(13)<.92){const u=Math.pow(r(24),1.6);sx=cx+Math.cos(tailAngle)*180*u+gx*(8+14*u);sy=cy+Math.sin(tailAngle)*225*u+gy*(9+22*u);}
      else{sx=(2*r(14)-1)*110;sy=(2*r(17)-1)*146;}
    }else if(formation==='stream'){
      let u=r(28)<.70?(Math.floor(r(13)*6)+.5)/6+.038*(r(14)+r(15)-1):r(13);u+=.018*Math.sin(t*.14+phaseX);
      sx=45*Math.sin(u*Math.PI*2.7-.7+t*.05)+(u-.5)*85+gx*(4+16*r(25)*r(25));
      sy=(u-.5)*284+gy*(3+9*r(24));
      if(r(28)>.83)sx+=(r(29)<.5?-1:1)*(30+35*r(30));
    }else{
      const group=Math.floor(r(13)*6),cx=(group%2?52:-52)+12*Math.sin(group*1.7+particleRandom(seed,0,60)*tau),cy=-118+group*46,row=Math.floor(r(14)*4);
      sx=cx+(r(15)-.5)*74;sy=cy+(row-1.5)*8+(r(16)-.5)*3;
      if(r(28)<.12){sx=(2*r(29)-1)*110;sy=(2*r(30)-1)*146;}
    }
    sx+=Math.sin(t*(.22+.28*r(8))+phaseX)*(5+4*r(31));sy+=Math.cos(t*(.20+.28*r(10))+phaseY)*(5+4*r(32));
    const q=m.perspective/(m.perspective-v.z),limitX=Math.max(6,116-Math.min(104,width*v.scale*q*.55)),limitY=Math.max(20,154-4*v.scale*q);
    v.x=clamp(sx,-limitX,limitX)/q;v.y=clamp(sy,-limitY,limitY)/q;return v;
  }
  function project(x,y,position,camera) {
    const {rx,ry,rz}=position;const scale=position.scale??1;x*=scale;y*=scale;
    const y1=y*Math.cos(rx),z1=y*Math.sin(rx);
    const x2=x*Math.cos(ry)+z1*Math.sin(ry),z2=-x*Math.sin(ry)+z1*Math.cos(ry);
    const x3=x2*Math.cos(rz)-y1*Math.sin(rz),y3=x2*Math.sin(rz)+y1*Math.cos(rz);
    const z=position.z+z2;
    if(camera-z<24)return null;
    const q=camera/(camera-z);return [120+(position.x+x3)*q,160+(position.y+y3)*q];
  }
  class Experience {
    constructor(library,random=Math.random){this.library=validateLibrary(library);this.random=random;this.blowForce=0;this.poemIndex=-1;this.animationIndex=-1;this.pick();}
    different(count,last){if(count<2)return 0;if(last<0)return Math.floor(this.random()*count);const n=Math.floor(this.random()*(count-1));return n>=last?n+1:n;}
    chooseLayout(poem){if(poem.layout!=='random')return layout(poem.text,poem.layout);const eligible=LAYOUTS.flatMap(name=>{try{return[layout(poem.text,name)];}catch(error){return[];}});const choices=eligible.length>1?eligible.filter(a=>a.layoutName!==this.arrangement?.layoutName):eligible;return choices[Math.floor(this.random()*choices.length)];}
    makeScene(poemIndex,animationIndex){const poem=this.library.poems[poemIndex],motion={...this.library.animations[animationIndex]};return{poemIndex,animationIndex,poem,motion,arrangement:this.chooseLayout(poem),time:0,seed:Math.floor(this.random()*4294967296)>>>0};}
    adopt(scene){this.poemIndex=scene.poemIndex;this.animationIndex=scene.animationIndex;this.poem=scene.poem;this.motion=scene.motion;this.arrangement=scene.arrangement;this.time=scene.time;this.seed=scene.seed;this.mode='MOVING';this.age=0;this.next=null;this.cycle=(this.cycle||0)+1;}
    pick(){this.adopt(this.makeScene(this.different(this.library.poems.length,this.poemIndex),this.different(this.library.animations.length,this.animationIndex)));}
    selectPoem(index){this.adopt(this.makeScene(index,this.animationIndex));}
    selectAnimation(index){this.adopt(this.makeScene(this.poemIndex,index));}
    resumeMotion(){this.mode='MOVING';this.age=0;this.next=null;}
    previewLayout(value){const poem={...this.poem,layout:value},arrangement=this.chooseLayout(poem),reading=this.mode==='READING';this.poem=poem;this.arrangement=arrangement;this.resumeMotion();if(reading)this.mode='READING';}
    units(scene=this){return buildUnits(scene.arrangement,scene.motion.unit,scene.seed);}
    moving(scene=this){const units=this.units(scene),force=this.blowForce;return units.map((unit,i)=>{
      const position=fieldPose(i,units.length,scene.time,scene.motion,unit.width,scene.seed);if(!force)return position;
      // Expand around the display centre in screen pixels, independent of depth.
      const q=scene.motion.perspective/(scene.motion.perspective-position.z),sx=position.x*q,sy=position.y*q,distance=Math.hypot(sx,sy);
      const phase=distance>1e-6?Math.atan2(sy,sx):particleRandom(scene.seed,i,80)*Math.PI*2;
      // A stable individual fan breaks up offset knots while every ray still points outwards.
      const fan=(particleRandom(scene.seed,i,82)*2-1)*.95,nx=Math.cos(phase+fan),ny=Math.sin(phase+fan);
      const limitX=Math.max(6,116-Math.min(104,unit.width*position.scale*q*.55)),limitY=Math.max(20,154-4*position.scale*q);
      const roomX=Math.abs(nx)>1e-6?((nx>0?limitX:-limitX)-sx)/nx:Infinity,roomY=Math.abs(ny)>1e-6?((ny>0?limitY:-limitY)-sy)/ny:Infinity;
      const headroom=Math.max(0,Math.min(roomX,roomY)),push=force*(45+60*particleRandom(scene.seed,i,81));
      // Approach the safe edge softly; a strong breath keeps the field visible.
      const outward=headroom>0?-headroom*Math.expm1(-push/headroom):0;
      position.x+=nx*outward/q;position.y+=ny*outward/q;return position;
    });}
    poses(){
      const moving=this.moving(),units=this.units();
      if(this.mode==='MOVING')return moving;
      return units.map((unit,i)=>{
        const target={x:unit.x,y:unit.y,z:0,rx:0,ry:0,rz:0,scale:1};if(this.mode==='READING')return target;
        const p=particleProgress(this.seed,i,this.age,this.mode==='ASSEMBLING');
        const dynamic={...moving[i]};for(const k of ['rx','ry','rz'])dynamic[k]=this.anchors[i][k]+angle(dynamic[k]-this.anchors[i][k]);
        return {...Object.fromEntries(Object.keys(target).map(k=>[k,this.mode==='ASSEMBLING'?mix(dynamic[k],target[k],p):mix(target[k],dynamic[k],p)])),progress:p};
      });
    }
    layers(){
      const p=ease(this.age/(this.mode==='ASSEMBLING'?ASSEMBLE_SECONDS:CHANGE_SECONDS));
      const current={scene:this,poses:this.poses(),mode:this.mode,progress:p,opacity:this.mode==='DISSOLVING'?1-p:1};
      return this.mode==='DISSOLVING'?[current,{scene:this.next,poses:this.moving(this.next),mode:'MOVING',progress:0,opacity:p}]:[current];
    }
    touch(){
      if(this.mode!=='MOVING'&&this.mode!=='READING')return false;
      this.anchors=this.moving().map(p=>({...p,rx:angle(p.rx),ry:angle(p.ry),rz:angle(p.rz)}));
      if(this.mode==='MOVING'){this.mode='ASSEMBLING';this.age=0;return true;}
      this.next=this.makeScene(this.different(this.library.poems.length,this.poemIndex),this.different(this.library.animations.length,this.animationIndex));this.mode='DISSOLVING';this.age=0;return true;
    }
    step(dt,motionScale=1,blowForce=0){this.blowForce=clamp(Number.isFinite(blowForce)?blowForce:0,0,1);dt=clamp(dt,0,.1);const motionDt=dt*clamp(Number.isFinite(motionScale)?motionScale:1,0,5);this.time+=motionDt;this.age+=dt;if(this.next)this.next.time+=motionDt;
      if(this.mode==='ASSEMBLING'&&this.age>=ASSEMBLE_SECONDS){this.mode='READING';this.age=0;}
      if(this.mode==='DISSOLVING'&&this.age>=CHANGE_SECONDS)this.adopt(this.next);
    }
  }
  const api={MOTION_TEMPO,KINDS,LIMITS,FORMATIONS,FORMATION_LABELS,LAYOUTS,LAYOUT_LABELS,displayText,layout,particleRandom,particleProgress,buildUnits,validateLibrary,pose,fieldPose,project,Experience,ease,shade,ASSEMBLE_SECONDS,CHANGE_SECONDS};
  if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.PoetryEngine=api;
})(globalThis);
