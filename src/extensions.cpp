module limarkto;
import :parser;
using namespace rstd::prelude;
using namespace rstd::literals;
namespace limarkto
{
static auto cells(ref<str> source, Line line) -> Vec<Line> {
    Vec<Line> result;
    while (line.begin < line.end && space(byte(source, line.begin))) ++line.begin;
    while (line.end > line.begin && space(byte(source, line.end - 1))) --line.end;
    if (byte(source, line.begin) == '|') ++line.begin;
    auto begin = line.begin;
    for (auto at = begin; at < line.end; ++at) {
        if (byte(source, at) == '\\' && at + 1 < line.end) {
            ++at;
            continue;
        }
        if (byte(source, at) != '|') continue;
        auto cell = Line { begin, at, at };
        while (cell.begin < cell.end && space(byte(source, cell.begin))) ++cell.begin;
        while (cell.end > cell.begin && space(byte(source, cell.end - 1))) --cell.end;
        result.push(Line(cell));
        begin = at + 1;
    }
    if (begin < line.end || begin == line.begin) {
        auto cell = Line { begin, line.end, line.end };
        while (cell.begin < cell.end && space(byte(source, cell.begin))) ++cell.begin;
        while (cell.end > cell.begin && space(byte(source, cell.end - 1))) --cell.end;
        result.push(Line(cell));
    }
    return result;
}
auto Parser::prepare_table(slice<Line> lines, usize begin) -> Option<PreparedTable> {
    if (begin + usize(1) >= lines.len() || indentation(lines[begin]) >= 4 ||
        indentation(lines[begin + usize(1)]) >= 4)
        return None();
    auto header_line    = lines[begin];
    auto delimiter_line = lines[begin + usize(1)];
    if (! cut(source(), header_line.begin, header_line.end)->contains("|"_str) &&
        ! cut(source(), delimiter_line.begin, delimiter_line.end)->contains("|"_str))
        return None();
    auto headers    = cells(source(), header_line);
    auto delimiters = cells(source(), delimiter_line);
    if (headers.is_empty() || headers.len() != delimiters.len()) return None();
    Vec<TableAlignment> alignments;
    for (auto delimiter : delimiters) {
        auto at   = delimiter.begin;
        bool left = byte(source(), at) == ':';
        if (left) ++at;
        auto first = at;
        while (at < delimiter.end && byte(source(), at) == '-') ++at;
        if (first == at) return None();
        bool right = byte(source(), at) == ':';
        if (right) ++at;
        if (at != delimiter.end) return None();
        alignments.push(left && right ? TableAlignment::Center
                        : left        ? TableAlignment::Left
                        : right       ? TableAlignment::Right
                                      : TableAlignment::None);
    }
    return Some(
        PreparedTable { header_line, delimiter_line, rstd::move(headers), rstd::move(alignments) });
}
auto Parser::table(slice<Line> lines, usize begin, NodeId parent) -> usize {
    Option<PreparedTable> prepared;
    if (next_table_.is_some() && next_table_->header.begin == lines[begin].begin) {
        prepared    = rstd::move(next_table_);
        next_table_ = None();
    } else
        prepared = prepare_table(lines, begin);
    if (prepared.is_none()) return begin;
    const auto& header_line    = prepared->header;
    const auto& delimiter_line = prepared->delimiter;
    const auto& headers        = prepared->cells;
    const auto& alignments     = prepared->alignments;
    auto        node =
        add(NodeKind::Table, { usize(header_line.begin), usize(delimiter_line.end) }, parent);
    auto row = [&](Line line, Vec<Line> values, bool header) {
        auto id = add(NodeKind::TableRow, { usize(line.begin), usize(line.end) }, node);
        get(id).table_header = header;
        for (usize column {}; column < headers.len() && tick(); ++column) {
            auto cell =
                column < values.len() ? values[column] : Line { line.end, line.end, line.end };
            auto child = add(NodeKind::TableCell, { usize(cell.begin), usize(cell.end) }, id);
            get(child).alignment    = alignments[column];
            get(child).table_header = header;
            Vec<Line> input;
            input.push(Line(cell));
            auto       content = mapped(input.as_slice());
            MappedText unescaped;
            for (Offset i {}; i < content.text.len().to_primitive(); ++i) {
                if (byte(content.text.as_str(), i) == '\\' &&
                    byte(content.text.as_str(), i + 1) == '|')
                    ++i;
                auto current = byte(content.text.as_str(), i);
                if (current < 128)
                    unescaped.text.push_ascii(static_cast<char>(current));
                else {
                    auto stop = i + 1;
                    while (stop < content.text.len().to_primitive() &&
                           (byte(content.text.as_str(), stop) & 0xc0) == 0x80)
                        ++stop;
                    unescaped.text.push_str(cut(content.text.as_str(), i, stop));
                    for (auto j = i; j < stop; ++j) {
                        unescaped.offsets.push(usize(content.offsets[usize(j)]));
                        unescaped.ends.push(usize(content.ends[usize(j)]));
                    }
                    i = stop - 1;
                    continue;
                }
                unescaped.offsets.push(usize(content.offsets[usize(i)]));
                unescaped.ends.push(usize(content.ends[usize(i)]));
            }
            unescaped.offsets.push(usize(cell.end));
            get(child).content_spans.push(SourceSpan { usize(cell.begin), usize(cell.end) });
            pending_.push(PendingInline { child, rstd::move(unescaped) });
        }
    };
    Vec<Line> header_cells;
    for (auto value : headers) header_cells.push(Line(value));
    row(header_line, rstd::move(header_cells), true);
    auto end = begin + usize(2);
    while (end < lines.len() && tick()) {
        auto line  = lines[end];
        auto value = cut(source(), line.begin, line.end);
        if (value->trim_ascii().is_empty() || indentation(line) >= 4) break;
        auto trimmed = strip(line, indentation(line));
        auto c       = byte(source(), trimmed.begin);
        if (c == '>' || c == '#' || c == '`' || c == '~' || c == '<') break;
        row(line, cells(source(), line), false);
        get(node).span.end = usize(line.end);
        ++end;
    }
    return end;
}

struct BareLink {
    Offset begin {}, end {};
    String url;
};
static auto local_byte(unsigned c) -> bool {
    return ascii_letter(c) || digit(c) || c == '.' || c == '_' || c == '+' || c == '-';
}
static auto domain_valid(ref<str> host, bool short_host) -> bool {
    if (host.is_empty()) return false;
    unsigned dots {};
    Offset   last {}, previous {};
    for (Offset i {}; i < host.len().to_primitive(); ++i) {
        auto c = byte(host, i);
        if (c == '.') {
            if (i == last) return false;
            ++dots;
            previous = last;
            last     = i + 1;
        } else if (! ascii_letter(c) && ! digit(c) && c != '_' && c != '-' && c < 128)
            return false;
    }
    if (last == host.len().to_primitive() || (! short_host && ! dots)) return false;
    return ! cut(host, previous, host.len().to_primitive())->contains("_"_str);
}
void Parser::autolinks(NodeId parent, const MappedText& content) {
    auto text            = content.text.as_str();
    auto original        = rstd::move(get(parent).children);
    get(parent).children = Vec<NodeId> {};
    auto logical         = [&](usize offset) -> Offset {
        usize low {}, high = content.offsets.len();
        while (low < high) {
            auto middle = low + (high - low) / usize(2);
            if (content.offsets[middle] < offset)
                low = middle + usize(1);
            else
                high = middle;
        }
        return low.to_primitive();
    };
    for (usize i {}; i < original.len() && tick();) {
        if (get(original[i]).kind != NodeKind::Text) {
            get(parent).children.push(NodeId(original[i++]));
            continue;
        }
        auto first = i;
        auto begin = logical(get(original[i]).span.begin);
        auto end   = logical(get(original[i]).span.end);
        ++i;
        while (i < original.len() && get(original[i]).kind == NodeKind::Text &&
               get(original[i]).span.begin == get(original[i - usize(1)]).span.end) {
            end = logical(get(original[i++]).span.end);
        }
        Vec<BareLink> links;
        for (auto at = begin; at < end && tick();) {
            if (byte(text, at) >= 128) {
                ++at;
                while (at < end && (byte(text, at) & 0xc0) == 0x80) ++at;
                continue;
            }
            auto part   = cut(text, at, end);
            auto before = at == begin ? unsigned(' ') : byte(text, at - 1);
            bool scheme = (part->starts_with("http://"_str) || part->starts_with("https://"_str) ||
                           part->starts_with("ftp://"_str)) &&
                          ! ascii_letter(before) && ! digit(before);
            bool www =
                part->starts_with("www."_str) && (whitespace(before) || before == '*' ||
                                                  before == '_' || before == '~' || before == '(');
            if (scheme || www) {
                auto stop = at;
                while (stop < end && ! whitespace(byte(text, stop)) && byte(text, stop) != '<' &&
                       byte(text, stop) != '"' && byte(text, stop) != '\'' && tick())
                    ++stop;
                auto host_begin = at + (www                                 ? 0
                                        : part->starts_with("https://"_str) ? 8
                                        : part->starts_with("http://"_str)  ? 7
                                                                            : 6);
                auto host_end   = host_begin;
                while (host_end < stop && byte(text, host_end) != '/' &&
                       byte(text, host_end) != '?' && byte(text, host_end) != '#')
                    ++host_end;
                while (host_end > host_begin &&
                       (byte(text, host_end - 1) == '.' || byte(text, host_end - 1) == '*' ||
                        byte(text, host_end - 1) == '_' || byte(text, host_end - 1) == '~' ||
                        byte(text, host_end - 1) == ')'))
                    --host_end;
                if (domain_valid(cut(text, host_begin, host_end), scheme)) {
                    unsigned opens {}, closes {};
                    for (auto j = at; j < stop; ++j) {
                        if (byte(text, j) == '(') ++opens;
                        if (byte(text, j) == ')') ++closes;
                    }
                    bool trimming = true;
                    while (stop > at && trimming) {
                        auto c = byte(text, stop - 1);
                        if (c == '.' || c == ',' || c == '!' || c == '?' || c == ':' || c == '*' ||
                            c == '_' || c == '~')
                            --stop;
                        else if (c == ')' && closes > opens) {
                            --stop;
                            --closes;
                        } else if (c == ';') {
                            auto amp = stop - 1;
                            while (amp > at && (ascii_letter(byte(text, amp - 1)) ||
                                                digit(byte(text, amp - 1))))
                                --amp;
                            if (amp > at && byte(text, amp - 1) == '&' && amp < stop - 1)
                                stop = amp - 1;
                            else
                                trimming = false;
                        } else
                            trimming = false;
                    }
                    String url;
                    if (www) url.push_str("http://"_str);
                    url.push_str(decode(cut(text, at, stop)).as_str());
                    links.push(BareLink { at, stop, rstd::move(url) });
                    at = stop;
                    continue;
                }
            }
            auto email_begin = at;
            bool mailto      = part->starts_with("mailto:"_str) && ! local_byte(before);
            bool xmpp        = part->starts_with("xmpp:"_str) && ! local_byte(before);
            if (mailto) email_begin += 7;
            if (xmpp) email_begin += 5;
            if ((mailto || xmpp || ! local_byte(before)) && local_byte(byte(text, email_begin))) {
                auto separator = email_begin;
                while (separator < end && local_byte(byte(text, separator)) && tick()) ++separator;
                if (byte(text, separator) == '@') {
                    auto stop = separator + 1;
                    while (stop < end &&
                           (ascii_letter(byte(text, stop)) || digit(byte(text, stop)) ||
                            byte(text, stop) == '.' || byte(text, stop) == '-' ||
                            byte(text, stop) == '_') &&
                           tick())
                        ++stop;
                    while (stop > separator + 1 && byte(text, stop - 1) == '.') --stop;
                    if (domain_valid(cut(text, separator + 1, stop), false)) {
                        if (xmpp && byte(text, stop) == '/') {
                            while (stop < end && ! whitespace(byte(text, stop)) &&
                                   byte(text, stop) != '<' && tick())
                                ++stop;
                            while (stop > separator + 1 && byte(text, stop - 1) == '.') --stop;
                        }
                        String url;
                        if (! mailto && ! xmpp) url.push_str("mailto:"_str);
                        url.push_str(cut(text, at, stop));
                        links.push(BareLink { at, stop, rstd::move(url) });
                        at = stop;
                        continue;
                    }
                }
            }
            ++at;
        }
        if (links.is_empty()) {
            for (auto j = first; j < i; ++j) get(parent).children.push(NodeId(original[j]));
            continue;
        }
        Vec<bool> kept;
        for (auto j = first; j < i; ++j) kept.push(false);
        auto emit = [&](Offset left, Offset right) {
            for (auto j = first; j < i && tick(); ++j) {
                auto id = original[j];
                auto a = logical(get(id).span.begin), b = logical(get(id).span.end);
                if (a >= right || b <= left) continue;
                if (a >= left && b <= right) {
                    get(parent).children.push(NodeId(id));
                    kept[j - first] = true;
                } else {
                    a                  = a < left ? left : a;
                    b                  = b > right ? right : b;
                    auto child         = add(NodeKind::Text, content.span(a, b), parent);
                    get(child).literal = decode(cut(text, a, b));
                }
            }
        };
        auto copied = begin;
        for (auto& link : links) {
            emit(copied, link.begin);
            auto id             = add(NodeKind::Link, content.span(link.begin, link.end), parent);
            get(id).link_form   = LinkForm::Autolink;
            get(id).destination = rstd::move(link.url);
            auto child          = add(NodeKind::Text, content.span(link.begin, link.end), id);
            get(child).literal  = decode(cut(text, link.begin, link.end));
            copied              = link.end;
        }
        emit(copied, end);
        for (auto j = first; j < i; ++j)
            if (! kept[j - first]) get(original[j]).parent = None();
    }
}
} // namespace limarkto
