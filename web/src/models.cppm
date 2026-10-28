export module limarkto.web:models;
export import limarkto;
export import rstd.serde;
using namespace rstd::prelude;
using namespace rstd::literals;

export namespace limarkto
{
constexpr auto rstd_serde_definition(NodeKind*) {
    return rstd::serde::EnumDefinition<
        NodeKind,
        "limarkto::NodeKind",
        rstd::serde::Direction::Serialize,
        rstd::serde::Variant<NodeKind::Document, "document">,
        rstd::serde::Variant<NodeKind::Paragraph, "paragraph">,
        rstd::serde::Variant<NodeKind::Heading, "heading">,
        rstd::serde::Variant<NodeKind::ThematicBreak, "thematicBreak">,
        rstd::serde::Variant<NodeKind::BlockQuote, "blockquote">,
        rstd::serde::Variant<NodeKind::List, "list">,
        rstd::serde::Variant<NodeKind::Item, "item">,
        rstd::serde::Variant<NodeKind::CodeBlock, "codeBlock">,
        rstd::serde::Variant<NodeKind::HtmlBlock, "htmlBlock">,
        rstd::serde::Variant<NodeKind::Text, "text">,
        rstd::serde::Variant<NodeKind::SoftBreak, "softBreak">,
        rstd::serde::Variant<NodeKind::HardBreak, "hardBreak">,
        rstd::serde::Variant<NodeKind::Code, "code">,
        rstd::serde::Variant<NodeKind::Emphasis, "emphasis">,
        rstd::serde::Variant<NodeKind::Strong, "strong">,
        rstd::serde::Variant<NodeKind::Link, "link">,
        rstd::serde::Variant<NodeKind::Image, "image">,
        rstd::serde::Variant<NodeKind::HtmlInline, "htmlInline">,
        rstd::serde::Variant<NodeKind::Strikethrough, "strikethrough">,
        rstd::serde::Variant<NodeKind::Table, "table">,
        rstd::serde::Variant<NodeKind::TableRow, "tableRow">,
        rstd::serde::Variant<NodeKind::TableCell, "tableCell">,
        rstd::serde::Variant<NodeKind::InlineMath, "inlineMath">,
        rstd::serde::Variant<NodeKind::Math, "math">> {};
}
constexpr auto rstd_serde_definition(TableAlignment*) {
    return rstd::serde::EnumDefinition<TableAlignment,
                                       "limarkto::TableAlignment",
                                       rstd::serde::Direction::Serialize,
                                       rstd::serde::Variant<TableAlignment::None, "none">,
                                       rstd::serde::Variant<TableAlignment::Left, "left">,
                                       rstd::serde::Variant<TableAlignment::Center, "center">,
                                       rstd::serde::Variant<TableAlignment::Right, "right">> {};
}
} // namespace limarkto
export namespace limarkto::web
{
enum class [[pmacro::derive("serde::derive")]] ErrorCode
{
    InvalidInput,
    InvalidOptions,
    InputLimit,
    NodeLimit,
    DepthLimit,
    ReferenceLimit,
    ExpansionLimit,
    WorkLimit,
    OutputLimit
};
struct [[pmacro::derive("serde::derive")]] Error {
    ErrorCode code;
    String    message;
};
struct [[pmacro::derive("serde::derive"), pmacro::helper("serde::derive", encode_only)]] WireNode {
    NodeKind       kind;
    u32            begin;
    u32            end;
    Vec<u32>       children;
    String         literal;
    String         destination;
    String         title;
    String         info;
    u32            headingLevel;
    u32            listStart;
    bool           ordered;
    bool           tight;
    Option<bool>   checked;
    TableAlignment alignment;
    bool           tableHeader;
    bool           closed;
};
struct [[pmacro::derive("serde::derive"), pmacro::helper("serde::derive", encode_only)]] Snapshot {
    u32           schemaVersion { 1 };
    u32           root {};
    Vec<WireNode> nodes;
};
struct [[pmacro::derive("serde::derive")]] Options {
    [[pmacro::helper("serde::derive", default)]]
    bool gfm { false };
    [[pmacro::helper("serde::derive", default)]]
    bool math { false };
    [[pmacro::helper("serde::derive", default)]]
    u32 maxInputBytes { 1048576 };
    [[pmacro::helper("serde::derive", default)]]
    u32 maxNodes { 65536 };
    [[pmacro::helper("serde::derive", default)]]
    u32 maxDepth { 64 };
    [[pmacro::helper("serde::derive", default)]]
    u32 maxReferences { 4096 };
    [[pmacro::helper("serde::derive", default)]]
    u32 maxExpandedBytes { 4194304 };
    [[pmacro::helper("serde::derive", default)]]
    u32 maxWork { 33554432 };
    [[pmacro::helper("serde::derive", default)]]
    u32 maxOutputBytes { 16777216 };
};
using Output = Result<Snapshot, Error>;
} // namespace limarkto::web
