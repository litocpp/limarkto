export module limarkto.web;
import :models;
import wasmto;
import wasmto.serde;
import wasmto.serde.json;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace limarkto::web
{
auto failure(ErrorCode code, ref<str> message) -> Output {
    return Err(Error { code, String::make(message) });
}
auto error_code(const ParseError& error) -> ErrorCode {
    if (error.is_InvalidInput()) return ErrorCode::InvalidInput;
    if (error.is_InvalidOptions()) return ErrorCode::InvalidOptions;
    if (error.is_InputLimit()) return ErrorCode::InputLimit;
    if (error.is_NodeLimit()) return ErrorCode::NodeLimit;
    if (error.is_DepthLimit()) return ErrorCode::DepthLimit;
    if (error.is_ReferenceLimit()) return ErrorCode::ReferenceLimit;
    if (error.is_ExpansionLimit()) return ErrorCode::ExpansionLimit;
    return ErrorCode::WorkLimit;
}
[[pmacro::attr("wasmto::bindgen",
               export_name("limarkto_schema_version"),
               js_name("schemaVersion"))]]
auto schema_version() -> u32 {
    return u32(1);
}
[[pmacro::attr("wasmto::bindgen",
               export_name("limarkto_package_version"),
               js_name("packageVersion"))]]
auto package_version() -> String {
    return String::make("0.1.0"_str);
}

auto resolve_options(Options config) -> Result<Options, Error> {
    if (config.maxInputBytes > u32(1048576) || config.maxNodes < u32(1) ||
        config.maxNodes > u32(65536) || config.maxDepth < u32(1) || config.maxDepth > u32(256) ||
        config.maxReferences < u32(1) || config.maxReferences > u32(16384) ||
        config.maxExpandedBytes < u32(1) || config.maxExpandedBytes > u32(16777216) ||
        config.maxWork < u32(1) || config.maxWork > u32(67108864) ||
        config.maxOutputBytes < u32(512) || config.maxOutputBytes > u32(33554432))
        return Err(
            Error { ErrorCode::InvalidOptions, String::make("Invalid Markdown parse budget"_str) });
    return Ok(config);
}
[[pmacro::attr("wasmto::bindgen",
               serde,
               export_name("limarkto_resolve_options"),
               js_name("resolveOptions"))]]
auto checked_options(Options config) -> wasmto::serde::Batch<Result<Options, Error>> {
    return { resolve_options(config) };
}

auto parse_snapshot(String source, Options config) -> Output {
    auto checked = resolve_options(config);
    if (checked.is_err()) return Err(rstd::move(checked).unwrap_err_unchecked());
    ParseOptions options;
    if (config.gfm) options.extensions = Extensions::gfm();
    options.extensions.math    = config.math;
    options.max_input_bytes    = usize(config.maxInputBytes.to_primitive());
    options.max_nodes          = usize(config.maxNodes.to_primitive());
    options.max_depth          = usize(config.maxDepth.to_primitive());
    options.max_references     = usize(config.maxReferences.to_primitive());
    options.max_expanded_bytes = usize(config.maxExpandedBytes.to_primitive());
    options.max_work           = usize(config.maxWork.to_primitive());
    auto parsed                = limarkto::parse(rstd::move(source), options);
    if (parsed.is_err()) {
        auto message = rstd::format("{}", parsed.unwrap_err());
        return failure(error_code(parsed.unwrap_err()), message.as_str());
    }
    auto       document = rstd::move(parsed).unwrap();
    Vec<usize> mapping;
    for (usize i {}; i < document.node_count(); ++i) mapping.push(usize(usize::MAX));
    Vec<NodeId> order;
    order.push(document.root().id());
    mapping[usize()] = usize();
    Snapshot snapshot;
    usize    remaining = usize(config.maxOutputBytes.to_primitive()) - usize(128);
    for (usize at {}; at < order.len(); ++at) {
        const auto& node = document.node(document.handle(order[at])).unwrap().get();
        const auto  strings =
            node.literal.len() + node.destination.len() + node.title.len() + node.info.len();
        const auto required = usize(512) + strings * usize(6) + node.children.len() * usize(12);
        if (required > remaining)
            return failure(ErrorCode::OutputLimit, "Markdown AST exceeds its output budget"_str);
        remaining -= required;
        WireNode wire { node.kind,
                        u32(node.span.begin.to_primitive()),
                        u32(node.span.end.to_primitive()),
                        {},
                        node.literal.clone(),
                        node.destination.clone(),
                        node.title.clone(),
                        node.info.clone(),
                        u32(node.heading_level),
                        u32(node.list_start),
                        node.ordered,
                        node.tight,
                        node.checked.clone(),
                        node.alignment,
                        node.table_header,
                        node.closed };
        for (auto child : node.children) {
            if (mapping[child.index] == usize::MAX) {
                mapping[child.index] = order.len();
                order.push(NodeId(child));
            }
            wire.children.push(u32(mapping[child.index].to_primitive()));
        }
        snapshot.nodes.push(rstd::move(wire));
    }
    return Ok(rstd::move(snapshot));
}
[[pmacro::attr("wasmto::bindgen",
               serde,
               export_name("limarkto_parse_value"),
               js_name("parseValue"))]]
auto parse_value(String source, Options options) -> wasmto::serde::Batch<Output> {
    return { parse_snapshot(rstd::move(source), options),
             { 16, 3000000 },
             usize(32 * 1024 * 1024) };
}
} // namespace limarkto::web
