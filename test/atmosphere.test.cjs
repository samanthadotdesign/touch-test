const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const {levelFromSamples,smoothLevel,motionScale,createBlowGate,THEME_KEY,COLOURWAY_KEY,HIDE_DELAY}=require('../sandbox/atmosphere.js');

function deferred(){let resolve,reject;const promise=new Promise((yes,no)=>{resolve=yes;reject=no;});return{promise,resolve,reject};}

// The controller runs unchanged; only browser DOM, clock, and audio devices are supplied.
function atmosphereHarness({saved=null,savedColourway=null,storage,random=()=>.5,blockedStorage=false,getUserMedia,resumeAudio,onBlowResult=true}={}){
 const listeners=new Map(),timers=new Map(),writes=[],reads=[],contexts=[],requests=[];
 const storageValues=storage||new Map([[THEME_KEY,saved],[COLOURWAY_KEY,savedColourway]]);
 let now=0,nextTimer=0,blows=0,sampleValue=0;
 const document={activeElement:null,hidden:false};
 function element(name){
  const events=new Map(),attributes=new Map();
  return{name,events,attributes,dataset:{},style:{},hidden:false,disabled:false,textContent:'',value:'0',
   addEventListener:(type,handler)=>{const list=events.get(type)||[];list.push(handler);events.set(type,list);},
   setAttribute:(key,value)=>attributes.set(key,String(value)),
   focus(){document.activeElement=this;},
   dispatch(type,properties={}){let prevented=false;const event={preventDefault(){prevented=true;},...properties};for(const handler of events.get(type)||[])handler(event);return prevented;}
  };
 }
 const screen=element('screen'),hotspot=element('hotspot'),sheet=element('sheet'),backdrop=element('backdrop'),close=element('close');
 const choices=['dark','light','colour'].map(theme=>{const node=element(theme);node.dataset.theme=theme;return node;});
 const mic=element('mic'),status=element('status'),meter=element('meter'),bar=element('bar'),preview=element('preview'),output=element('output'),notice=element('notice'),threshold=element('threshold'),thresholdOutput=element('threshold-output'),blowFeedback=element('blow-feedback'),marker=element('marker');
 threshold.value='10';thresholdOutput.textContent='10%';
 sheet.hidden=backdrop.hidden=notice.hidden=true;screen.insertAdjacentHTML=()=>{};
 screen.querySelector=selector=>({'.atmosphere-hotspot':hotspot,'.atmosphere-sheet':sheet,'.atmosphere-backdrop':backdrop}[selector]);
 sheet.querySelector=selector=>{assert.equal(selector,'.sheet-close');return close;};
 sheet.querySelectorAll=selector=>selector==='.theme-choice'?choices:selector==='button:not(:disabled)'?[close,...choices,mic].filter(node=>!node.disabled):[];
 sheet.contains=node=>[sheet,close,...choices,mic].includes(node);
 meter.querySelector=selector=>{assert.equal(selector,'i');return bar;};
 document.querySelector=selector=>({'#sound-preview':preview,'#sound-level':output,'#blow-threshold':threshold,'#blow-threshold-value':thresholdOutput,'#blow-feedback':blowFeedback,'.blow-threshold-marker':marker}[selector]);
 document.querySelectorAll=selector=>({'[data-mic-toggle]':[mic],'[data-mic-status]':[status],'[data-mic-meter]':[meter],'[data-shader-status]':[notice],'.blow-threshold-marker':[marker]}[selector]||[]);
 class AudioContext{
  constructor(){this.state='suspended';this.closed=0;this.source={connect:node=>{this.connected=node;},disconnect:()=>{this.sourceDisconnected=true;}};this.analyser={fftSize:0,disconnect:()=>{this.analyserDisconnected=true;},getFloatTimeDomainData:samples=>samples.fill(sampleValue)};contexts.push(this);}
  resume(){this.state='running';return resumeAudio?resumeAudio(this):Promise.resolve();}
  close(){this.state='closed';this.closed++;return Promise.resolve();}
  createAnalyser(){return this.analyser;}
  createMediaStreamSource(){return this.source;}
 }
 const math=Object.create(Math);math.random=random;
 const environment={document,AudioContext,Float32Array,Math:math,module:{exports:{}},
  navigator:{mediaDevices:{getUserMedia:constraints=>{requests.push(constraints);return getUserMedia?getUserMedia(constraints):Promise.reject(Object.assign(Error('Denied'),{name:'NotAllowedError'}));}}},
  localStorage:{getItem:key=>{reads.push(key);if(blockedStorage)throw Error('denied');return storageValues.get(key)??null;},setItem:(key,value)=>{if(blockedStorage)throw Error('denied');writes.push([key,value]);storageValues.set(key,value);}},
  addEventListener:(type,handler)=>{const list=listeners.get(type)||[];list.push(handler);listeners.set(type,list);},
  setTimeout:(callback,delay)=>{const id=++nextTimer;timers.set(id,{callback,at:now+delay});return id;},clearTimeout:id=>timers.delete(id)
 };
 vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../sandbox/atmosphere.js'),'utf8'),environment);
 const controller=environment.module.exports.createController(screen,{onBlow:()=>{blows++;return onBlowResult;}});
 return{controller,screen,hotspot,sheet,backdrop,close,choices,mic,status,meter,bar,preview,output,notice,threshold,thresholdOutput,blowFeedback,marker,writes,reads,storage:storageValues,contexts,requests,document,
  get blows(){return blows;},get pendingTimers(){return timers.size;},
  sample:value=>{sampleValue=value;},
  dispatch:(type,properties={})=>{for(const handler of listeners.get(type)||[])handler(properties);},
  advance(milliseconds){const until=now+milliseconds;while(true){const next=[...timers].filter(([,timer])=>timer.at<=until).sort((a,b)=>a[1].at-b[1].at)[0];if(!next)break;timers.delete(next[0]);now=next[1].at;next[1].callback();}now=until;}
 };
}
function audioStream(){const trackEvents=new Map(),track={stops:0,stop(){this.stops++;},addEventListener:(type,handler)=>trackEvents.set(type,handler)};return{track,getTracks:()=>[track],getAudioTracks:()=>[track],end:()=>trackEvents.get('ended')?.()};}

