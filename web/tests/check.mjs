import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
const [directory, baseline] = process.argv.slice(2);
const sdk = await import(pathToFileURL(path.resolve(directory, 'index.js')));
const bytes = await readFile(path.resolve(directory, 'limarkto.wasm'));
assert.throws(() => sdk.parse('x'), /not initialized/);
assert.deepEqual(sdk.decodeSnapshot({ok:false,error:{code:'InputLimit',message:'test'}}, ''), {ok:false,error:{code:'InputLimit',message:'test'}});
await assert.rejects(sdk.init(new Uint8Array([0])));
assert.throws(() => sdk.parse('x'), /not initialized/);
const first = sdk.init(bytes); assert.equal(sdk.init(bytes), first); await first;
await assert.rejects(sdk.init(bytes.slice()), /different initialization/);
const sources = ['', '# Title\n\nHello **world**.', '中文😀\r\n\r\n![猫](a.png "title")\n', '- [x] task\n- plain\n\n|a|b|\n|-|-|\n|1|2|', '$x$\n\n$$\nx^2\n$$', '[x][id]\n\n[id]: /link "t"', '\ud800\n', '`a`\n\n```js\nx\n```\n\n> quote\n\n---\n', '<div>x</div>'];
const optionCases = [{}, {gfm:true,math:true}, {gfm:undefined}, {maxNodes:1}, {maxDepth:1}, {maxInputBytes:0}, {maxOutputBytes:512}, {maxWork:1}, {maxReferences:1}, {maxExpandedBytes:1}, {maxNodes:0}, {maxOutputBytes:0}, {maxNodes:65537}, {maxOutputBytes:511}, {maxInputBytes:undefined}, {math:'yes'}, {other:true}, null, []];
let previous;
if (baseline) { previous = await import(pathToFileURL(path.resolve(baseline,'index.js'))); await previous.init(await readFile(path.resolve(baseline,'limarkto.wasm'))); }
let count = 0;
for (const source of sources) for (const options of optionCases) {
  const result = sdk.parse(source, options);
  if (previous) assert.deepEqual(result, previous.parse(source, options), `${JSON.stringify(source)} / ${JSON.stringify(options)}`);
  if (result.ok) {
    assert.equal(result.document.nodes[0].span.end, new TextEncoder().encode(source).length);
    assert.equal(result.document.nodes[0].range.end, source.length);
  }
  ++count;
}
assert.throws(() => sdk.parse(123), TypeError);
const source = '中文😀';
const parsed = sdk.parse(source); assert.ok(parsed.ok);
const wire = () => ({ok:true, schemaVersion:1,root:0,nodes:parsed.document.nodes.map(({span,range,...fields})=>({...fields,begin:span.begin,end:span.end}))});
assert.deepEqual(sdk.decodeSnapshot(wire(),source), parsed);
const extra = wire(); extra.nodes[0].extra = true; extra.extra = true;
assert.deepEqual(sdk.decodeSnapshot(extra,source), parsed);
const mutations = [w=>w.schemaVersion=2,w=>w.root=1,w=>w.nodes=[],w=>w.nodes[0].kind='unknown',w=>w.nodes[1].begin=1,w=>w.nodes[0].children=[0],w=>w.nodes[0].children=[1,1],w=>w.nodes[0].children=[],w=>w.nodes[0].literal=3,w=>w.nodes[0].checked=undefined,w=>w.nodes[0].headingLevel=7,w=>w.nodes[0].alignment='up',w=>w.nodes[0].listStart=1e9,w=>w.nodes[0].closed=0,w=>w.nodes[0].children=[999]];
for (const mutate of mutations) {const w=wire();mutate(w);assert.throws(()=>sdk.decodeSnapshot(w,source), /Invalid limarkto AST protocol/);}
for(const code of ['Unknown',123]) assert.throws(()=>sdk.decodeSnapshot({ok:false,error:{code,message:'bad'}},''));
assert.throws(()=>sdk.decodeSnapshot({ok:false,error:{code:'InputLimit',message:'x'.repeat(1025)}},''));
for(let i=0;i<20;++i) { assert.ok(sdk.parse('hello').ok); assert.equal(sdk.parse('hello',{maxOutputBytes:512}).error.code,'OutputLimit'); }
console.log(`SDK: ${count} comparisons and malformed snapshots passed`);

const dense = '# Heading\n\n**bold** and text.\n\n'.repeat(1500);
const denseResult = sdk.parse(dense); assert.ok(denseResult.ok); assert.ok(denseResult.document.nodes.length > 5000);
if (previous) assert.deepEqual(denseResult, previous.parse(dense));
assert.equal(sdk.parse('x'.repeat(1048577)).error.code, 'InputLimit');
assert.equal(sdk.parse('x'.repeat(1048577),{maxNodes:0}).error.code, 'InvalidOptions');
console.log('SDK: large snapshot and input boundary passed');

const binding = await import(pathToFileURL(path.resolve(directory,'limarkto.js')));
const memory = (await binding.init(bytes)).memory;
assert.ok(sdk.parse('warmup').ok);
const before = memory.buffer.byteLength;
assert.equal(sdk.parse('x'.repeat(16 * 1024 * 1024)).error.code,'InputLimit');
assert.equal(memory.buffer.byteLength,before);
