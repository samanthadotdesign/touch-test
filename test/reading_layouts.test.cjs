const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const {spawnSync}=require('node:child_process');
const root=path.join(__dirname,'..');
const source=JSON.parse(fs.readFileSync(path.join(root,'data/poetry.json'),'utf8'));
const manifest=JSON.parse(fs.readFileSync(path.join(root,'data/reading-layouts.json'),'utf8'));
function python(program,input={}){
 const result=spawnSync('python3',['-c','import json, sys\nfrom scripts import generate_poetry as g\nrequest=json.load(sys.stdin)\n'+program],{cwd:root,input:JSON.stringify(input),encoding:'utf8'});
 assert.equal(result.status,0,result.stderr);return JSON.parse(result.stdout);
}
test('the shared Figma manifest generates identical browser boxes and bounded native arrays',()=>{
 const generated=python(`data=g.validate(request)
outputs=g.generate(data)
print(json.dumps({"browser":outputs[g.ROOT/"sandbox/poetry-data.js"],"header":outputs[g.ROOT/"src/reading_layouts.h"],"layouts":g.LAYOUTS}))`,source);
 const environment={};vm.runInNewContext(generated.browser,environment);
 assert.deepEqual(JSON.parse(JSON.stringify(environment.POETRY_READING_LAYOUTS)),manifest.layouts);
 assert.deepEqual(generated.layouts.slice(13),manifest.layouts.map(layout=>layout.key));
 assert.match(generated.header,/PoetryReadingBox boxes\[9\]/);
 assert.match(generated.header,/POETRY_REFERENCE_LAYOUT_COUNT = 4/);assert.match(generated.header,/POETRY_MAX_LAYOUT = 16/);
 for(const layout of manifest.layouts){
  assert.ok(generated.header.includes('{'+layout.id+', '+layout.boxes.length+', {'));
  for(const box of layout.boxes)assert.ok(generated.header.includes('{'+['x','y','width','rows','weight'].map(key=>box[key]).join(', ')+'}'));
 }
});
test('phrase partition preserves actual newlines, weighted global ends and full text coverage',()=>{
 const cases=[
  ['one two three four five six seven eight nine ten eleven twelve',[5,8,12]],
  ['a b c d e f',[2,4,6]],
  ['a\n\nb c d e f',[1,4,6]],
  [Array(15).fill('abcd').join(' '),[4,10,15]],
  ['one two',null],
  [Array(8).fill('a'.repeat(28)).join(' '),null],
  ['\n'.repeat(96)+'a b c d e f',[2,4,6]],
  ['\n'.repeat(97)+'a b c d e f',null],
  ['\n'.repeat(96)+Array(7).fill('abcd').join(' '),null]
 ];
 const result=python(`template=g.reading_layouts()[3]
result=[g.phrase_partition(text,template) for text in request]
# The old body measure would wrap this at28 columns; a reference box uses36.
wide={"boxes":[{"width":216,"rows":1,"weight":1}]}
assert g.phrase_partition(" ".join(["abcd"]*7),wide)==[7]
print(json.dumps(result))`,cases.map(([text])=>text));
 assert.deepEqual(result,cases.map(([,expected])=>expected));
});
test('all reference eligibility checks agree with the partition solver and random retains long verse',()=>{
 const result=python(`out=[]
for template in g.reading_layouts():
    matches=[]
    for poem in request["poems"]:
        text=g.normalize_display(poem["text"])
        ends=g.phrase_partition(text,template)
        try:
            g.layout_size(text,template["key"])
            assert ends is not None
        except ValueError:
            assert ends is None
        if ends is not None:matches.append(poem["id"])
    out.append(matches)
assert g.layout_size("\\n".join(["word"]*22),"random")==1
try:g.layout_size("\\n".join(["word"]*23),"centered");raise AssertionError("legacy overflow accepted")
except ValueError:pass
print(json.dumps(out))`,source);
 for(const matches of result){assert.ok(matches.includes('every-act'));assert.equal(matches.includes('sea-rose'),false);}
});
test('legacy animation speeds validate before normalization and source presets retain varied faster speeds',()=>{
 const result=python(`import copy
data=request
speeds=[]
for value in [.1,.15,.55,.68,.9,2]:
    candidate=copy.deepcopy(data);candidate["animations"][0]["speed"]=value
    speeds.append(g.validate(candidate)["animations"][0]["speed"])
for value in [.099,0,-1,2.001,None,float("nan"),float("inf")]:
    candidate=copy.deepcopy(data);candidate["animations"][0]["speed"]=value
    try:g.validate(candidate);raise AssertionError("invalid speed accepted")
    except ValueError:pass
print(json.dumps(speeds))`,source);
 assert.deepEqual(result,[.68,.68,.68,.68,.9,2]);
 assert.deepEqual(source.animations.map(animation=>animation.speed),[.84,.76,1,.92,.68,.76,.68,.84]);
});
test('motion-only releases change asset content without changing poem-edit provenance',()=>{
 const result=python(`import copy,re
first=g.generate(g.validate(copy.deepcopy(request)))
updated=copy.deepcopy(request);updated["animations"][0]["speed"]=1.23
second=g.generate(g.validate(updated))
key=g.ROOT/"sandbox/poetry-data.js"
revision=lambda outputs:re.search(r'POETRY_SOURCE_REVISION = "([a-f0-9]+)"',outputs[key]).group(1)
assert revision(first)==revision(second)
assert first[key]!=second[key]
assert first[g.ROOT/"sandbox/index.html"]!=second[g.ROOT/"sandbox/index.html"]
print(json.dumps(revision(first)))`,source);
 assert.match(result,/^[0-9a-f]{64}$/);
});
