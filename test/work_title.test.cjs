const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const {spawnSync}=require('node:child_process');
const {createHash}=require('node:crypto');
const {loadLibrary,saveLibrary}=require('../sandbox/library-store.js');

const root=path.join(__dirname,'..');
const source=JSON.parse(fs.readFileSync(path.join(root,'data/poetry.json'),'utf8'));
const clone=value=>JSON.parse(JSON.stringify(value));
const python=`
import json, sys
from scripts import generate_poetry as generator
data = generator.validate(json.load(sys.stdin))
outputs = generator.generate(data)
print(json.dumps({
    "data": data,
    "header": outputs[generator.ROOT / "src/poetry_data.h"],
    "browser": outputs[generator.ROOT / "sandbox/poetry-data.js"],
    "pages": {name: outputs[generator.ROOT / name] for name in ("sandbox/index.html", "prototype/index.html")},
}))
`;
function runGenerator(data){
 return spawnSync('python3',['-c',python],{cwd:root,input:JSON.stringify(data),encoding:'utf8'});
}
function generate(data){
 const result=runGenerator(data);assert.equal(result.status,0,result.stderr);
 return JSON.parse(result.stdout);
}
function firmwarePoems(header){
 const quoted='("(?:\\\\.|[^"\\\\])*")';
 const pattern=new RegExp('^  \\{'+[quoted,quoted,quoted,quoted,'(\\d+)'].join(', ')+'\\},$','gm');
 return [...header.matchAll(pattern)].map(match=>({
  id:JSON.parse(match[1]),title:JSON.parse(match[2]),text:JSON.parse(match[3]),by:JSON.parse(match[4]),layout:Number(match[5])
 }));
}
function browserLibrary(browser){
 const environment={};vm.runInNewContext(browser,environment);
 return clone(environment.POETRY_DATA);
}

test('firmware footers use explicit work titles while browser data keeps library labels',()=>{
 const data=clone(source),poem=data.poems[0];
 poem.title='Administrative label';poem.workTitle='A Reader’s Life—“Here”…';
 poem.text='I read “this”—once…';poem.by='Reader’s friend';
 const generated=generate(data),rows=firmwarePoems(generated.header);
 assert.equal(rows.length,data.poems.length);
 assert.equal(rows[0].title,`A Reader's Life-"Here"...`);
 assert.equal(rows[0].text,'I read "this"-once...');assert.equal(rows[0].by,"Reader's friend");
 assert.equal(rows[0].id,poem.id);
 const browser=browserLibrary(generated.browser);
 assert.deepEqual(browser.poems[0],poem);
 assert.equal(browser.poems[0].title,'Administrative label');
 assert.equal(browser.poems[0].workTitle,'A Reader’s Life—“Here”…');
 const approved={
  'time-is-like-a-river':'Meditations',
  'every-act':'The Paper Menagerie and Other Stories',
  'every-act-memory':'The Paper Menagerie and Other Stories',
  'sea-rose':'Sea Rose',
  'and-once-the-storm-is-over':'Kafka on the Shore',
  'every-one-of-us-is-losing':'Kafka on the Shore'
 };
 for(const [id,title]of Object.entries(approved))assert.equal(source.poems.find(p=>p.id===id).workTitle,title);
 assert.equal(source.poems.find(p=>p.id==='i-went-to-the-woods').workTitle,'');
});

test('older libraries without workTitle default to an author-only footer without inferring labels',()=>{
 const legacy=clone(source);legacy.poems.forEach(poem=>delete poem.workTitle);
 const labels=legacy.poems.map(poem=>poem.title),generated=generate(legacy);
 assert.ok(generated.data.poems.every(poem=>poem.workTitle===''));
 assert.ok(firmwarePoems(generated.header).every(poem=>poem.title===''));
 const browser=browserLibrary(generated.browser);
 assert.deepEqual(browser.poems.map(poem=>poem.title),labels);
 assert.ok(browser.poems.every(poem=>poem.workTitle===''));
});

test('work titles validate supported glyphs and length after display punctuation expansion',()=>{
 for(const workTitle of ['A'.repeat(60),'…'.repeat(20),'']){
  const data=clone(source);data.poems[0].workTitle=workTitle;assert.equal(runGenerator(data).status,0);
 }
 for(const workTitle of [null,7,{},'A'.repeat(61),'…'.repeat(21),'A title\ncontinued','An unsupported 🌙','   ']){
  const data=clone(source);data.poems[0].workTitle=workTitle;const result=runGenerator(data);
  assert.notEqual(result.status,0);assert.match(result.stderr,/workTitle/);
 }
});

test('source metadata refresh keeps custom work titles and never infers a title from an old label',()=>{
 const previous=clone(source);previous.poems.forEach(poem=>delete poem.workTitle);
 previous.poems.push({id:'custom-work',title:'My library label',workTitle:'My Actual Work',text:'My own words.',by:'Reader',layout:'centered'});
 previous.poems.push({id:'custom-label-only',title:'Another library label',text:'Another thought.',by:'Reader',layout:'centered'});
 const values=new Map(),storage={getItem:key=>values.get(key)??null,setItem:(key,value)=>values.set(key,value)};
 saveLibrary(previous,'before-work-title',storage);
 const loaded=loadLibrary(clone(source),'with-work-title',storage).library;
 assert.equal(loaded.poems.find(poem=>poem.id==='time-is-like-a-river').workTitle,'Meditations');
 const custom=loaded.poems.find(poem=>poem.id==='custom-work');
 assert.equal(custom.title,'My library label');assert.equal(custom.workTitle,'My Actual Work');
 const rows=firmwarePoems(generate(loaded).header);
 assert.equal(rows.find(poem=>poem.id==='custom-work').title,'My Actual Work');
 assert.equal(rows.find(poem=>poem.id==='custom-label-only').title,'');
});

