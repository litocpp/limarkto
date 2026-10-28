import fs from 'node:fs/promises';
import {pathToFileURL} from 'node:url';
import path from 'node:path';
import {performance} from 'node:perf_hooks';
import {gzipSync} from 'node:zlib';
const dir=path.resolve(process.argv[2]);
let calls=0, memory;
const instantiate=WebAssembly.instantiate;
if (process.argv.includes('--count')) WebAssembly.instantiate=async function(input, imports) {
 const wrapped=Object.fromEntries(Object.entries(imports).map(([name,entries])=>[name,Object.fromEntries(Object.entries(entries).map(([key,fn])=>[key,typeof fn==='function' ? (...args)=>{calls++; return fn(...args);} : fn]))]));
 const result=await instantiate(input,wrapped);memory=(result.instance??result).exports.memory;return result;
};
const sdk=await import(pathToFileURL(path.join(dir,'limarkto.js')));const bytes=await fs.readFile(path.join(dir,'limarkto.wasm'));memory=(await sdk.init(bytes)).memory;
const unit='# Hello 中文😀\n\nA **bold** paragraph with [link](https://example.com).\n\n- one\n- two\n\n';
const cases=[['small',unit.repeat(3),{gfm:true}],['large',unit.repeat(120),{gfm:true}],['dense',unit.repeat(700),{gfm:true}],['error',unit,{maxNodes:1}],['output',unit.repeat(20),{maxOutputBytes:512}]];
const rows=[];
for(const [name,source,options] of cases){
 for(let i=0;i<3;++i)sdk.parse(source,options);
 const times=[];let count=0,mem=0,maxHeap=0,checksum=0;
 for(let i=0;i<9;++i){global.gc?.();calls=0;const start=performance.now();const result=sdk.parse(source,options);times.push(performance.now()-start);count=calls;mem=memory.buffer.byteLength;maxHeap=Math.max(maxHeap,process.memoryUsage().heapUsed);checksum=result.tag==='Ok'?result.value.nodes.length:result.value.code;}
 times.sort((a,b)=>a-b);rows.push({name,bytes:Buffer.byteLength(source),ms:times[4],imports:process.argv.includes('--count')?count:null,linearMemory:mem,heapAfter:maxHeap,checksum});
}
const sizes={};for(const name of ['limarkto.wasm','limarkto.js']){const b=await fs.readFile(path.join(dir,name));sizes[name]={raw:b.length,gzip:gzipSync(b).length};}
console.log(JSON.stringify({node:process.version,rows,sizes,rssPeakKiB:process.resourceUsage().maxRSS},null,2));
