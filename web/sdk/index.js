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

const kinds = new Set(['document', 'paragraph', 'heading', 'thematicBreak', 'blockquote', 'list',
  'item', 'codeBlock', 'htmlBlock', 'text', 'softBreak', 'hardBreak', 'code', 'emphasis', 'strong',
  'link', 'image', 'htmlInline', 'strikethrough', 'table', 'tableRow', 'tableCell', 'inlineMath', 'math']);
const errorCodes = new Set(['InvalidInput', 'InvalidOptions', 'InputLimit', 'NodeLimit', 'DepthLimit',
  'ReferenceLimit', 'ExpansionLimit', 'WorkLimit', 'OutputLimit']);
const integer = (value, min, max) => Number.isSafeInteger(value) && value >= min && value <= max;
const object = value => value !== null && typeof value === 'object' && !Array.isArray(value);
function invalidWire() { throw new Error('Invalid limarkto AST protocol'); }

export function decodeSnapshot(wire, source) {
  if (typeof source !== 'string' || source.length > 1048576) invalidWire();
  if (!object(wire) || typeof wire.ok !== 'boolean') invalidWire();
  if (!wire.ok) {
    if (!object(wire.error) || !errorCodes.has(wire.error.code) || typeof wire.error.message !== 'string' || wire.error.message.length > 1024) invalidWire();
    return { ok: false, error: { code: wire.error.code, message: wire.error.message } };
  }
  if (wire.schemaVersion !== schemaVersion || wire.root !== 0 || !Array.isArray(wire.nodes) || !integer(wire.nodes.length, 1, 65536)) invalidWire();
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
    if (!object(node) || !kinds.has(node.kind) || !integer(node.begin, 0, length) ||
      !integer(node.end, node.begin, length) || offsets[node.begin] < 0 || offsets[node.end] < 0 ||
      !Array.isArray(node.children) || node.children.length > wire.nodes.length) invalidWire();
    const children = node.children.map(id => {
      if (!integer(id, 1, wire.nodes.length - 1) || ++parents[id] !== 1 || ++edges >= wire.nodes.length) invalidWire();
      return id;
    });
    for (const name of ['literal', 'destination', 'title', 'info']) if (typeof node[name] !== 'string') invalidWire();
    for (const name of ['ordered', 'tight', 'tableHeader', 'closed']) if (typeof node[name] !== 'boolean') invalidWire();
    if (!integer(node.headingLevel, 0, 6) || (node.kind === 'heading' && node.headingLevel === 0) ||
      !integer(node.listStart, 0, 999999999) || (node.checked !== null && typeof node.checked !== 'boolean') ||
      !['none', 'left', 'center', 'right'].includes(node.alignment)) invalidWire();
    return { kind: node.kind, span: { begin: node.begin, end: node.end }, range: { begin: offsets[node.begin], end: offsets[node.end] },
      children, literal: node.literal, destination: node.destination, title: node.title, info: node.info,
      headingLevel: node.headingLevel, listStart: node.listStart, ordered: node.ordered, tight: node.tight,
      checked: node.checked, alignment: node.alignment, tableHeader: node.tableHeader, closed: node.closed };
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

const defaults = { maxInputBytes: 1048576, maxNodes: 65536, maxDepth: 64, maxReferences: 4096,
  maxExpandedBytes: 4194304, maxWork: 33554432, maxOutputBytes: 16777216 };
const maxima = { maxInputBytes: 1048576, maxNodes: 65536, maxDepth: 256, maxReferences: 16384,
  maxExpandedBytes: 16777216, maxWork: 67108864, maxOutputBytes: 33554432 };

export function parse(source, options = {}) {
  if (!ready) throw new Error('limarkto is not initialized');
  if (typeof source !== 'string') throw new TypeError('Markdown source must be a string');
  if (!object(options) || Object.keys(options).some(key => !Object.hasOwn(defaults, key) && key !== 'gfm' && key !== 'math') ||
    ['gfm', 'math'].some(key => options[key] !== undefined && typeof options[key] !== 'boolean'))
    return { ok: false, error: { code: 'InvalidOptions', message: 'Invalid Markdown parse options' } };
  const limits = { ...defaults, ...options };
  if (Object.keys(defaults).some(key => !integer(limits[key], key === 'maxInputBytes' ? 0 : key === 'maxOutputBytes' ? 512 : 1, maxima[key])))
    return { ok: false, error: { code: 'InvalidOptions', message: 'Invalid Markdown parse budget' } };
  if (source.length > limits.maxInputBytes || new TextEncoder().encode(source).length > limits.maxInputBytes)
    return { ok: false, error: { code: 'InputLimit', message: 'Markdown input exceeds its byte budget' } };
  const result = binding.parseWire(source, (options.gfm ? 15 : 0) | (options.math ? 16 : 0),
    ...Object.keys(defaults).map(key => limits[key]));
  if (new TextEncoder().encode(result).length > limits.maxOutputBytes) invalidWire();
  return decodeSnapshot(JSON.parse(result), source);
}
