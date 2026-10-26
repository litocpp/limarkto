module limarkto;
import :parser;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace limarkto
{
auto MappedText::span(Offset begin, Offset end) const -> SourceSpan {
    if (begin == end) return { offsets[usize(begin)], offsets[usize(begin)] };
    return { offsets[usize(begin)], ends[usize(end - 1)] };
}
auto Parser::tick(Offset amount) -> bool {
    if (error_) return false;
    if (! budget_.consume(amount)) {
        error_ = Some(ParseError::WorkLimit(document_.options_.max_work));
        return false;
    }
    return true;
}
auto Parser::add(NodeKind kind, SourceSpan span, NodeId parent) -> NodeId {
    if (document_.nodes_.len() >= document_.options_.max_nodes) {
        error_ = Some(ParseError::NodeLimit(document_.options_.max_nodes));
        return {};
    }
    auto id = NodeId { document_.nodes_.len() };
    Node node;
    node.kind   = kind;
    node.span   = span;
    node.parent = Some(parent);
    document_.nodes_.push(rstd::move(node));
    get(parent).children.push(NodeId(id));
    return id;
}
auto Parser::trim(Line line) const -> Line {
    while (line.end > line.begin && space(byte(source(), line.end - 1))) --line.end;
    return line;
}
auto Parser::indentation(Line line) const -> unsigned {
    auto column = line.column + line.padding;
    for (auto i = line.begin; i < line.end && space(byte(source(), i)); ++i)
        column += byte(source(), i) == '\t' ? 4 - column % 4 : 1;
    return column - line.column;
}
auto Parser::strip(Line line, unsigned columns) const -> Line {
    auto remaining = columns;
    auto column    = line.column;
    if (line.padding) {
        auto take = remaining < line.padding ? remaining : line.padding;
        line.padding -= take;
        remaining -= take;
        column += take;
    }
    while (remaining && line.begin < line.end && space(byte(source(), line.begin))) {
        auto width = byte(source(), line.begin++) == '\t' ? 4 - column % 4 : 1;
        auto take  = remaining < width ? remaining : width;
        column += take;
        remaining -= take;
        line.padding += width - take;
    }
    line.column = column;
    return line;
}
auto Parser::mapped(slice<Line> lines) -> MappedText {
    MappedText result;
    for (usize i {}; i < lines.len(); ++i) {
        const auto line = lines[i];
        if (! tick(line.end - line.begin + line.padding)) break;
        for (unsigned j {}; j < line.padding; ++j) {
            result.text.push_ascii(' ');
            result.offsets.push(usize(line.begin ? line.begin - 1 : line.begin));
            result.ends.push(usize(line.begin));
        }
        auto text = cut(source(), line.begin, line.end);
        if (text->contains("\0"_str)) {
            auto chars    = text->chars();
            auto position = line.begin;
            for (auto cp = chars.next(); cp.is_some(); cp = chars.next()) {
                auto original_width = cp->len_utf8().to_primitive();
                auto value          = cp->to_primitive() ? *cp : rstd::Char::REPLACEMENT_CHARACTER;
                result.text.push(value);
                for (Offset j {}; j < value.len_utf8().to_primitive(); ++j) {
                    result.offsets.push(usize(position + (original_width == 1 ? 0 : j)));
                    result.ends.push(usize(position + (original_width == 1 ? 1 : j + 1)));
                }
                position += original_width;
            }
        } else {
            result.text.push_str(text);
            for (auto j = line.begin; j < line.end; ++j) {
                result.offsets.push(usize(j));
                result.ends.push(usize(j + 1));
            }
        }
        if (i + usize(1) < lines.len()) {
            result.text.push_ascii('\n');
            result.offsets.push(usize(line.end));
            result.ends.push(usize(line.next));
        }
    }
    result.offsets.push(lines.is_empty() ? usize() : usize(lines[lines.len() - usize(1)].end));
    return result;
}
auto Parser::run() -> Result<Document, ParseError> {
    auto options = document_.options_;
    if (options.max_nodes == usize() || options.max_depth == usize() ||
        options.max_work == usize() || options.max_depth > usize(256))
        return Err(ParseError::InvalidOptions());
    if (document_.source_.len() > options.max_input_bytes)
        return Err(ParseError::InputLimit(options.max_input_bytes, document_.source_.len()));
    auto utf8 = rstd::str_::from_utf8(source().as_bytes());
    if (utf8.is_err()) return Err(ParseError::InvalidInput(rstd::move(utf8).unwrap_err()));
    if (! tick(source().len().to_primitive())) return Err(rstd::move(error_).unwrap());
    Node root;
    root.kind = NodeKind::Document;
    root.span = { usize(), source().len() };
    document_.nodes_.push(rstd::move(root));
    Vec<Line> lines;
    document_.line_starts_.push(usize());
    auto length = source().len().to_primitive();
    for (Offset begin {}; begin < length;) {
        auto end = begin;
        while (end < length && byte(source(), end) != '\n' && byte(source(), end) != '\r') ++end;
        auto next = end;
        if (next < length && byte(source(), next++) == '\r' && byte(source(), next) == '\n') ++next;
        lines.push(Line { begin, end, next });
        if (next > end) document_.line_starts_.push(usize(next));
        begin = next;
    }
    if (! lines.is_empty() && source()->starts_with("\xef\xbb\xbf"_str)) lines[usize()].begin += 3;
    blocks(rstd::move(lines), NodeId {}, usize());
    for (const auto& pending : pending_) {
        if (error_) break;
        delimiters_.clear();
        inlines(
            pending.node, pending.content, 0, pending.content.text.len().to_primitive(), usize());
        if (document_.options_.extensions.autolinks) autolinks(pending.node, pending.content);
        emphasis(pending.node);
    }
    if (error_) return Err(rstd::move(error_).unwrap());
    if (budget_.exhausted) return Err(ParseError::WorkLimit(options.max_work));
    struct Visit {
        NodeId node;
        usize  depth;
    };
    Vec<Visit> visits;
    visits.push(Visit { NodeId {}, usize() });
    while (! visits.is_empty()) {
        auto visit = visits.pop().unwrap();
        if (visit.depth >= options.max_depth) return Err(ParseError::DepthLimit(options.max_depth));
        for (auto child : get(visit.node).children)
            visits.push(Visit { child, visit.depth + usize(1) });
    }
    return Ok(rstd::move(document_));
}
auto Document::parse(String source, ParseOptions options) -> Result<Document, ParseError> {
    return Parser(rstd::move(source), options).run();
}
} // namespace limarkto
