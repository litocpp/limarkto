export type NodeKind = 'document' | 'paragraph' | 'heading' | 'thematicBreak' | 'blockquote' |
  'list' | 'item' | 'codeBlock' | 'htmlBlock' | 'text' | 'softBreak' | 'hardBreak' | 'code' |
  'emphasis' | 'strong' | 'link' | 'image' | 'htmlInline' | 'strikethrough' | 'table' |
  'tableRow' | 'tableCell' | 'inlineMath' | 'math';
export interface SourceRange { begin: number; end: number }
export interface MarkdownNode {
  kind: NodeKind;
  span: SourceRange;
  range: SourceRange;
  children: number[];
  literal: string;
  destination: string;
  title: string;
  info: string;
  headingLevel: number;
  listStart: number;
  ordered: boolean;
  tight: boolean;
  checked: boolean | null;
  alignment: 'none' | 'left' | 'center' | 'right';
  tableHeader: boolean;
  closed: boolean;
}
export interface DocumentSnapshot { schemaVersion: 1; root: 0; nodes: MarkdownNode[] }
export type ErrorCode = 'InvalidInput' | 'InvalidOptions' | 'InputLimit' | 'NodeLimit' |
  'DepthLimit' | 'ReferenceLimit' | 'ExpansionLimit' | 'WorkLimit' | 'OutputLimit';
export interface ParseError { code: ErrorCode; message: string }
export type ParseResult = { ok: true; document: DocumentSnapshot } | { ok: false; error: ParseError };
export interface ParseOptions {
  gfm?: boolean;
  math?: boolean;
  maxInputBytes?: number;
  maxNodes?: number;
  maxDepth?: number;
  maxReferences?: number;
  maxExpandedBytes?: number;
  maxWork?: number;
  maxOutputBytes?: number;
}
export declare const schemaVersion: 1;
export declare const packageVersion: string;
export declare function init(input: string | URL | Response | BufferSource | WebAssembly.Module): Promise<void>;
export declare function parse(source: string, options?: ParseOptions): ParseResult;
export declare function decodeSnapshot(wire: unknown, source: string): ParseResult;
