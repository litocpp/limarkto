import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { pathToFileURL } from 'node:url';
import path from 'node:path';

const [directory, baseline] = process.argv.slice(2);
const binding = await import(pathToFileURL(path.resolve(directory, 'limarkto.js')));
const bytes = await readFile(path.resolve(directory, 'limarkto.wasm'));
assert.equal(binding.decodeSnapshot, undefined);
assert.equal(binding.parseValue, undefined);
assert.equal(binding.resolveOptions, undefined);
assert.throws(() => binding.parse('x', {}));
await assert.rejects(binding.init(new Uint8Array([0])));
const { memory } = await binding.init(bytes);
assert.equal(binding.schemaVersion(), 1);
assert.equal(binding.packageVersion(), '0.1.0');
let previous;
if (baseline) {
  previous = await import(pathToFileURL(path.resolve(baseline, 'index.js')));
  await previous.init(await readFile(path.resolve(baseline, 'limarkto.wasm')));
}
const sources = ['', '# Title\n\nHello **world**.', '中文😀\r\n\r\n![猫](a.png "title")\n',
  '- [x] task\n- plain\n\n|a|b|\n|-|-|\n|1|2|', '$x$\n\n$$\nx^2\n$$', '[x][id]\n\n[id]: /link "t"',
  '\ud800\n', '\udc00\n', 'a\ud800\ud800b 😀 e\u0301', '\u0000x',
  '`a`\n\n```js\nx\n```\n\n> quote\n\n---\n', '<div>x</div>'];
const optionCases = [{}, { gfm: true, math: true }, { maxNodes: 1 }, { maxDepth: 1 },
  { maxInputBytes: 0 }, { maxOutputBytes: 512 }, { maxWork: 1 }, { maxReferences: 1 },
  { maxExpandedBytes: 1 }, { maxNodes: 0 }, { maxOutputBytes: 0 }, { maxNodes: 65537 },
  { maxOutputBytes: 511 }];
function compare(source, options) {
  const result = binding.parse(source, options);
  if (previous) {
    const before = previous.parse(source, options);
    assert.deepEqual(result, before.ok ? { tag: 'Ok', value: before.document } : { tag: 'Err', value: before.error },
      `${JSON.stringify(source.slice(0, 100))} / ${JSON.stringify(options)}`);
  }
  if (result.tag === 'Ok') {
    const snapshot = result.value;
    assert.equal(snapshot.schemaVersion, 1);
    assert.equal(snapshot.root, 0);
    assert.equal(snapshot.nodes[0].span.end, new TextEncoder().encode(source).length);
    assert.equal(snapshot.nodes[0].range.end, source.length);
    const parents = new Array(snapshot.nodes.length).fill(0);
    for (const node of snapshot.nodes) {
      assert.ok(node.range.begin <= node.range.end && node.range.end <= source.length);
      for (const child of node.children) {
        assert.ok(child > 0 && child < parents.length);
        assert.equal(++parents[child], 1);
      }
    }
    assert.ok(parents.slice(1).every(count => count === 1));
  }
  return result;
}
for (const source of sources) for (const options of optionCases) compare(source, options);
for (const options of [{ gfm: undefined }, { maxInputBytes: undefined }, { math: 'yes' }, { other: true }, null, []])
  assert.throws(() => binding.parse('x', options));
for (const source of [123, null, {}, new String('x')]) assert.throws(() => binding.parse(source, {}));
assert.throws(() => binding.parse('x'));
for (let i = 0; i < 20; ++i) {
  assert.equal(binding.parse('hello', {}).tag, 'Ok');
  assert.equal(binding.parse('hello', { maxOutputBytes: 512 }).value.code, 'OutputLimit');
}
const dense = '# Heading\n\n**bold** and text.\n\n'.repeat(1500);
const result = compare(dense, {});
assert.equal(result.tag, 'Ok');
assert.ok(result.value.nodes.length > 5000);
compare('x'.repeat(1048577), {});
assert.equal(compare('x'.repeat(1048576), {}).tag, 'Ok');
assert.equal(binding.parse('x'.repeat(1048577), { maxNodes: 0 }).value.code, 'InvalidOptions');
for (const source of ['abc', '中文', '😀', '\ud800', 'e\u0301']) {
  const length = new TextEncoder().encode(source).length;
  compare(source, { maxInputBytes: length });
  compare(source, { maxInputBytes: length - 1 });
}
binding.parse('warmup', {});
const before = memory.buffer.byteLength;
for (const source of ['x'.repeat(16 * 1024 * 1024), '中'.repeat(524288), '😀'.repeat(400000)]) {
  assert.equal(binding.parse(source, {}).value.code, 'InputLimit');
  assert.equal(memory.buffer.byteLength, before);
}
assert.throws(() => binding.parse('x'.repeat(16 * 1024 * 1024), { math: 'yes' }));
assert.equal(memory.buffer.byteLength, before);
console.log(`Generated entry: ${sources.length * optionCases.length} comparisons, Unicode ranges, large snapshots and bounded input passed`);
