module limarkto:parser;
import :document;
import :syntax;
import liunicode;
using namespace rstd::prelude;
namespace limarkto
{
class Parser {
    Document           document_;
    Option<ParseError> error_;
    ScanBudget         budget_;
    usize              expanded_ {};
    struct PendingInline {
        NodeId     node;
        MappedText content;
    };
    Vec<PendingInline> pending_;
    struct Delimiter {
        NodeId   node;
        unsigned character {};
        Offset   length {};
        bool     opens {}, closes {};
    };
    Vec<Delimiter> delimiters_;
    struct PunctuationProperties {
        liunicode::Property punctuation, symbol;
    };
    Option<PunctuationProperties> punctuation_;
    struct PreparedTable {
        Line                header;
        Line                delimiter;
        Vec<Line>           cells;
        Vec<TableAlignment> alignments;
    };
    Option<PreparedTable> next_table_;
    auto                  source() const -> ref<str> { return document_.source(); }
    auto                  get(NodeId id) -> Node& { return document_.nodes_[id.index]; }
    auto                  tick(Offset amount = 1) -> bool;
    auto                  add(NodeKind kind, SourceSpan span, NodeId parent) -> NodeId;
    auto                  mapped(slice<Line> lines) -> MappedText;
    auto                  trim(Line line) const -> Line;
    auto                  indentation(Line line) const -> unsigned;
    auto                  strip(Line line, unsigned columns) const -> Line;
    auto                  unicode_punctuation(rstd::Char cp) -> bool;
    void                  blocks(Vec<Line> lines, NodeId parent, usize depth);
    void inlines(NodeId parent, const MappedText& content, Offset begin, Offset end, usize depth);
    void emphasis(NodeId parent);
    auto table(slice<Line> lines, usize begin, NodeId parent) -> usize;
    auto prepare_table(slice<Line> lines, usize begin) -> Option<PreparedTable>;
    void autolinks(NodeId parent, const MappedText& content);
    auto reference(const MappedText& content, Offset begin, NodeId parent) -> Offset;

public:
    Parser(String source, ParseOptions options)
        : document_(rstd::move(source), options), budget_ { options.max_work.to_primitive() } {}
    auto run() -> Result<Document, ParseError>;
};
} // namespace limarkto
