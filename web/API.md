# Web API

包入口直接使用 wasmto 生成的 `dist/limarkto.js` 与 `dist/limarkto.d.ts`。没有手写 SDK 或初始化前的快照解码接口。

```ts
import { init, parse } from '@litocpp/limarkto';

await init(wasmUrl);
const result = parse(source, { gfm: true, math: true });
if (result.tag === 'Ok') {
  const document = result.value;
  const root = document.nodes[document.root];
  console.log(root.span, root.range);
} else {
  console.error(result.value.code, result.value.message);
}
```

`init` 接受 URL、Response、字节或 WebAssembly.Module，返回 Wasm exports。调用方持有初始化 Promise，在完成后解析；不依赖 Promise 身份或对不同输入的旧 SDK 限制。`schemaVersion()` 和 `packageVersion()` 在初始化后调用。构建脚本核对版本，配套包通过 bundle 哈希验证，必须整体替换。

`parse(source, options)` 必须传 options，默认配置传 `{}`。字段省略时使用 C++ Options 默认值；显式 undefined、未知字段和错误类型抛转换异常。合法类型的无效预算返回 `Err`，解析/输入/输出预算耗尽也返回 `Err`。投影发现无效的原生树或位置时返回 `InvalidSnapshot`。

源文本参数由 wasmto 的 `JsString` 保留为 JS 句柄，limarkto 验证选项后才调用 `to_string(maxInputBytes)`。UTF-8 字节超限时不编码或复制到 Wasm。字符串进入 ABI 后沿用既有临时缓冲区和 String 所有权转移流程，不承诺零拷贝。

快照拥有节点、字符串和 children 数组。节点 `span` 是 UTF-8 字节范围，`range` 是 UTF-16 码元范围；均为半开区间。UTF-16 范围可直接用于原 JS source 的 slice。emoji 占两个码元，孤立 surrogate 按 TextEncoder 的 U+FFFD 规则编码。C++ Web 投影从 Document 的源码生成位置映射并验证树结构；原生 Document API 不变。

可从公开签名推导消费类型，不必依赖生成器的类型名称编码：

```ts
import type { parse } from '@litocpp/limarkto';
type ParseOptions = Parameters<typeof parse>[1];
type ParseResult = ReturnType<typeof parse>;
type DocumentSnapshot = Extract<ParseResult, { tag: 'Ok' }>['value'];
type MarkdownNode = DocumentSnapshot['nodes'][number];
```

原型 API 迁移：移除 `decodeSnapshot`、`resolveOptions`、`parseValue`、`{ok, document/error}` 信封和 SDK 类型别名。`schemaVersion` / `packageVersion` 从常量改为函数。没有旧接口兼容层。

定向验证：

```sh
node web/build.mjs --out /tmp/limarkto-package --build-dir /tmp/limarkto-build
node web/tests/check.mjs /tmp/limarkto-package/dist
```

check 的第二个路径可指向旧 SDK 的 dist 做输出对照。TypeScript 用例 `web/tests/types.ts` 与生成声明放在同一目录，以 `--strict --exactOptionalPropertyTypes --module nodenext --target es2022 --noEmit` 检查。
