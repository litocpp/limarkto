import * as binding from './limarkto.js';

export const schemaVersion = 1;
export const packageVersion = '0.1.0';
let initializing;
let initializingInput;
let ready = false;

export function init(input) {
  if (initializing) {
    if (input !== initializingInput) return Promise.reject(new Error('limarkto already has a different initialization input'));
    return initializing;
  }
  initializingInput = input;
  initializing = binding.init(input).then(() => {
    if (binding.schemaVersion() !== schemaVersion || binding.packageVersion() !== packageVersion)
      throw new Error('limarkto JavaScript/Wasm version mismatch');
    ready = true;
  }).catch(error => {
    initializing = undefined;
    initializingInput = undefined;
    ready = false;
    throw error;
  });
  return initializing;
}

const integer = (value, min, max) => Number.isSafeInteger(value) && value >= min && value <= max;
const object = value => value !== null && typeof value === 'object' && !Array.isArray(value);
function invalidWire() { throw new Error('Invalid limarkto AST protocol'); }

export function decodeSnapshot(wire, source) {
  if (typeof source !== 'string' || source.length > 1048576) invalidWire();
  if (!object(wire) || typeof wire.ok !== 'boolean') invalidWire();
  if (!wire.ok) {
    let error;
    try { error = binding.codecs.parseValue.result({ tag: 'Err', value: wire.error }).value; } catch { invalidWire(); }
    if (error.message.length > 1024) invalidWire();
    return { ok: false, error: { ...error } };
  }
  if (wire.schemaVersion !== schemaVersion || wire.root !== 0 || !Array.isArray(wire.nodes) || !integer(wire.nodes.length, 1, 65536)) invalidWire();
  try { wire = binding.codecs.parseValue.result({ tag: 'Ok', value: wire }, { maxNodes: 2000000 }).value; } catch { invalidWire(); }
  return projectSnapshot(wire, source);
}

function projectSnapshot(wire, source) {
  if (wire.schemaVersion !== schemaVersion || wire.root !== 0 || !integer(wire.nodes.length, 1, 65536)) invalidWire();
  const length = new TextEncoder().encode(source).length;
  if (length > 1048576) invalidWire();
  const offsets = new Int32Array(length + 1).fill(-1);
  let byte = 0;
  for (let unit = 0; unit < source.length;) {
    offsets[byte] = unit;
    const cp = source.codePointAt(unit);
    byte += cp <= 0x7f ? 1 : cp <= 0x7ff ? 2 : cp <= 0xffff ? 3 : 4;
    unit += cp > 0xffff ? 2 : 1;
  }
  offsets[length] = source.length;
  let edges = 0;
  const parents = new Uint32Array(wire.nodes.length);
  const nodes = wire.nodes.map(node => {
    if (!integer(node.begin, 0, length) ||
      !integer(node.end, node.begin, length) || offsets[node.begin] < 0 || offsets[node.end] < 0 ||
      !Array.isArray(node.children) || node.children.length > wire.nodes.length) invalidWire();
    const children = node.children.map(id => {
      if (!integer(id, 1, wire.nodes.length - 1) || ++parents[id] !== 1 || ++edges >= wire.nodes.length) invalidWire();
      return id;
    });
    if (!integer(node.headingLevel, 0, 6) || (node.kind === 'heading' && node.headingLevel === 0) ||
      !integer(node.listStart, 0, 999999999)) invalidWire();
    const { begin, end, ...fields } = node;
    fields.children = children;
    fields.span = { begin, end };
    fields.range = { begin: offsets[begin], end: offsets[end] };
    return fields;
  });
  if (nodes[0].kind !== 'document' || nodes[0].span.begin !== 0 || nodes[0].span.end !== length || edges !== nodes.length - 1) invalidWire();
  const visited = new Uint8Array(nodes.length);
  const queue = [{ id: 0, depth: 0 }];
  for (let i = 0; i < queue.length; ++i) {
    const { id, depth } = queue[i];
    if (visited[id] || depth >= 256) invalidWire();
    visited[id] = 1;
    for (const child of nodes[id].children) queue.push({ id: child, depth: depth + 1 });
  }
  if (queue.length !== nodes.length) invalidWire();
  return { ok: true, document: { schemaVersion, root: 0, nodes } };
}

export function parse(source, options = {}) {
  if (!ready) throw new Error('limarkto is not initialized');
  if (typeof source !== 'string') throw new TypeError('Markdown source must be a string');
  let values;
  try {
    // Undefined extension switches retain the SDK's omitted-option semantics.
    const normalized = object(options) ? { ...options } : options;
    if (object(normalized)) for (const key of ['gfm', 'math']) if (normalized[key] === undefined) delete normalized[key];
    values = binding.codecs.resolveOptions.arguments([normalized]);
  } catch (error) {
    if (!(error instanceof binding.CodecError)) throw error;
    const budget = error.code === 'range';
    return { ok: false, error: { code: 'InvalidOptions', message: budget ? 'Invalid Markdown parse budget' : 'Invalid Markdown parse options' } };
  }
  const checked = binding.resolveOptions(...values);
  if (checked.tag === 'Err') return { ok: false, error: { ...checked.value } };
  const maxInputBytes = checked.value.maxInputBytes;
  if (source.length > maxInputBytes || new TextEncoder().encode(source).length > maxInputBytes)
    return { ok: false, error: { code: 'InputLimit', message: 'Markdown input exceeds its byte budget' } };
  const result = binding.parseValue(source, values[0]);
  if (result.tag === 'Err') {
    if (result.value.code === 'InputLimit') result.value.message = 'Markdown input exceeds its byte budget';
    return { ok: false, error: { ...result.value } };
  }
  return projectSnapshot(result.value, source);
}
