import {parse,decodeSnapshot,init,type ParseOptions,type NodeKind,type ErrorCode,type MarkdownNode} from './index.js';
const options:ParseOptions={gfm:true,math:true,maxInputBytes:1000};
const result=parse('hello',options);
if(result.ok){const root:0=result.document.root;const schema:1=result.document.schemaVersion;const node:MarkdownNode=result.document.nodes[0];const kind:NodeKind=node.kind;const units:number=node.range.end;}
else{const code:ErrorCode=result.error.code;}
const valid:NodeKind='inlineMath';
// @ts-expect-error Unknown node kind.
const bad:NodeKind='widget';
// @ts-expect-error Unknown option.
parse('',{invalid:true});
// @ts-expect-error Boolean extension.
parse('',{math:'on'});
// @ts-expect-error Explicit undefined budget is invalid.
parse('',{maxNodes:undefined});
// @ts-expect-error Internal wire positions are not public node fields.
const internal=(node:MarkdownNode)=>node.begin;
decodeSnapshot({},'');
void init(new Uint8Array());
