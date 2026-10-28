import { parse, init } from './limarkto.js';
type ParseOptions = Parameters<typeof parse>[1];
type ParseResult = ReturnType<typeof parse>;
type DocumentSnapshot = Extract<ParseResult, { tag: 'Ok' }>['value'];
type MarkdownNode = DocumentSnapshot['nodes'][number];
type NodeKind = MarkdownNode['kind'];
const options: ParseOptions = { gfm: true, math: true, maxInputBytes: 1000 };
const result = parse('hello', options);
if (result.tag === 'Ok') {
  const node: MarkdownNode = result.value.nodes[result.value.root];
  const kind: NodeKind = node.kind;
  const units: number = node.range.end;
  const bytes: number = node.span.end;
} else {
  const message: string = result.value.message;
}
const valid: NodeKind = 'inlineMath';
// @ts-expect-error Unknown node kind.
const bad: NodeKind = 'widget';
// @ts-expect-error Unknown option.
parse('', { invalid: true });
// @ts-expect-error Boolean extension.
parse('', { math: 'on' });
// @ts-expect-error Explicit undefined budget is invalid.
parse('', { maxNodes: undefined });
// @ts-expect-error Source is a string.
parse(123, {});
// @ts-expect-error Options are explicit.
parse('');
// @ts-expect-error Byte positions are nested in span.
const internal = (node: MarkdownNode) => node.begin;
void init(new Uint8Array());
