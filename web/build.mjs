import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';
import { parseArgs } from 'node:util';
import { createHash } from 'node:crypto';

const root = fileURLToPath(new URL('.', import.meta.url));
const { values } = parseArgs({ options: {
  out: { type: 'string' }, 'build-dir': { type: 'string' }, 'dependency-root': { type: 'string' },
  profile: { type: 'string', default: 'release' }, jobs: { type: 'string', default: '6' },
  lito: { type: 'string', default: process.env.LITO || 'lito' }, offline: { type: 'boolean', default: false },
} });
if (!values.out) throw new Error('Usage: node web/build.mjs --out <package-directory> [--build-dir <directory>] [--offline]');
const out = path.resolve(values.out);
const build = path.resolve(values['build-dir'] ?? path.join(root, '../build/web'));
const dependencies = path.resolve(values['dependency-root'] ?? path.join(root, '../..'));
if (root === out || root.startsWith(out + path.sep) || out.startsWith(root + path.sep) ||
  build === out || build.startsWith(out + path.sep)) throw new Error('Package output must not contain source or build files');
await fs.mkdir(build, { recursive: true });
const reportPath = path.join(build, 'outputs.json');
const args = ['build', '--no-timing', '-j', values.jobs, '-p', 'limarkto-wasm', '--profile', values.profile,
  '--build-dir', build, '--outputs-json', reportPath, '--use-env-flags',
  '-c', 'toolchain.arch="wasm32"', '-c', 'toolchain.os="unknown"', '-c', 'toolchain.stdlib="libc++"',
  '-c', 'toolchain.wasm.entry="none"', '-c', 'toolchain.wasm.export-memory=true',
  '-c', 'toolchain.wasm.processor="wasmto-bindgen"'];
if (values.offline) args.push('--offline');
const prefixFlags = [[dependencies, '/source'], [path.resolve(root, '..'), '/source/limarkto'], [build, '/build']]
  .map(([from, to]) => `-ffile-prefix-map=${from}/=${to}/`)
  .map(flag => /\s/.test(flag) ? JSON.stringify(flag) : flag).join(' ');
const built = spawnSync(values.lito, args, { cwd: root, stdio: 'inherit',
  env: { ...process.env, CFLAGS: prefixFlags, CXXFLAGS: prefixFlags, LDFLAGS: '' } });
if (built.error) throw built.error;
if (built.status !== 0) process.exit(built.status || 1);
const report = JSON.parse(await fs.readFile(reportPath, 'utf8'));
if (report.format !== 'lito-build-outputs' || report.version !== 1 || report.profile !== values.profile ||
  !report.target.startsWith('wasm32-unknown-unknown')) throw new Error('Invalid limarkto Lito output report');
const artifacts = report.artifacts.filter(item => item.package === 'limarkto-wasm' && item.name === 'limarkto' && item.kind === 'bin');
if (artifacts.length !== 1) throw new Error('Expected one limarkto Wasm artifact');
const roles = { runtime: 'limarkto.wasm', 'javascript-module': 'limarkto.js', 'typescript-declaration': 'limarkto.d.ts' };
const files = Object.entries(roles).map(([role, name]) => {
  const found = artifacts[0].files.filter(file => file.role === role && file.publish);
  if (found.length !== 1) throw new Error(`Expected one ${role} artifact`);
  return { source: found[0].path, destination: `dist/${name}` };
});
await fs.mkdir(path.dirname(out), { recursive: true });
const staging = await fs.mkdtemp(path.join(path.dirname(out), '.limarkto-'));
let backup;
try {
  await fs.mkdir(path.join(staging, 'dist'));
  for (const file of files) await fs.copyFile(file.source, path.join(staging, file.destination));
  const metadata = JSON.parse(await fs.readFile(path.join(root, 'package.json'), 'utf8'));
  delete metadata.scripts;
  metadata.files.push('bundle.json');
  await fs.writeFile(path.join(staging, 'package.json'), JSON.stringify(metadata, null, 2) + '\n');
  await fs.mkdir(path.join(staging, 'licenses'));
  const licenses = [
    ['limarkto', 'LICENSE-MIT'], ['limarkto', 'LICENSE-APACHE'], ['limarkto', 'LICENSE-WHATWG'],
    ['rstd', 'LICENSE-MIT'], ['rstd', 'LICENSE-APACHE'], ['rstd', 'LICENSE-UNICODE'],
    ['rstd', 'src/runtime/compiler-rt/LICENSE.TXT'], ['rstd', 'src/runtime/dlmalloc/LICENSE'],
    ['liunicode', 'LICENSE-MIT'], ['liunicode', 'LICENSE-APACHE'], ['liunicode', 'LICENSE.unicode'],
  ];
  for (const [repository, filename] of licenses) await fs.copyFile(path.join(dependencies, repository, filename),
    path.join(staging, 'licenses', `${repository}-${filename.replaceAll('/', '-')}`));
  await fs.writeFile(path.join(staging, 'NOTICE'),
    'limarkto and wasmto declare MIT OR Apache-2.0.\n' +
    'wasmto supplies the generated bindings and runtime under its declared MIT OR Apache-2.0 license.\n' +
    'rstd, Unicode data, WHATWG entity data, LLVM builtins and dlmalloc notices are included in licenses/.\n');
  const bytes = await fs.readFile(path.join(staging, 'dist/limarkto.wasm'));
  for (const directory of [dependencies, path.resolve(root, '..'), build])
    if (bytes.includes(Buffer.from(directory + path.sep))) throw new Error('Wasm contains an unmapped local source path');
  const module = await WebAssembly.compile(bytes);
  const allowed = new Set(['rstd', 'wasmto']);
  if (WebAssembly.Module.imports(module).some(item => !allowed.has(item.module))) throw new Error('Unexpected Wasm host dependency');
  const binding = await import(pathToFileURL(path.join(staging, 'dist/limarkto.js')));
  await binding.init(bytes);
  if (binding.packageVersion() !== metadata.version || binding.schemaVersion() !== 1)
    throw new Error('Package metadata/Wasm version mismatch');
  const hashes = {};
  for (const name of ['limarkto.js', 'limarkto.d.ts', 'limarkto.wasm'])
    hashes[`dist/${name}`] = createHash('sha256').update(await fs.readFile(path.join(staging, 'dist', name))).digest('hex');
  const revision = spawnSync('git', ['rev-parse', 'HEAD'], { cwd: root, encoding: 'utf8' });
  const dirty = spawnSync('git', ['status', '--porcelain'], { cwd: root, encoding: 'utf8' });
  await fs.writeFile(path.join(staging, 'bundle.json'), JSON.stringify({ format: 'limarkto-web-package', version: 1,
    packageVersion: metadata.version, schemaVersion: 1, profile: values.profile,
    sourceRevision: revision.status === 0 ? revision.stdout.trim() : null,
    sourceModified: dirty.status === 0 ? dirty.stdout.length > 0 : null, files: hashes }, null, 2) + '\n');
  try {
    const old = JSON.parse(await fs.readFile(path.join(out, 'bundle.json'), 'utf8'));
    if (old.format !== 'limarkto-web-package') throw new Error('Refusing to replace an unrelated directory');
    backup = `${staging}-previous`;
    await fs.rename(out, backup);
  } catch (error) { if (error.code !== 'ENOENT') throw error; }
  await fs.rename(staging, out);
  if (backup) await fs.rm(backup, { recursive: true });
} catch (error) {
  if (backup) await fs.rename(backup, out).catch(() => {});
  await fs.rm(staging, { recursive: true, force: true });
  throw error;
}
console.log(`limarkto package: ${out}`);
