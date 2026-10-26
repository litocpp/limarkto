export module limarkto:document;
export import :error;
export import rstd.parse;
using namespace rstd::prelude;

export namespace limarkto
{
using SourceSpan = rstd::parse::Span;

struct Extensions {
    bool                  tables {}, strikethrough {}, autolinks {}, tasklists {};
    static constexpr auto gfm() -> Extensions { return { true, true, true, true }; }
};
struct ParseOptions {
    Extensions extensions;
    usize      max_input_bytes { 1024 * 1024 };
    usize      max_nodes { 65536 };
    // Valid range: 1..256, including the document root.
    usize max_depth { 64 };
    usize max_references { 4096 };
    usize max_expanded_bytes { 4 * 1024 * 1024 };
    usize max_work { 32 * 1024 * 1024 };
};
enum class NodeKind
{
    Document,
    Paragraph,
    Heading,
    ThematicBreak,
    BlockQuote,
    List,
    Item,
    CodeBlock,
    HtmlBlock,
    Text,
    SoftBreak,
    HardBreak,
    Code,
    Emphasis,
    Strong,
    Link,
    Image,
    HtmlInline,
    Strikethrough,
    Table,
    TableRow,
    TableCell
};
enum class TableAlignment
{
    None,
    Left,
    Center,
    Right
};
enum class LinkForm
{
    Inline,
    FullReference,
    CollapsedReference,
    ShortcutReference,
    Autolink
};
struct NodeId {
    usize index {};
    auto  operator==(const NodeId&) const -> bool = default;
};
class NodeHandle {
    friend class Document;
    const void* owner_ {};
    NodeId      id_ {};
    NodeHandle(const void* owner, NodeId id): owner_(owner), id_(id) {}

public:
    auto id() const -> NodeId { return id_; }
    auto operator==(const NodeHandle&) const -> bool = default;
};
struct Node {
    NodeKind        kind { NodeKind::Text };
    SourceSpan      span;
    SourceSpan      label_span;
    Vec<SourceSpan> content_spans;
    Option<NodeId>  parent;
    Vec<NodeId>     children;
    String          literal;
    String          destination;
    String          title;
    String          info;
    String          reference_label;
    Option<usize>   definition;
    LinkForm        link_form { LinkForm::Inline };
    unsigned        heading_level {};
    unsigned        list_start { 1 };
    bool            ordered {};
    bool            tight { true };
    Option<bool>    checked;
    TableAlignment  alignment { TableAlignment::None };
    bool            table_header {};
};
struct ReferenceDefinition {
    String     label;
    String     destination;
    String     title;
    SourceSpan span;
    NodeId     container;
};
struct ImageView {
    NodeHandle node;
    SourceSpan span;
    ref<str>   destination;
    ref<str>   title;
};
struct ImageReplacement {
    NodeHandle node;
    String     destination;
};
struct RewriteResult {
    String      source;
    Vec<String> images;
};
class Parser;
class Document {
    friend class Parser;
    String                   source_;
    ParseOptions             options_;
    Box<empty>               identity_;
    Vec<Node>                nodes_;
    Vec<ReferenceDefinition> definitions_;
    Vec<usize>               line_starts_;
    Document(String source, ParseOptions options)
        : source_(rstd::move(source)), options_(options), identity_(Box<empty>::make()) {}

public:
    Document(const Document&)                               = delete;
    Document(Document&&) noexcept                           = default;
    auto        operator=(Document&&) noexcept -> Document& = default;
    static auto parse(String source, ParseOptions options = {}) -> Result<Document, ParseError>;
    auto        source() const [[clang::lifetimebound]] -> ref<str> { return source_.as_str(); }
    auto        root() const -> NodeHandle { return handle(NodeId {}); }
    auto        node_count() const -> usize { return nodes_.len(); }
    auto        handle(NodeId id) const -> NodeHandle {
        return NodeHandle(identity_.as_ptr().as_raw_ptr(), id);
    }
    auto node(NodeHandle handle) const [[clang::lifetimebound]] -> Option<ref<Node>> {
        if (handle.owner_ != identity_.as_ptr().as_raw_ptr() || handle.id_.index >= nodes_.len())
            return None();
        return Some(ref<Node>::from_raw_parts(rstd::addressof(nodes_[handle.id_.index])));
    }
    auto text(SourceSpan span) const [[clang::lifetimebound]] -> Option<ref<str>> {
        if (span.begin > span.end) return None();
        return source_.as_str()->get(span.begin, span.end);
    }
    auto definitions() const [[clang::lifetimebound]] -> slice<ReferenceDefinition> {
        return definitions_.as_slice();
    }
    auto position(usize byte_offset) const -> Option<rstd::parse::SourcePosition>;
    // Nested images in alt content remain in the AST, but are not independent resource uses.
    auto images() const [[clang::lifetimebound]] -> Vec<ImageView>;
    auto rewrite_images(slice<ImageReplacement> replacements,
                        usize                   max_output_bytes = usize(1024 * 1024)) const
        -> Result<RewriteResult, RewriteError>;
};
inline auto parse(String source, ParseOptions options = {}) -> Result<Document, ParseError> {
    return Document::parse(rstd::move(source), options);
}
} // namespace limarkto
