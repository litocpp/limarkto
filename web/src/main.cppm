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
auto parse_snapshot(wasmto::JsString input, Options config) -> Output {
    auto checked = resolve_options(config);
    if (checked.is_err()) return Err(rstd::move(checked).unwrap_err_unchecked());
    auto source = input.to_string(usize(config.maxInputBytes.to_primitive()));
    if (source.is_none())
        return failure(ErrorCode::InputLimit, "Markdown input exceeds its byte budget"_str);
    ParseOptions options;
    if (config.gfm) options.extensions = Extensions::gfm();
    options.extensions.math    = config.math;
    options.max_input_bytes    = usize(config.maxInputBytes.to_primitive());
    options.max_nodes          = usize(config.maxNodes.to_primitive());
    options.max_depth          = usize(config.maxDepth.to_primitive());
    options.max_references     = usize(config.maxReferences.to_primitive());
    options.max_expanded_bytes = usize(config.maxExpandedBytes.to_primitive());
    options.max_work           = usize(config.maxWork.to_primitive());
    auto parsed                = limarkto::parse(rstd::move(*source), options);
    if (parsed.is_err()) {
        auto message = rstd::format("{}", parsed.unwrap_err());
        return failure(error_code(parsed.unwrap_err()), message.as_str());
    }
    auto document         = rstd::move(parsed).unwrap();
    auto invalid_snapshot = [] {
        return failure(ErrorCode::InvalidSnapshot, "Invalid Markdown AST"_str);
    };
    if (document.node_count() == usize() || document.node_count() > usize(65536))
        return invalid_snapshot();
    const auto text    = document.source();
    auto       offsets = Vec<u32>::with_capacity(text.len() + usize(1));
    for (usize i {}; i <= text.len(); ++i) offsets.push(u32(u32::MAX));
    usize byte_offset {};
    u32   utf16_offset {};
    for (auto character : text.chars()) {
        offsets[byte_offset] = utf16_offset;
        byte_offset += character.len_utf8();
        utf16_offset += character.to_u32() > u32(65535) ? u32(2) : u32(1);
    }
    offsets[text.len()] = utf16_offset;
    Vec<usize> mapping;
    for (usize i {}; i < document.node_count(); ++i) mapping.push(usize(usize::MAX));
    Vec<NodeId> order;
    order.push(document.root().id());
    mapping[usize()] = usize();
    DocumentSnapshot snapshot;
    Vec<usize>       depths;
    depths.push(usize());
    usize remaining = usize(config.maxOutputBytes.to_primitive()) - usize(128);
    for (usize at {}; at < order.len(); ++at) {
        const auto& node = document.node(document.handle(order[at])).unwrap().get();
        if (depths[at] >= usize(256) || node.span.begin > node.span.end ||
            node.span.end > text.len() || offsets[node.span.begin] == u32::MAX ||
            offsets[node.span.end] == u32::MAX || node.heading_level > 6 ||
            (node.kind == NodeKind::Heading && node.heading_level == 0) ||
            node.list_start > 999999999)
            return invalid_snapshot();
        if (at == usize() && (node.kind != NodeKind::Document || node.span.begin != usize() ||
                              node.span.end != text.len()))
            return invalid_snapshot();
        const auto strings =
            node.literal.len() + node.destination.len() + node.title.len() + node.info.len();
        const auto required = usize(512) + strings * usize(6) + node.children.len() * usize(12);
        if (required > remaining)
            return failure(ErrorCode::OutputLimit, "Markdown AST exceeds its output budget"_str);
        remaining -= required;
        MarkdownNode wire { node.kind,
                            { u32(node.span.begin.to_primitive()),
                              u32(node.span.end.to_primitive()) },
                            { offsets[node.span.begin], offsets[node.span.end] },
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
            if (child.index >= mapping.len() || mapping[child.index] != usize::MAX)
                return invalid_snapshot();
            mapping[child.index] = order.len();
            order.push(NodeId(child));
            depths.push(depths[at] + usize(1));
            wire.children.push(u32(mapping[child.index].to_primitive()));
        }
        snapshot.nodes.push(rstd::move(wire));
    }
    return Ok(rstd::move(snapshot));
}
[[pmacro::attr("wasmto::bindgen", serde, export_name("limarkto_parse"), js_name("parse"))]]
auto parse(wasmto::JsString source, Options options) -> wasmto::serde::Batch<Output> {
    return { parse_snapshot(rstd::move(source), options),
             { 16, 3000000 },
             usize(32 * 1024 * 1024) };
}
} // namespace limarkto::web
