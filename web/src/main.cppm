module;
#include <rstd/macro.hpp>
export module limarkto.web;
import limarkto;
import rstd.json;
import wasmto;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace limarkto::web
{
auto kind_name(NodeKind kind) -> ref<str> {
    switch (kind) {
    case NodeKind::Document: return "document"_str;
    case NodeKind::Paragraph: return "paragraph"_str;
    case NodeKind::Heading: return "heading"_str;
    case NodeKind::ThematicBreak: return "thematicBreak"_str;
    case NodeKind::BlockQuote: return "blockquote"_str;
    case NodeKind::List: return "list"_str;
    case NodeKind::Item: return "item"_str;
    case NodeKind::CodeBlock: return "codeBlock"_str;
    case NodeKind::HtmlBlock: return "htmlBlock"_str;
    case NodeKind::Text: return "text"_str;
    case NodeKind::SoftBreak: return "softBreak"_str;
    case NodeKind::HardBreak: return "hardBreak"_str;
    case NodeKind::Code: return "code"_str;
    case NodeKind::Emphasis: return "emphasis"_str;
    case NodeKind::Strong: return "strong"_str;
    case NodeKind::Link: return "link"_str;
    case NodeKind::Image: return "image"_str;
    case NodeKind::HtmlInline: return "htmlInline"_str;
    case NodeKind::Strikethrough: return "strikethrough"_str;
    case NodeKind::Table: return "table"_str;
    case NodeKind::TableRow: return "tableRow"_str;
    case NodeKind::TableCell: return "tableCell"_str;
    case NodeKind::InlineMath: return "inlineMath"_str;
    case NodeKind::Math: return "math"_str;
    }
    rstd::unreachable();
}
auto alignment_name(TableAlignment alignment) -> ref<str> {
    switch (alignment) {
    case TableAlignment::None: return "none"_str;
    case TableAlignment::Left: return "left"_str;
    case TableAlignment::Center: return "center"_str;
    case TableAlignment::Right: return "right"_str;
    }
    rstd::unreachable();
}
struct WireNode {
    const Node* node;
    Vec<usize>  children;
    usize*      remaining;
};
struct Snapshot {
    Vec<WireNode> nodes;
};
auto failure(ref<str> code, ref<str> message) -> String {
    using rstd::json::Value;
    rstd::json::Map error, result;
    error.insert(String::make("code"_str), Value::String(String::make(code)));
    error.insert(String::make("message"_str), Value::String(String::make(message)));
    result.insert(String::make("ok"_str), Value::Bool(false));
    result.insert(String::make("error"_str), Value::Object(rstd::move(error)));
    return rstd::json::to_string(Value::Object(rstd::move(result)));
}
auto error_code(const ParseError& error) -> ref<str> {
    if (error.is_InvalidInput()) return "InvalidInput"_str;
    if (error.is_InvalidOptions()) return "InvalidOptions"_str;
    if (error.is_InputLimit()) return "InputLimit"_str;
    if (error.is_NodeLimit()) return "NodeLimit"_str;
    if (error.is_DepthLimit()) return "DepthLimit"_str;
    if (error.is_ReferenceLimit()) return "ReferenceLimit"_str;
    if (error.is_ExpansionLimit()) return "ExpansionLimit"_str;
    return "WorkLimit"_str;
}
} // namespace limarkto::web