test('poem revisions version both data script URLs while preserving the rest of each page',()=>{
 const before=new Map(['sandbox/index.html','prototype/index.html'].map(name=>[name,fs.readFileSync(path.join(root,name),'utf8')]));
 const first=generate(clone(source)),updated=clone(source);
 updated.poems[0].workTitle='A new explicit work title';
 const second=generate(updated);
 const revision=generated=>createHash('sha256').update(generated.browser).digest('hex').slice(0,12);
 assert.notEqual(revision(first),revision(second));
 const withoutVersion=html=>html.replace(/(src=["'][a-zA-Z0-9./_-]+\.js)(?:\?[^"']*)?/g,'$1');
 for(const [name,original]of before){
  const asset=name==='sandbox/index.html'?'poetry-data.js':'../sandbox/poetry-data.js';
  assert.ok(first.pages[name].includes(asset+'?rev='+revision(first)));
  assert.ok(second.pages[name].includes(asset+'?rev='+revision(second)));
  assert.equal(withoutVersion(first.pages[name]),withoutVersion(original));
  assert.equal(withoutVersion(second.pages[name]),withoutVersion(original));
  assert.equal(fs.readFileSync(path.join(root,name),'utf8'),original,'collecting custom generated outputs must not write a page');
 }
});

test('script content hashes are idempotent and check mode catches stale data or app URLs',()=>{
 const program=`
import contextlib, hashlib, io, json, re, tempfile
from pathlib import Path
from scripts import generate_poetry as generator
data = json.load(__import__("sys").stdin)
font = (generator.ROOT / "data/font5x7.json").read_text()
pages = {}
external = '<script src="https://example.invalid/external.js?keep=1"></script>'
unlisted = '<script src="optional-entry.js?keep=1"></script>'
for name, assets in generator.PAGE_SCRIPTS.items():
    quote = chr(34) if name.startswith("sandbox/") else chr(39)
    pages[name] = '<main><input id="keep-editor" value="untouched"></main>' + ''.join(
        '<script defer src=' + quote + asset + '?rev=old&other=1' + quote + '></script>' for asset in assets
    ) + external + unlisted + '\\n'
with tempfile.TemporaryDirectory() as directory:
    generator.ROOT = Path(directory)
    for name, assets in generator.PAGE_SCRIPTS.items():
        for asset in assets:
            script = (generator.ROOT / name).parent / asset
            script.parent.mkdir(parents=True, exist_ok=True)
            script.write_text('// test script: ' + asset + '\\n')
    for name, content in {**pages, "data/font5x7.json": font, "data/poetry.json": json.dumps(data)}.items():
        target = generator.ROOT / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content)
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        assert generator.main([]) == 0
        paths = list(generator.generate(generator.validate(data)))
        snapshot = {path: path.read_bytes() for path in paths}
        assert generator.main([]) == 0
        assert snapshot == {path: path.read_bytes() for path in paths}
        assert generator.main(["--check"]) == 0
        for name, original in pages.items():
            actual = (generator.ROOT / name).read_text()
            strip_query = lambda value: re.sub(r'(\\.js)\\?[^\\x22\\x27]*', r'\\1', value)
            assert strip_query(actual) == strip_query(original)
            assert external in actual and unlisted in actual
            for asset in generator.PAGE_SCRIPTS[name]:
                local = ((generator.ROOT / name).parent / asset).resolve()
                expected = hashlib.sha256(local.read_bytes()).hexdigest()[:12]
                assert asset + '?rev=' + expected in actual
        target = generator.ROOT / "sandbox/index.html"
        stale = re.sub(r'\\?rev=[0-9a-f]{12}', '?rev=stale', target.read_text(), count=1)
        target.write_text(stale)
        error = io.StringIO()
        with contextlib.redirect_stderr(error):
            assert generator.main(["--check"]) == 1
        assert "sandbox/index.html" in error.getvalue()
        assert target.read_text() == stale
        assert generator.main([]) == 0
        assert generator.main(["--check"]) == 0
        # An app edit changes only that script's URL, on every page using it.
        before = {name: (generator.ROOT / name).read_text() for name in pages}
        engine = generator.ROOT / "sandbox/engine.js"
        previous_hash = hashlib.sha256(engine.read_bytes()).hexdigest()[:12]
        engine.write_text(engine.read_text() + '// actual code edit\\n')
        next_hash = hashlib.sha256(engine.read_bytes()).hexdigest()[:12]
        assert previous_hash != next_hash
        error = io.StringIO()
        with contextlib.redirect_stderr(error):
            assert generator.main(["--check"]) == 1
        assert "sandbox/index.html" in error.getvalue() and "prototype/index.html" in error.getvalue()
        outputs = generator.generate(generator.validate(data))
        for name, original in before.items():
            expected = original.replace('engine.js?rev=' + previous_hash, 'engine.js?rev=' + next_hash)
            assert outputs[generator.ROOT / name] == expected
            assert (generator.ROOT / name).read_text() == original
        assert generator.main([]) == 0
        assert generator.main(["--check"]) == 0
print("Content-hashed script generation and stale checks passed")
`;
 const result=spawnSync('python3',['-c',program],{cwd:root,input:JSON.stringify(source),encoding:'utf8'});
 assert.equal(result.status,0,result.stderr);
});