test('sound level measures RMS above a noise floor and clamps to a useful range',()=>{
 assert.equal(levelFromSamples([]),0);assert.equal(levelFromSamples([0,0,0]),0);assert.equal(levelFromSamples([.001,-.001]),0);
 assert.ok(Math.abs(levelFromSamples([.006,-.006])-.135)<1e-12);
 assert.equal(levelFromSamples([1,-1]),1);assert.ok(levelFromSamples([.006,-.006])<levelFromSamples([.02,-.02]));
});
test('sound smoothing reacts quickly and uses a curved motion boost up to nine times',()=>{
 const attack=smoothLevel(0,1,.1),release=1-smoothLevel(1,0,.1);
 assert.ok(attack>release);assert.equal(smoothLevel(.4,1,-1),.4);assert.equal(smoothLevel(.4,1,NaN),.4);
 assert.equal(smoothLevel(0,1,1),attack);assert.equal(motionScale(0),1);assert.equal(motionScale(0,true),1.25);
 assert.equal(motionScale(1,true),9);assert.equal(motionScale(100,true),9);assert.equal(motionScale(-1),1);
 assert.ok(smoothLevel(0,1,.02)>.65,'movement responds strongly in the first20ms');
 assert.ok(smoothLevel(1,0,.1)<.6,'movement settles quickly after sound stops');
 assert.ok(motionScale(.05,true)>2.9&&motionScale(.05,true)<3,'quiet input produces a visible curved boost');
 assert.equal(motionScale(1),8.75);
});
test('blow gate rejects spikes and fires once per sustained breath',()=>{
 const gate=createBlowGate();
 assert.equal(gate.threshold,.10);assert.equal(gate.armed,true);assert.equal(gate.progress,0);
 assert.equal(gate.update(.9,.05),false);
 assert.ok(gate.progress>0&&gate.progress<1);assert.equal(gate.update(.01,.01),false);assert.equal(gate.progress,0);
 let triggers=0;for(let i=0;i<3;i++)triggers+=Number(gate.update(.9,.05));
 assert.equal(triggers,1);assert.equal(gate.armed,false);
 for(let i=0;i<100;i++)assert.equal(gate.update(1,.1),false);
 assert.equal(gate.armed,false);
});
test('blow gate requires fresh raw sound throughout the hold instead of a smoothed spike tail',()=>{
 const gate=createBlowGate();
 assert.equal(gate.update(1,.05,true,1),false);
 assert.equal(gate.update(.65,.05,true,0),false);assert.equal(gate.progress,0);
 assert.equal(gate.update(.4,.05,true,0),false);
 assert.equal(gate.update(.01,.05,true,1),false);assert.equal(gate.progress,0,'raw strength alone cannot complete the hold');
 assert.equal(gate.update(.135,.03,true,.135),false);
 assert.equal(gate.update(.135,.03,true,.135),true);
 assert.equal(gate.armed,false);
});
test('blow gate needs a quiet release and cooldown before a new breath can fire',()=>{
 const gate=createBlowGate();
 for(let i=0;i<4;i++)gate.update(1,.1);
 for(let i=0;i<6;i++)assert.equal(gate.update(0,.1),false);
 assert.equal(gate.armed,false);
 for(let i=0;i<20;i++)assert.equal(gate.update(.5,.1),false);
 assert.equal(gate.armed,false,'a sustained middle level is not a quiet release');
 for(let i=0;i<10;i++)assert.equal(gate.update(0,.1),false);
 assert.equal(gate.armed,true);
 let triggers=0;for(let i=0;i<3;i++)triggers+=Number(gate.update(.9,.05));assert.equal(triggers,1);
});
test('blow gate bounds inputs, caps elapsed time, and resets when disabled',()=>{
 const capped=createBlowGate({holdSeconds:.3});
 assert.equal(capped.update(1,10),false);assert.ok(Math.abs(capped.progress-.1/.3)<1e-9);
 const progress=capped.progress;assert.equal(capped.update(1,NaN),false);assert.equal(capped.update(1,-1),false);assert.equal(capped.progress,progress);
 const gate=createBlowGate();
 gate.setThreshold(2);assert.equal(gate.threshold,.95);gate.setThreshold(-1);assert.equal(gate.threshold,.05);
 gate.setThreshold(NaN);assert.ok(gate.threshold>=.05&&gate.threshold<=.95);
 gate.reset();assert.equal(gate.progress,0);assert.equal(gate.armed,true);
 for(let i=0;i<4;i++)gate.update(2,.1);assert.equal(gate.armed,false);
 assert.equal(gate.update(1,.1,false),false);assert.equal(gate.progress,0);assert.equal(gate.armed,true);
 let triggers=0;for(let i=0;i<4;i++)triggers+=Number(gate.update(1,.1));assert.equal(triggers,1);
});
test('theme sheet starts hidden, opens from the invisible hotspot, and resets its inactivity timer',()=>{
 const app=atmosphereHarness();assert.equal(app.sheet.hidden,true);assert.equal(app.backdrop.hidden,true);assert.equal(app.pendingTimers,0);
 app.screen.dispatch('click');app.sheet.dispatch('pointerdown');app.advance(HIDE_DELAY);assert.equal(app.sheet.hidden,true);
 app.hotspot.focus();app.hotspot.dispatch('click');assert.equal(app.sheet.hidden,false);assert.equal(app.hotspot.attributes.get('aria-expanded'),'true');assert.equal(app.choices[2],app.choices.find(choice=>choice.attributes.get('aria-pressed')==='true'));
 app.advance(5000);app.sheet.dispatch('pointerdown');app.advance(5999);assert.equal(app.sheet.hidden,false);app.advance(1);assert.equal(app.sheet.hidden,true);assert.equal(app.hotspot.attributes.get('aria-expanded'),'false');
 app.hotspot.dispatch('click');assert.equal(app.backdrop.dispatch('pointerdown'),true);assert.equal(app.sheet.hidden,true);
 app.hotspot.dispatch('click');assert.equal(app.sheet.dispatch('keydown',{key:'Escape'}),true);assert.equal(app.sheet.hidden,true);
});
test('theme choices affect only the current session and ignore legacy stored themes',()=>{
 const app=atmosphereHarness({saved:'dark'});assert.equal(app.controller.theme,'colour');assert.equal(app.screen.dataset.theme,'colour');
 const colourway=app.controller.colourway,writes=app.writes.length;
 for(const [index,theme] of ['dark','light','colour'].entries()){
  app.hotspot.dispatch('click');app.choices[index].dispatch('click');assert.equal(app.controller.theme,theme);assert.equal(app.screen.dataset.theme,theme);
  assert.equal(app.choices[index].attributes.get('aria-pressed'),'true');assert.equal(app.writes.length,writes);assert.equal(app.controller.colourway,colourway);
  app.advance(5999);assert.equal(app.sheet.hidden,false);app.advance(1);assert.equal(app.sheet.hidden,true);
 }
 app.controller.setTheme('dark');app.dispatch('storage',{key:THEME_KEY,newValue:'light'});assert.equal(app.controller.theme,'dark');
 app.dispatch('storage',{key:null,newValue:null});assert.equal(app.controller.theme,'dark');
 app.controller.setTheme('invalid');assert.equal(app.controller.theme,'dark');
 assert.equal(app.reads.includes(THEME_KEY),false);
 const reloaded=atmosphereHarness({storage:app.storage});assert.equal(reloaded.controller.theme,'colour');
 assert.notEqual(reloaded.controller.colourway,colourway);
});
test('every reload selects and saves a different one of the three colourways',()=>{
 const storage=new Map([[THEME_KEY,'light']]);let previous=-1;
 const picked=new Set();
 for(let restart=0;restart<30;restart++){
  const app=atmosphereHarness({storage,random:()=>restart%3/3});
  const current=app.controller.colourway;assert.ok(Number.isInteger(current)&&current>=0&&current<3);
  assert.notEqual(current,previous);assert.equal(app.controller.theme,'colour');assert.equal(app.screen.dataset.colourway,String(current));
  assert.deepEqual(app.writes,[[COLOURWAY_KEY,String(current)]]);assert.equal(storage.get(COLOURWAY_KEY),String(current));
  picked.add(current);previous=current;app.controller.setTheme('dark');assert.equal(app.writes.length,1);
 }
 assert.equal(picked.size,3);
 for(let previous=0;previous<3;previous++)for(const random of [()=>0,()=>.999999]){
  const app=atmosphereHarness({savedColourway:String(previous),random});assert.notEqual(app.controller.colourway,previous);
 }
});
test('corrupt or blocked colourway storage still opens colourfully without crashing',()=>{
 for(const savedColourway of [null,'','bad','-1','3','1.5','null','{"colourway":1}',' 1','01']){
  const app=atmosphereHarness({saved:'dark',savedColourway,random:()=>.999999});
  assert.equal(app.controller.theme,'colour');assert.equal(app.controller.colourway,2);assert.deepEqual(app.writes,[[COLOURWAY_KEY,'2']]);
 }
 const blocked=atmosphereHarness({saved:'light',blockedStorage:true,random:()=>0});
 assert.equal(blocked.controller.theme,'colour');assert.equal(blocked.controller.colourway,0);assert.equal(blocked.writes.length,0);
 assert.doesNotThrow(()=>blocked.controller.setTheme('light'));assert.equal(blocked.controller.theme,'light');
});
test('sound preview accelerates smoothly without microphone permission and shader status is visible only on failure',()=>{
 const app=atmosphereHarness();assert.equal(app.controller.step(.1),1);
 app.preview.value='100';app.preview.dispatch('input');assert.equal(app.blows,0);assert.equal(app.requests.length,0);
 let speed=app.controller.step(.1);assert.ok(speed>8&&speed<8.75);assert.ok(Number(app.meter.attributes.get('aria-valuenow'))>0);
 for(let i=0;i<20;i++)speed=app.controller.step(.1);assert.ok(speed>8.7);assert.match(app.output.textContent,/%$/);
 app.preview.value='0';app.preview.dispatch('input');assert.ok(app.controller.step(.1)<speed);assert.equal(app.controller.micActive,false);
 app.controller.setShaderStatus(false);assert.equal(app.notice.hidden,false);assert.equal(app.screen.dataset.shader,'unavailable');
 app.controller.setShaderStatus(true);assert.equal(app.notice.hidden,true);assert.equal(app.screen.dataset.shader,'ready');
});
test('low linear input curves the movement without triggering navigation and a held breath triggers once',()=>{
 const app=atmosphereHarness();app.preview.value='5';app.preview.dispatch('input');
 for(let i=0;i<30;i++)app.controller.step(.1);
 assert.equal(app.blows,0);assert.ok(app.controller.level>.049&&app.controller.level<.051);
 assert.ok(app.controller.blowForce>.22&&app.controller.blowForce<.23);assert.equal(app.meter.attributes.get('aria-valuenow'),'5');
 assert.ok(Math.abs(app.controller.blowForce-Math.sqrt(app.controller.level))<1e-12);
 assert.equal(app.output.textContent,'5%');assert.ok(app.controller.motionScale>2.7);
 app.preview.value='100';app.preview.dispatch('input');for(let i=0;i<40;i++)app.controller.step(.1);
 assert.equal(app.blows,1);assert.ok(app.controller.blowForce>.99);
 app.preview.value='0';app.preview.dispatch('input');for(let i=0;i<30;i++)app.controller.step(.1);
 app.preview.value='100';app.preview.dispatch('input');for(let i=0;i<12;i++)app.controller.step(.1);
 assert.equal(app.blows,2);assert.equal(app.requests.length,0);
});
test('a rapid zero-to-high preview change cannot bypass the quiet release between breaths',()=>{
 const app=atmosphereHarness();app.preview.value='100';app.preview.dispatch('input');
 for(let i=0;i<12;i++)app.controller.step(.1);assert.equal(app.blows,1);
 app.preview.value='0';app.preview.dispatch('input');app.controller.step(.1);
 assert.ok(app.controller.blowForce>.75,'one quiet frame has not released the smoothed breath');
 app.preview.value='100';app.preview.dispatch('input');for(let i=0;i<40;i++)app.controller.step(.1);
 assert.equal(app.blows,1,'returning the slider to high must not create a new breath');
 app.preview.value='0';app.preview.dispatch('input');for(let i=0;i<30;i++)app.controller.step(.1);
 app.preview.value='100';app.preview.dispatch('input');for(let i=0;i<12;i++)app.controller.step(.1);
 assert.equal(app.blows,2,'a real quiet release and cooldown allow the next breath');
});
test('live microphone breaths rearm after quiet and stopping ignores residual strength',async()=>{
 const incoming=audioStream(),app=atmosphereHarness({getUserMedia:()=>Promise.resolve(incoming)});
 await app.controller.toggleMic();app.sample(.5);
 for(let i=0;i<40;i++)app.controller.step(.1);assert.equal(app.blows,1);
 app.sample(0);for(let i=0;i<30;i++)app.controller.step(.1);
 app.sample(.5);for(let i=0;i<12;i++)app.controller.step(.1);assert.equal(app.blows,2);
 app.controller.stopMic();

 const other=audioStream(),stopped=atmosphereHarness({getUserMedia:()=>Promise.resolve(other)});
 stopped.threshold.value='5';stopped.threshold.dispatch('input');await stopped.controller.toggleMic();stopped.sample(.006);
 stopped.controller.step(.04);
 assert.equal(stopped.blows,0);assert.ok(stopped.controller.blowForce>.15);
 stopped.controller.stopMic();for(let i=0;i<30;i++)stopped.controller.step(.1);
 assert.equal(stopped.blows,0,'residual smoothing after mic stop must not trigger navigation');
 assert.equal(other.track.stops,1);assert.equal(stopped.controller.micActive,false);
});
test('a soft live breath triggers within 150 ms, ignores the noise floor, and waits for a new breath',async()=>{
 const incoming=audioStream(),app=atmosphereHarness({getUserMedia:()=>Promise.resolve(incoming)});
 await app.controller.toggleMic();assert.equal(app.threshold.value,'10');assert.equal(app.marker.style.left,'10%');
 app.sample(.001);for(let i=0;i<50;i++)app.controller.step(.02);
 assert.equal(app.blows,0);assert.equal(app.controller.blowForce,0);
 app.sample(.006);let firedAt=0;
 for(let i=1;i<=10;i++){app.controller.step(.02);if(app.blows&&!firedAt)firedAt=i*.02;}
 assert.equal(app.blows,1);assert.ok(firedAt>=.06&&firedAt<.15,`Gentle breath triggered at ${firedAt}s`);
 for(let i=0;i<100;i++)app.controller.step(.02);assert.equal(app.blows,1,'held room noise or one held breath triggers once');
 app.sample(0);for(let i=0;i<35;i++)app.controller.step(.02);
 app.sample(.006);for(let i=0;i<10;i++)app.controller.step(.02);assert.equal(app.blows,2);
 app.controller.stopMic();
});
test('a 50 ms full-scale live spike cannot trigger while its slower visual response fades',async()=>{
 const incoming=audioStream(),app=atmosphereHarness({getUserMedia:()=>Promise.resolve(incoming)});
 await app.controller.toggleMic();app.sample(1);app.controller.step(.05);assert.equal(app.blows,0);
 app.sample(0);for(let i=0;i<40;i++)app.controller.step(.01);
 assert.equal(app.blows,0);assert.ok(app.controller.blowForce>.1,'visual easing continues without completing a sound hold');
 app.sample(.02);for(let i=0;i<10;i++)app.controller.step(.02);assert.equal(app.blows,1);
 app.controller.stopMic();
});
test('switching from a strong preview to a quiet microphone cannot complete a preview breath',async()=>{
 const incoming=audioStream(),app=atmosphereHarness({getUserMedia:()=>Promise.resolve(incoming)});
 app.threshold.value='35';app.threshold.dispatch('input');app.preview.value='100';app.preview.dispatch('input');
 app.controller.step(.04);
 const visualStrength=app.controller.blowForce;assert.ok(visualStrength>.6);assert.equal(app.blows,0);
 await app.controller.toggleMic();assert.equal(app.controller.blowForce,visualStrength,'source changes preserve the continuous visual response');
 app.sample(0);for(let i=0;i<15;i++)app.controller.step(.1);
 assert.equal(app.blows,0,'a quiet microphone must not inherit the preview’s partial breath or residual detection strength');
 app.sample(.5);for(let i=0;i<12;i++)app.controller.step(.1);assert.equal(app.blows,1);
 app.controller.stopMic();
});
test('blows stay inert while the theme sheet or document is hidden',()=>{
 const app=atmosphereHarness();app.preview.value='100';app.preview.dispatch('input');app.controller.open();
 for(let i=0;i<30;i++)app.controller.step(.1);assert.equal(app.blows,0);
 app.controller.close();app.document.hidden=true;
 for(let i=0;i<30;i++)app.controller.step(.1);assert.equal(app.blows,0);
 app.document.hidden=false;for(let i=0;i<12;i++)app.controller.step(.1);assert.equal(app.blows,1);
});
test('a rejected navigation consumes the breath and threshold controls tune a new breath',()=>{
 const app=atmosphereHarness({onBlowResult:false}),writes=app.writes.length;app.preview.value='100';app.preview.dispatch('input');
 for(let i=0;i<40;i++)app.controller.step(.1);assert.equal(app.blows,1);
 app.preview.value='0';app.preview.dispatch('input');for(let i=0;i<30;i++)app.controller.step(.1);
 app.threshold.value='40';app.threshold.dispatch('input');assert.equal(app.thresholdOutput.textContent,'40%');assert.equal(app.marker.style.left,'40%');
 app.preview.value='50';app.preview.dispatch('input');for(let i=0;i<12;i++)app.controller.step(.1);assert.equal(app.blows,2);
 assert.equal(app.writes.length,writes,'threshold tuning stays in this session');
});
test('microphone denial closes audio resources and leaves the preview available',async()=>{
 const app=atmosphereHarness();app.hotspot.dispatch('click');await app.controller.toggleMic();
 assert.equal(app.controller.micActive,false);assert.match(app.status.textContent,/permission blocked/);assert.equal(app.preview.disabled,false);assert.equal(app.contexts[0].closed,1);
 assert.equal(app.mic.attributes.get('aria-pressed'),'false');assert.equal(app.requests.length,1);assert.equal(app.requests[0].video,false);
 app.advance(HIDE_DELAY);assert.equal(app.sheet.hidden,true);
});
test('live microphone increases motion with volume and stopping releases tracks and audio nodes',async()=>{
 const incoming=audioStream(),app=atmosphereHarness({getUserMedia:()=>Promise.resolve(incoming)});
 await app.controller.toggleMic();assert.equal(app.controller.micActive,true);assert.equal(app.blows,0);assert.equal(app.preview.disabled,true);
 const quiet=app.controller.step(.1);app.sample(.003);const spoken=app.controller.step(.1);assert.ok(spoken>quiet);
 app.sample(.006);const loud=app.controller.step(.1);assert.ok(loud>spoken&&loud<=9);
 app.controller.stopMic();assert.equal(incoming.track.stops,1);assert.equal(app.contexts[0].closed,1);assert.equal(app.contexts[0].sourceDisconnected,true);assert.equal(app.contexts[0].analyserDisconnected,true);assert.equal(app.preview.disabled,false);assert.equal(app.controller.micActive,false);
});
test('cancelled microphone requests stop late-arriving tracks and cannot restart listening',async()=>{
 const pending=deferred(),incoming=audioStream(),app=atmosphereHarness({getUserMedia:()=>pending.promise});
 app.hotspot.dispatch('click');const starting=app.controller.toggleMic();assert.equal(app.mic.textContent,'Cancel mic');assert.equal(app.pendingTimers,0);
 await app.controller.toggleMic();assert.equal(app.contexts[0].closed,1);assert.equal(app.controller.micActive,false);
 pending.resolve(incoming);await starting;assert.equal(incoming.track.stops,1);assert.equal(app.controller.micActive,false);assert.equal(app.blows,0);
});
test('a late cancelled request does not replace or stop a newer microphone session',async()=>{
 const oldRequest=deferred(),oldStream=audioStream(),newStream=audioStream();let calls=0;
 const app=atmosphereHarness({getUserMedia:()=>++calls===1?oldRequest.promise:Promise.resolve(newStream)});
 const oldStarting=app.controller.toggleMic();app.controller.stopMic();await app.controller.toggleMic();assert.equal(app.controller.micActive,true);
 oldRequest.resolve(oldStream);await oldStarting;
 assert.equal(oldStream.track.stops,1);assert.equal(newStream.track.stops,0);assert.equal(app.contexts[1].closed,0);assert.equal(app.controller.micActive,true);assert.equal(app.blows,0);
 app.controller.stopMic();assert.equal(newStream.track.stops,1);
});
test('cancelling while audio unlock is pending releases the acquired stream',async()=>{
 const unlock=deferred(),incoming=audioStream(),app=atmosphereHarness({getUserMedia:()=>Promise.resolve(incoming),resumeAudio:()=>unlock.promise});
 const starting=app.controller.toggleMic();await new Promise(resolve=>setImmediate(resolve));assert.equal(app.mic.textContent,'Cancel mic');
 app.controller.stopMic();assert.equal(incoming.track.stops,1);assert.equal(app.contexts[0].closed,1);
 unlock.resolve();await starting;assert.equal(app.controller.micActive,false);assert.equal(app.blows,0);
});
test('audio unlock failure is handled while microphone permission remains pending',async()=>{
 const permission=deferred(),incoming=audioStream(),app=atmosphereHarness({getUserMedia:()=>permission.promise,resumeAudio:()=>Promise.reject(Error('Audio unlock failed'))});
 const starting=app.controller.toggleMic();await new Promise(resolve=>setImmediate(resolve));
 permission.resolve(incoming);await starting;assert.equal(incoming.track.stops,1);assert.equal(app.contexts[0].closed,1);assert.equal(app.controller.micActive,false);assert.match(app.status.textContent,/unavailable/);
});
test('page exit cancels pending access and a disconnected microphone releases its resources',async()=>{
 const pending=deferred(),incoming=audioStream(),cancelled=atmosphereHarness({getUserMedia:()=>pending.promise});
 cancelled.hotspot.dispatch('click');const starting=cancelled.controller.toggleMic();cancelled.dispatch('pagehide');assert.equal(cancelled.pendingTimers,0);pending.resolve(incoming);await starting;assert.equal(incoming.track.stops,1);assert.equal(cancelled.controller.micActive,false);
 const live=audioStream(),app=atmosphereHarness({getUserMedia:()=>Promise.resolve(live)});await app.controller.toggleMic();live.end();assert.equal(app.controller.micActive,false);assert.equal(live.track.stops,1);assert.match(app.status.textContent,/disconnected/);
});
