module limarkto;
import :parser;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace limarkto
{
struct Marker {
    Offset   width {};
    unsigned start { 1 }, delimiter {};
    bool     ordered {};
};
static auto marker(ref<str> text) -> Option<Marker> {
    Marker result;
    auto   c = byte(text, 0);
    Offset at {};
    if (c == '-' || c == '+' || c == '*') {
        result.delimiter = c;
        ++at;
    } else if (digit(c)) {
        unsigned number {}, digits {};
        while (digit(byte(text, at)) && digits < 9) {
            number = number * 10 + byte(text, at++) - '0';
            ++digits;
        }
        if (byte(text, at) != '.' && byte(text, at) != ')') return None();
        result.ordered   = true;
        result.start     = number;
        result.delimiter = byte(text, at++);
    } else
        return None();
    if (at < text.len().to_primitive() && ! space(byte(text, at))) return None();
    result.width = at;
    return Some(result);
}
static auto thematic(ref<str> text) -> bool {
    auto c = byte(text, 0);
    if (c != '*' && c != '-' && c != '_') return false;
    unsigned count {};
    for (auto value : text.as_bytes()) {
        if (value == u8(c))
            ++count;
        else if (! space(value.to_primitive()))
            return false;
    }
    return count >= 3;
}
static auto fence(ref<str> text) -> Offset {
    auto c = byte(text, 0);
    if (c != '`' && c != '~') return 0;
    Offset count {};
    while (byte(text, count) == c) ++count;
    if (count < 3) return 0;
    if (c == '`' && cut(text, count, text.len().to_primitive())->contains("`"_str)) return 0;
    return count;
}
static auto heading(ref<str> text) -> unsigned {
    unsigned level {};
    while (level < 7 && byte(text, level) == '#') ++level;
    return level && level <= 6 && (level == text.len().to_primitive() || space(byte(text, level)))
               ? level
               : 0;
}
static auto setext(ref<str> text) -> unsigned {
    auto c = byte(text, 0);
    if (c != '=' && c != '-') return 0;
    Offset at {};
    while (byte(text, at) == c) ++at;
    while (at < text.len().to_primitive() && space(byte(text, at))) ++at;
    return at == text.len().to_primitive() ? c == '=' ? 1 : 2 : 0;
}
static auto contains_html_end(ref<str> text, ref<str> closing) -> bool {
    for (Offset i {}; i + closing.len().to_primitive() <= text.len().to_primitive(); ++i) {
        bool equal = true;
        for (Offset j {}; j < closing.len().to_primitive(); ++j) {
            auto c = byte(text, i + j);
            if (c >= 'A' && c <= 'Z') c += 32;
            if (c != byte(closing, j)) {
                equal = false;
                break;
            }
        }
        if (equal) return true;
    }
    return false;
}
static auto html_end(ref<str> text, bool paragraph, ScanBudget& budget) -> Option<ref<str>> {
    if (text->starts_with("<!--"_str)) return Some("-->"_str);
    if (text->starts_with("<?"_str)) return Some("?>"_str);
    if (text->starts_with("<![CDATA["_str)) return Some("]]>"_str);
    if (text->starts_with("<!"_str) && byte(text, 2) >= 'A' && byte(text, 2) <= 'Z')
        return Some(">"_str);
    if (byte(text, 0) != '<') return None();
    Offset at    = byte(text, 1) == '/' ? 2 : 1;
    auto   begin = at;
    while (ascii_letter(byte(text, at)) || digit(byte(text, at))) ++at;
    if (begin == at || (at < text.len().to_primitive() && ! whitespace(byte(text, at)) &&
                        byte(text, at) != '>' && byte(text, at) != '/'))
        return None();
    String name;
    for (auto c : cut(text, begin, at).as_bytes()) {
        auto value = c.to_primitive();
        name.push_ascii(static_cast<char>(value >= 'A' && value <= 'Z' ? value + 32 : value));
    }
    if (begin == 1) {
        if (name.as_str() == "script"_str) return Some("</script>"_str);
        if (name.as_str() == "style"_str) return Some("</style>"_str);
        if (name.as_str() == "pre"_str) return Some("</pre>"_str);
        if (name.as_str() == "textarea"_str) return Some("</textarea>"_str);
    }
    for (auto tag : array<ref<str>, 62> {
             "address"_str,    "article"_str,  "aside"_str,   "base"_str,     "basefont"_str,
             "blockquote"_str, "body"_str,     "caption"_str, "center"_str,   "col"_str,
             "colgroup"_str,   "dd"_str,       "details"_str, "dialog"_str,   "dir"_str,
             "div"_str,        "dl"_str,       "dt"_str,      "fieldset"_str, "figcaption"_str,
             "figure"_str,     "footer"_str,   "form"_str,    "frame"_str,    "frameset"_str,
             "h1"_str,         "h2"_str,       "h3"_str,      "h4"_str,       "h5"_str,
             "h6"_str,         "head"_str,     "header"_str,  "hr"_str,       "html"_str,
             "iframe"_str,     "legend"_str,   "li"_str,      "link"_str,     "main"_str,
             "menu"_str,       "menuitem"_str, "nav"_str,     "noframes"_str, "ol"_str,
             "optgroup"_str,   "option"_str,   "p"_str,       "param"_str,    "search"_str,
             "section"_str,    "source"_str,   "summary"_str, "table"_str,    "tbody"_str,
             "td"_str,         "tfoot"_str,    "th"_str,      "thead"_str,    "title"_str,
             "tr"_str,         "ul"_str })
        if (name.as_str() == tag) return Some(""_str);
    if (! paragraph) {
        auto end = html_tag_end(text, 0, text.len().to_primitive(), budget);
        if (end && cut(text, end, text.len().to_primitive())->trim_ascii().is_empty())
            return Some(""_str);
    }
    return None();
}
auto Parser::reference(const MappedText& content, Offset begin, NodeId parent) -> Offset {
    auto text = content.text.as_str();
    if (byte(text, begin) != '[') return begin;
    auto     at = begin + 1;
    unsigned characters {};
    for (; at < text.len().to_primitive() && byte(text, at) != ']'; ++at) {
        if (! tick()) return begin;
        if (byte(text, at) == '[') return begin;
        if (byte(text, at) == '\\' && punctuation(byte(text, at + 1))) ++at;
        if ((byte(text, at) & 0xc0) != 0x80 && ++characters > 999) return begin;
    }
    if (byte(text, at) != ']' || byte(text, at + 1) != ':') return begin;
    auto label = normalize_label(cut(text, begin + 1, at));
    if (label.is_empty()) return begin;
    auto target = destination(text, at + 2, false, budget_);
    if (target.is_none()) return begin;
    if (! tick(target->end - at)) return begin;
    if (document_.definitions_.len() >= document_.options_.max_references) {
        error_ = Some(ParseError::ReferenceLimit(document_.options_.max_references));
        return begin;
    }
    // Keep definitions in source order; the first matching label wins.
    document_.definitions_.push(ReferenceDefinition { rstd::move(label),
                                                      rstd::move(target->url),
                                                      rstd::move(target->title),
                                                      content.span(begin, target->end),
                                                      parent });
    return target->end;
}
void Parser::blocks(Vec<Line> lines, NodeId parent, usize depth) {
    if (depth >= document_.options_.max_depth) {
        error_ = Some(ParseError::DepthLimit(document_.options_.max_depth));
        return;
    }
    for (usize i {}; i < lines.len() && tick();) {
        auto line   = lines[i];
        auto indent = indentation(line);
        auto start  = strip(line, indent);
        auto text   = cut(source(), start.begin, start.end);
        if (text->trim_ascii().is_empty() && ! start.padding) {
            ++i;
            continue;
        }
        auto span = SourceSpan { usize(line.begin), usize(line.end) };
        if (document_.options_.extensions.tables) {
            auto end = table(lines.as_slice(), i, parent);
            if (end > i) {
                i = end;
                continue;
            }
        }
        auto opening = indent < 4 ? fence(text) : 0;
        if (opening) {
            auto node           = add(NodeKind::CodeBlock, span, parent);
            auto info           = cut(text, opening, text.len().to_primitive())->trim_ascii();
            get(node).info      = decode(info);
            auto      delimiter = byte(text, 0);
            Vec<Line> body;
            ++i;
            while (i < lines.len() && tick()) {
                auto   next  = strip(lines[i], indentation(lines[i]));
                auto   value = cut(source(), next.begin, next.end);
                Offset count {};
                while (byte(value, count) == delimiter) ++count;
                if (indentation(lines[i]) < 4 && count >= opening &&
                    cut(value, count, value.len().to_primitive())->trim_ascii().is_empty()) {
                    get(node).span.end = usize(lines[i++].end);
                    break;
                }
                auto content = strip(lines[i], indent);
                body.push(Line(content));
                get(node).span.end = usize(lines[i++].end);
            }
            auto content      = mapped(body.as_slice());
            get(node).literal = literal_text(content.text.as_str());
            if (! body.is_empty()) get(node).literal.push_ascii('\n');
            for (auto value : body)
                get(node).content_spans.push(SourceSpan { usize(value.begin), usize(value.end) });
            continue;
        }
        if (indent >= 4) {
            auto      node = add(NodeKind::CodeBlock, span, parent);
            Vec<Line> body;
            while (i < lines.len() && tick()) {
                auto next  = lines[i];
                auto width = indentation(next);
                if (width < 4 && ! cut(source(), next.begin, next.end)->trim_ascii().is_empty())
                    break;
                body.push(strip(next, 4));
                ++i;
            }
            while (! body.is_empty() &&
                   cut(source(), body[body.len() - usize(1)].begin, body[body.len() - usize(1)].end)
                       ->trim_ascii()
                       .is_empty())
                (void)body.pop();
            auto content      = mapped(body.as_slice());
            get(node).literal = literal_text(content.text.as_str());
            if (! body.is_empty()) {
                get(node).literal.push_ascii('\n');
                get(node).span.end = usize(body[body.len() - usize(1)].end);
            }
            continue;
        }
        if (auto end = html_end(text, false, budget_); end.is_some()) {
            auto      node = add(NodeKind::HtmlBlock, span, parent);
            Vec<Line> body;
            while (i < lines.len() && tick()) {
                auto next  = lines[i];
                auto value = cut(source(), next.begin, next.end);
                if (end->is_empty() && value->trim_ascii().is_empty()) break;
                body.push(Line(next));
                get(node).span.end = usize(next.end);
                ++i;
                if (! end->is_empty() && contains_html_end(value, *end)) break;
            }
            get(node).literal = literal_text(mapped(body.as_slice()).text.as_str());
            if (! body.is_empty()) get(node).literal.push_ascii('\n');
            continue;
        }
        if (byte(text, 0) == '>') {
            auto      node = add(NodeKind::BlockQuote, span, parent);
            Vec<Line> body;
            bool      lazy {};
            Offset    quote_fence {};
            unsigned  quote_delimiter {};
            while (i < lines.len() && tick()) {
                auto next    = lines[i];
                auto content = strip(next, indentation(next));
                if (indentation(next) < 4 && byte(source(), content.begin) == '>') {
                    ++content.begin;
                    ++content.column;
                    if (content.begin < content.end && space(byte(source(), content.begin)))
                        content = strip(content, 1);
                    auto value          = cut(source(), content.begin, content.end);
                    auto content_indent = indentation(content);
                    auto stripped       = strip(content, content_indent);
                    auto inline_text    = cut(source(), stripped.begin, stripped.end);
                    if (quote_fence) {
                        Offset count {};
                        while (byte(inline_text, count) == quote_delimiter) ++count;
                        if (content_indent < 4 && count >= quote_fence &&
                            cut(inline_text, count, inline_text.len().to_primitive())
                                ->trim_ascii()
                                .is_empty())
                            quote_fence = 0;
                        lazy = false;
                    } else {
                        quote_fence     = content_indent < 4 ? fence(inline_text) : 0;
                        quote_delimiter = byte(inline_text, 0);
                        lazy = ! value->trim_ascii().is_empty() && content_indent < 4 &&
                               ! quote_fence && ! heading(inline_text) && ! thematic(inline_text) &&
                               ! html_end(inline_text, false, budget_);
                    }
                    body.push(Line(content));
                } else if (lazy && ! cut(source(), next.begin, next.end)->trim_ascii().is_empty() &&
                           (indentation(next) >= 4 ||
                            (! heading(cut(source(), content.begin, content.end)) &&
                             ! fence(cut(source(), content.begin, content.end)) &&
                             ! thematic(cut(source(), content.begin, content.end)) &&
                             ! marker(cut(source(), content.begin, content.end)) &&
                             ! html_end(
                                 cut(source(), content.begin, content.end), true, budget_)))) {
                    next.lazy = true;
                    body.push(Line(next));
                } else
                    break;
                get(node).span.end = usize(next.end);
                ++i;
            }
            blocks(rstd::move(body), node, depth + usize(1));
            continue;
        }
        if (thematic(text)) {
            add(NodeKind::ThematicBreak, span, parent);
            ++i;
            continue;
        }
        if (auto level = heading(text)) {
            auto node               = add(NodeKind::Heading, span, parent);
            get(node).heading_level = level;
            auto content            = start;
            content.begin += level;
            while (content.begin < content.end && space(byte(source(), content.begin)))
                ++content.begin;
            content  = trim(content);
            auto end = content.end;
            while (end > content.begin && byte(source(), end - 1) == '#') --end;
            if (end < content.end && (end == content.begin || space(byte(source(), end - 1)))) {
                content.end = end;
                content     = trim(content);
            }
            Vec<Line> body;
            body.push(Line(content));
            get(node).content_spans.push(SourceSpan { usize(content.begin), usize(content.end) });
            pending_.push(PendingInline { node, mapped(body.as_slice()) });
            ++i;
            continue;
        }
        if (auto first = marker(text); first.is_some()) {
            auto list            = add(NodeKind::List, span, parent);
            get(list).ordered    = first->ordered;
            get(list).list_start = first->start;
            bool separated_items {};
            while (i < lines.len() && tick()) {
                auto item_line   = strip(lines[i], indentation(lines[i]));
                auto item_marker = marker(cut(source(), item_line.begin, item_line.end));
                if (indentation(lines[i]) >= 4 ||
                    thematic(cut(source(), item_line.begin, item_line.end)) ||
                    item_marker.is_none() || item_marker->ordered != first->ordered ||
                    item_marker->delimiter != first->delimiter)
                    break;
                if (separated_items) get(list).tight = false;
                auto item =
                    add(NodeKind::Item, { usize(lines[i].begin), usize(lines[i].end) }, list);
                auto marker_indent = indentation(lines[i]);
                item_line.begin += item_marker->width;
                item_line.column += static_cast<unsigned>(item_marker->width);
                auto padding = indentation(item_line);
                padding      = padding >= 1 && padding <= 4 ? padding : 1;
                if (cut(source(), item_line.begin, item_line.end)->trim_ascii().is_empty())
                    padding = 1;
                auto width   = marker_indent + static_cast<unsigned>(item_marker->width) + padding;
                auto content = strip(item_line, padding);
                if (document_.options_.extensions.tasklists &&
                    byte(source(), content.begin) == '[' &&
                    byte(source(), content.begin + 2) == ']' &&
                    (byte(source(), content.begin + 1) == ' ' ||
                     byte(source(), content.begin + 1) == 'x' ||
                     byte(source(), content.begin + 1) == 'X') &&
                    (content.begin + 3 == content.end ||
                     whitespace(byte(source(), content.begin + 3)))) {
                    get(item).checked = Some(byte(source(), content.begin + 1) != ' ');
                    content.begin += 3;
                    while (content.begin < content.end && space(byte(source(), content.begin)))
                        ++content.begin;
                }
                Vec<Line> body;
                body.push(Line(content));
                ++i;
                bool initial_empty =
                    cut(source(), content.begin, content.end)->trim_ascii().is_empty();
                bool blank = initial_empty;
                bool physical_blank {};
                while (i < lines.len() && tick()) {
                    auto next        = lines[i];
                    auto next_indent = indentation(next);
                    auto next_text   = cut(source(), strip(next, next_indent).begin, next.end);
                    if (next_text->trim_ascii().is_empty()) {
                        body.push(Line(next));
                        blank          = true;
                        physical_blank = true;
                        if (initial_empty && body.len() == usize(2)) {
                            ++i;
                            break;
                        }
                    } else if (next_indent >= width) {
                        body.push(strip(next, width));
                        blank = false;
                    } else if (! blank &&
                               (next_indent >= 4 || (! marker(next_text) && ! heading(next_text) &&
                                                     ! fence(next_text) && ! thematic(next_text) &&
                                                     byte(next_text, 0) != '>'))) {
                        body.push(Line(next));
                    } else
                        break;
                    if (! next_text->trim_ascii().is_empty()) get(item).span.end = usize(next.end);
                    ++i;
                }
                separated_items    = blank && physical_blank;
                get(list).span.end = get(item).span.end;
                Vec<SourceSpan> blank_lines;
                for (const auto& value : body) {
                    if (! tick()) return;
                    if (cut(source(), value.begin, value.end)->trim_ascii().is_empty())
                        blank_lines.push(SourceSpan { usize(value.begin), usize(value.end) });
                }
                auto has_blank = [&](usize left, usize right) -> bool {
                    usize low {}, high = blank_lines.len();
                    while (low < high) {
                        auto middle = low + (high - low) / usize(2);
                        if (blank_lines[middle].begin < left)
                            low = middle + usize(1);
                        else
                            high = middle;
                    }
                    return low < blank_lines.len() && blank_lines[low].end <= right;
                };
                blocks(rstd::move(body), item, depth + usize(1));
                auto item_children = get(item).children.as_slice();
                for (usize j { 1 }; j < item_children.len(); ++j) {
                    auto left  = get(item_children[j - usize(1)]).span.end;
                    auto right = get(item_children[j]).span.begin;
                    if (has_blank(left, right)) get(list).tight = false;
                }
                for (const auto& definition : document_.definitions_) {
                    if (! tick()) return;
                    if (definition.container != item) continue;
                    for (auto child : item_children) {
                        if (! tick()) return;
                        auto child_span = get(child).span;
                        auto left  = child_span.end <= definition.span.begin ? child_span.end
                                                                             : definition.span.end;
                        auto right = child_span.end <= definition.span.begin ? definition.span.begin
                                                                             : child_span.begin;
                        if (left > right) continue;
                        if (has_blank(left, right)) get(list).tight = false;
                    }
                }
            }
            continue;
        }
        Vec<Line> body;
        body.push(Line(start));
        auto     paragraph_end = line.end;
        unsigned level {};
        ++i;
        while (i < lines.len() && tick()) {
            auto next    = lines[i];
            auto content = strip(next, indentation(next));
            auto value   = cut(source(), content.begin, content.end);
            if (value->trim_ascii().is_empty()) break;
            if (indentation(next) < 4) {
                if (document_.options_.extensions.tables) {
                    auto prepared = prepare_table(lines.as_slice(), i);
                    if (prepared.is_some()) {
                        next_table_ = rstd::move(prepared);
                        break;
                    }
                }
                if (! next.lazy && (level = setext(value))) {
                    paragraph_end = next.end;
                    ++i;
                    break;
                }
                auto list = marker(value);
                auto nonempty_item =
                    list &&
                    ! cut(value, list->width, value.len().to_primitive())->trim_ascii().is_empty();
                if (! next.lazy && (heading(value) || fence(value) || thematic(value) ||
                                    byte(value, 0) == '>' || html_end(value, true, budget_) ||
                                    (nonempty_item && (! list->ordered || list->start == 1))))
                    break;
            }
            body.push(Line(content));
            paragraph_end = next.end;
            ++i;
        }
        body[body.len() - usize(1)] = trim(body[body.len() - usize(1)]);
        auto   content              = mapped(body.as_slice());
        Offset consumed {};
        while (tick()) {
            auto end = reference(content, consumed, parent);
            if (end == consumed) break;
            consumed = end;
        }
        if (consumed == content.text.len().to_primitive()) {
            if (level) --i;
            continue;
        }
        auto node               = add(level ? NodeKind::Heading : NodeKind::Paragraph,
                                      { content.offsets[usize(consumed)], usize(paragraph_end) },
                                      parent);
        get(node).heading_level = level;
        MappedText remaining;
        remaining.text =
            String::make(cut(content.text.as_str(), consumed, content.text.len().to_primitive()));
        for (auto j = usize(consumed); j < content.offsets.len(); ++j)
            remaining.offsets.push(usize(content.offsets[j]));
        for (auto j = usize(consumed); j < content.ends.len(); ++j)
            remaining.ends.push(usize(content.ends[j]));
        for (const auto value : body)
            if (value.end > content.offsets[usize(consumed)].to_primitive())
                get(node).content_spans.push(
                    SourceSpan { usize(value.begin) < content.offsets[usize(consumed)]
                                     ? content.offsets[usize(consumed)]
                                     : usize(value.begin),
                                 usize(value.end) });
        pending_.push(PendingInline { node, rstd::move(remaining) });
    }
}
} // namespace limarkto