template<>
struct rstd::Impl<rstd::serde::Serialize, limarkto::web::WireNode> {
    template<typename S>
    static auto serialize(S& serializer, const limarkto::web::WireNode& value) ->
        typename S::result_type {
        const auto& node = *value.node;
        auto        strings =
            node.literal.len() + node.destination.len() + node.title.len() + node.info.len();
        auto required = usize(512) + strings * usize(6) + value.children.len() * usize(12);
        if (required > *value.remaining) return Err(serializer.invalid_value("OutputLimit"_str));
        *value.remaining -= required;
        auto output = rstd_try(serializer.begin_map(usize(16)));
        rstd_try(rstd::serde::field(output, "kind"_str, limarkto::web::kind_name(node.kind)));
        rstd_try(rstd::serde::field(output, "begin"_str, node.span.begin));
        rstd_try(rstd::serde::field(output, "end"_str, node.span.end));
        rstd_try(rstd::serde::field(output, "children"_str, value.children));
        rstd_try(rstd::serde::field(output, "literal"_str, node.literal));
        rstd_try(rstd::serde::field(output, "destination"_str, node.destination));
        rstd_try(rstd::serde::field(output, "title"_str, node.title));
        rstd_try(rstd::serde::field(output, "info"_str, node.info));
        rstd_try(rstd::serde::field(output, "headingLevel"_str, u32(node.heading_level)));
        rstd_try(rstd::serde::field(output, "listStart"_str, u32(node.list_start)));
        rstd_try(rstd::serde::field(output, "ordered"_str, node.ordered));
        rstd_try(rstd::serde::field(output, "tight"_str, node.tight));
        rstd_try(rstd::serde::field(output, "checked"_str, node.checked));
        rstd_try(rstd::serde::field(
            output, "alignment"_str, limarkto::web::alignment_name(node.alignment)));
        rstd_try(rstd::serde::field(output, "tableHeader"_str, node.table_header));
        rstd_try(rstd::serde::field(output, "closed"_str, node.closed));
        return output.end();
    }
};
template<>
struct rstd::Impl<rstd::serde::Serialize, limarkto::web::Snapshot> {
    template<typename S>
    static auto serialize(S& serializer, const limarkto::web::Snapshot& value) ->
        typename S::result_type {
        auto output = rstd_try(serializer.begin_map(usize(4)));
        rstd_try(rstd::serde::field(output, "ok"_str, true));
        rstd_try(rstd::serde::field(output, "schemaVersion"_str, u32(1)));
        rstd_try(rstd::serde::field(output, "root"_str, u32(0)));
        rstd_try(rstd::serde::field(output, "nodes"_str, value.nodes));
        return output.end();
    }
};

namespace limarkto::web
{
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

[[pmacro::attr("wasmto::bindgen", export_name("limarkto_parse"), js_name("parseWire"))]]
auto parse_wire(ref<str> source,
                u32      flags,
                u32      input,
                u32      nodes,
                u32      depth,
                u32      references,
                u32      expanded,
                u32      work,
                u32      output) -> String {
    ParseOptions options;
    auto         bits  = flags.to_primitive();
    options.extensions = {
        bool(bits & 1), bool(bits & 2), bool(bits & 4), bool(bits & 8), bool(bits & 16)
    };
    options.max_input_bytes    = usize(input.to_primitive());
    options.max_nodes          = usize(nodes.to_primitive());
    options.max_depth          = usize(depth.to_primitive());
    options.max_references     = usize(references.to_primitive());
    options.max_expanded_bytes = usize(expanded.to_primitive());
    options.max_work           = usize(work.to_primitive());
    if (bits > 31 || output < u32(512) || output > u32(32 * 1024 * 1024))
        return failure("InvalidOptions"_str, "Invalid Markdown output budget or extensions"_str);
    auto parsed = limarkto::parse(String::make(source), options);
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
    usize    remaining = usize(output.to_primitive()) - usize(128);
    for (usize at {}; at < order.len(); ++at) {
        auto     node = document.node(document.handle(order[at])).unwrap();
        WireNode wire { rstd::addressof(node.get()), {}, &remaining };
        for (auto child : node->children) {
            if (mapping[child.index] == usize::MAX) {
                mapping[child.index] = order.len();
                order.push(NodeId(child));
            }
            wire.children.push(usize(mapping[child.index]));
        }
        snapshot.nodes.push(rstd::move(wire));
    }
    auto encoded = rstd::json::encode_direct(snapshot);
    if (encoded.is_err())
        return failure("OutputLimit"_str, "Markdown AST exceeds its output budget"_str);
    return rstd::move(encoded).unwrap();
}
} // namespace limarkto::web
