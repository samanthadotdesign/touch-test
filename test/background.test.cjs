const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const {transformSync}=require('esbuild');

// Exercise the shipped adapter. React and the GPU shader are replaced at their
// boundaries so these checks inspect the actual per-frame shader parameters.
function backgroundHarness(colourway){
 let shader;
 const frames=[],controls={colourway,theme:'colour',motionScale:1,blowForce:0,setShaderStatus(){}};
 const react={createElement:(type,props)=>({type,props}),useEffect(){}};
 const dependencies={
  react,
  'react-dom/client':{createRoot:()=>({render:component=>{shader=component.type().props.shader;}})},
  '@figma/expressive-components':{ShaderFill(){},useWebGPUDevice:()=>({error:null})},
  './lib/drifting-colour-fields':{setup(){},manifest:{isAnimated:true},render:(_,frame)=>frames.push(JSON.parse(JSON.stringify(frame.params)))}
 };
 const environment={
  window:{},document:{hidden:false},
  MutationObserver:class{observe(){}},
  require:name=>{assert.ok(name in dependencies,`Unexpected dependency ${name}`);return dependencies[name];}
 };
 const source=fs.readFileSync(path.join(__dirname,'../sandbox/background.tsx'),'utf8');
 vm.runInNewContext(transformSync(source,{loader:'tsx',format:'cjs',target:'es2022'}).code,environment);
 environment.window.PoetryBackground.mount({},controls);
 return{controls,render:()=>{shader.render({}, {time:0,state:{}});return frames.at(-1);}};
}
const gradient=stops=>({stops:stops.map(([position,r,g,b])=>({position,color:{r,g,b,a:1}}))});
const palettes=[
 gradient([[0,.96,.93,.83],[.32,.88,.8,.44],[.65,.64,.43,.57],[1,.65,.78,.87]]),
 gradient([[0,.88,.39,.37],[.5,.72,.61,.81],[1,.98,.9,.68]]),
 gradient([[0,.76,.53,.22],[.5,.61,.4,.53],[1,.96,.93,.84]])
];

test('each colourway rotates only the three active Figma palettes passed to the shader',()=>{
 for(let index=0;index<3;index++){
  const frame=backgroundHarness(index).render();
  assert.deepEqual(frame.palette,palettes[index]);
  assert.deepEqual(frame.palette2,palettes[(index+1)%3]);
  assert.deepEqual(frame.palette3,palettes[(index+2)%3]);
  assert.deepEqual(frame.palette4,gradient([[0,0,0,0],[.32,.52,.56,.3],[.65,.65,.79,.88],[1,.97,.94,.84]]));
  const {palette,palette2,palette3,palette4,...numbers}=frame;
  assert.deepEqual(numbers,{blobCount:7,softness:.72,intensity:1,blobSize:65,colourSpeed:0,seed:12,grainAmount:.16,grainSize:.8,motionSpeed:.18,paletteTransitionSpeed:.6,activePaletteCount:3,blowForce:0});
 }
});
test('session theme changes retain the chosen colourway and leave monochrome palettes unchanged',()=>{
 const baseline=backgroundHarness(0),selected=backgroundHarness(2);
 for(const theme of ['dark','light']){
  baseline.controls.theme=selected.controls.theme=theme;
  assert.deepEqual(selected.render(),baseline.render());
  assert.equal(selected.render().activePaletteCount,1);
 }
 selected.controls.theme='colour';
 assert.deepEqual(selected.render().palette,palettes[2]);
});
