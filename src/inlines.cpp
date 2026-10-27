module limarkto;
import :parser;
import liunicode;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace limarkto
{
static auto codepoint(ref<str> text, Offset at, bool previous) -> rstd::Char {
    if ((previous && ! at) || (! previous && at == text.len().to_primitive()))
        return rstd::Char::from_char32(' ').unwrap();
    if (previous) {
        --at;
        while (at && (byte(text, at) & 0xc0) == 0x80) --at;
    }
    return cut(text, at, text.len().to_primitive())->chars().next().unwrap();
}
static auto unicode_punctuation(rstd::Char cp) -> bool {
    if (punctuation(static_cast<unsigned>(cp.to_primitive()))) return true;
    u16 p[] { u16('P') }, s[] { u16('S') };
    return liunicode::property(liunicode::PropertyGroup::GeneralCategory,
                               slice<u16>::from_raw_parts(p, usize(1)))
               .unwrap()
               .contains(cp.to_primitive()) ||
           liunicode::property(liunicode::PropertyGroup::GeneralCategory,
                               slice<u16>::from_raw_parts(s, usize(1)))
               .unwrap()
               .contains(cp.to_primitive());
}
auto html_tag_end(ref<str> text, Offset at, Offset end, ScanBudget& budget) -> Offset {
    auto     part  = cut(text, at, end);
    ref<str> close = ""_str;
    Offset   start {};
    if (part->starts_with("<!-->"_str)) return at + 5;
    if (part->starts_with("<!--->"_str)) return at + 6;
    if (part->starts_with("<!--"_str)) {
        close = "-->"_str;
        start = at + 4;
    } else if (part->starts_with("<?"_str)) {
        close = "?>"_str;
        start = at + 2;
    } else if (part->starts_with("<![CDATA["_str)) {
        close = "]]>"_str;
        start = at + 9;
    } else if (part->starts_with("<!"_str) && byte(part, 2) >= 'A' && byte(part, 2) <= 'Z') {
        close = ">"_str;
        start = at + 2;
    }
    if (! close->is_empty()) {
        auto found = cut(text, start, end)->find(close);
        if (! budget.consume(found.is_some() ? found->to_primitive() + close.len().to_primitive()
                                             : end - start))
            return at;
        return found.is_some() ? start + found->to_primitive() + close.len().to_primitive() : at;
    }
    auto i       = at + 1;
    auto closing = byte(text, i) == '/';
    if (closing) ++i;
    if (! ascii_letter(byte(text, i))) return at;
    while (i < end &&
           (ascii_letter(byte(text, i)) || digit(byte(text, i)) || byte(text, i) == '-')) {
        if (! budget.consume()) return at;
        ++i;
    }
    if (closing) {
        while (i < end && whitespace(byte(text, i))) {
            if (! budget.consume()) return at;
            ++i;
        }
        return byte(text, i) == '>' ? i + 1 : at;
    }
    auto attribute_start = [](unsigned c) {
        return ascii_letter(c) || c == '_' || c == ':';
    };
    while (i < end) {
        if (! budget.consume()) return at;
        auto before_space = i;
        while (i < end && whitespace(byte(text, i))) {
            if (! budget.consume()) return at;
            ++i;
        }
        if (byte(text, i) == '>') return i + 1;
        if (byte(text, i) == '/' && byte(text, i + 1) == '>') return i + 2;
        if (i == before_space || ! attribute_start(byte(text, i))) return at;
        ++i;
        while (i < end && (attribute_start(byte(text, i)) || digit(byte(text, i)) ||
                           byte(text, i) == '-' || byte(text, i) == '.')) {
            if (! budget.consume()) return at;
            ++i;
        }
        auto after_name = i;
        while (i < end && whitespace(byte(text, i))) {
            if (! budget.consume()) return at;
            ++i;
        }
        if (byte(text, i) != '=') {
            i = after_name;
            continue;
        }
        ++i;
        while (i < end && whitespace(byte(text, i))) {
            if (! budget.consume()) return at;
            ++i;
        }
        auto quote = byte(text, i);
        if (quote == '\'' || quote == '"') {
            ++i;
            while (i < end && byte(text, i) != quote) {
                if (! budget.consume()) return at;
                ++i;
            }
            if (i == end) return at;
            ++i;
        } else {
            auto value_begin = i;
            while (i < end && ! whitespace(byte(text, i)) && byte(text, i) != '>') {
                if (! budget.consume()) return at;
                auto c = byte(text, i);
                if (c == '"' || c == '\'' || c == '=' || c == '<' || c == '`') return at;
                ++i;
            }
            if (i == value_begin) return at;
        }
    }
    return at;
}
void Parser::inlines(NodeId            parent,
                     const MappedText& content,
                     Offset            begin,
                     Offset            end,
                     usize             depth) {
    if (depth >= document_.options_.max_depth) {
        error_ = Some(ParseError::DepthLimit(document_.options_.max_depth));
        return;
    }
    auto text   = content.text.as_str();
    auto copied = begin;
    struct Bracket {
        NodeId marker;
        Offset begin {}, label {};
        usize  child_index {};
        bool   image {}, active { true };
    };
    Vec<Bracket> brackets;
    auto         literal = [&](Offset first, Offset last) {
        if (last <= first || error_) return;
        auto id         = add(NodeKind::Text, content.span(first, last), parent);
        get(id).literal = decode(cut(text, first, last));
    };
    for (auto at = begin; at < end && tick();) {
        auto c = byte(text, at);
        if (c == '\\' && at + 1 < end && punctuation(byte(text, at + 1))) {
            at += 2;
            continue;
        }
        if (c == '\n' || (c == '\\' && byte(text, at + 1) == '\n')) {
            auto first = at;
            bool hard  = c == '\\';
            if (c == '\n') {
                auto spaces = at;
                while (spaces > copied && byte(text, spaces - 1) == ' ') --spaces;
                hard  = at - spaces >= 2;
                first = spaces;
            }
            literal(copied, first);
            auto stop = at + (c == '\\' ? 2 : 1);
            add(hard ? NodeKind::HardBreak : NodeKind::SoftBreak,
                content.span(first, stop),
                parent);
            while (stop < end && space(byte(text, stop))) ++stop;
            at = copied = stop;
            continue;
        }
        if (c == '$' && document_.options_.extensions.math) {
            Offset count = 1;
            while (at + count < end && byte(text, at + count) == '$') ++count;
            auto close = at + count;
            bool found {};
            while (close < end && tick()) {
                if (byte(text, close) != '$') {
                    ++close;
                    continue;
                }
                Offset run {};
                while (close + run < end && byte(text, close + run) == '$') ++run;
                if (run == count) {
                    found = true;
                    break;
                }
                close += run;
            }
            if (! found) {
                at += count;
                continue;
            }
            literal(copied, at);
            auto   id = add(NodeKind::InlineMath, content.span(at, close + count), parent);
            String value;
            bool   non_space {};
            for (auto cp : cut(text, at + count, close)->chars()) {
                auto character = cp.to_primitive();
                if (character != ' ' && character != '\n') non_space = true;
                value.push(char32_t(character ? character : 0xfffd));
            }
            if (non_space && value.len() >= usize(2) &&
                (byte(value.as_str(), 0) == ' ' || byte(value.as_str(), 0) == '\n') &&
                (byte(value.as_str(), value.len().to_primitive() - 1) == ' ' ||
                 byte(value.as_str(), value.len().to_primitive() - 1) == '\n'))
                value = String::make(cut(value.as_str(), 1, value.len().to_primitive() - 1));
            get(id).literal = rstd::move(value);
            at = copied = close + count;
            continue;
        }
        if (c == '`') {
            auto count = Offset(1);
            while (at + count < end && byte(text, at + count) == '`') ++count;
            auto close = at + count;
            bool found {};
            while (close < end && tick()) {
                if (byte(text, close) != '`') {
                    ++close;
                    continue;
                }
                Offset run {};
                while (close + run < end && byte(text, close + run) == '`') ++run;
                if (run == count) {
                    found = true;
                    break;
                }
                close += run;
            }
            if (! found) {
                at += count;
                continue;
            }
            literal(copied, at);
            auto   id = add(NodeKind::Code, content.span(at, close + count), parent);
            String code;
            auto   chars = cut(text, at + count, close)->chars();
            bool   non_space {};
            for (auto cp = chars.next(); cp.is_some(); cp = chars.next()) {
                auto value = cp->to_primitive();
                if (value == '\n') value = ' ';
                if (value != ' ') non_space = true;
                code.push(char32_t(value ? value : 0xfffd));
            }
            if (non_space && code.len() >= usize(2) && byte(code.as_str(), 0) == ' ' &&
                byte(code.as_str(), code.len().to_primitive() - 1) == ' ')
                code = String::make(cut(code.as_str(), 1, code.len().to_primitive() - 1));
            get(id).literal = rstd::move(code);
            at = copied = close + count;
            continue;
        }
        if (c == '[' || (c == '!' && byte(text, at + 1) == '[')) {
            literal(copied, at);
            auto width      = c == '!' ? 2 : 1;
            auto id         = add(NodeKind::Text, content.span(at, at + width), parent);
            get(id).literal = String::make(cut(text, at, at + width));
            if (brackets.len() >= document_.options_.max_depth) {
                error_ = Some(ParseError::DepthLimit(document_.options_.max_depth));
                return;
            }
            brackets.push(
                Bracket { id, at, at + width, get(parent).children.len() - usize(1), c == '!' });
            at = copied = at + width;
            continue;
        }
        if (c == ']' && ! brackets.is_empty()) {
            auto                bracket = brackets.pop().unwrap();
            Option<Destination> target;
            Option<usize>       definition;
            String              label;
            auto                form = LinkForm::Inline;
            auto                stop = at + 1;
            if (bracket.active && byte(text, stop) == '(')
                target = destination(cut(text, 0, end), stop + 1, true, budget_);
            if (target.is_none() && bracket.active) {
                auto label_begin = bracket.label, label_end = at;
                form = LinkForm::ShortcutReference;
                bool full {}, valid = true;
                if (byte(text, stop) == '[') {
                    auto     ref_end = stop + 1;
                    unsigned count {};
                    while (ref_end < end && byte(text, ref_end) != ']' && tick()) {
                        if (byte(text, ref_end) == '[') {
                            valid = false;
                            break;
                        }
                        if (byte(text, ref_end) == '\\' && punctuation(byte(text, ref_end + 1)))
                            ++ref_end;
                        if ((byte(text, ref_end) & 0xc0) != 0x80 && ++count > 999) {
                            valid = false;
                            break;
                        }
                        ++ref_end;
                    }
                    if (valid && byte(text, ref_end) == ']') {
                        full = true;
                        if (ref_end > stop + 1) {
                            label_begin = stop + 1;
                            label_end   = ref_end;
                            form        = LinkForm::FullReference;
                        } else
                            form = LinkForm::CollapsedReference;
                        stop = ref_end + 1;
                    } else
                        valid = false;
                }
                if (valid && label_end - label_begin <= 4 * 999) {
                    auto     chars = cut(text, label_begin, label_end)->chars();
                    unsigned count {};
                    for (auto cp = chars.next(); cp.is_some(); cp = chars.next()) {
                        if (! tick()) return;
                        if (++count > 999) {
                            valid = false;
                            break;
                        }
                    }
                    if (! valid) {
                        ++at;
                        continue;
                    }
                    label = normalize_label(cut(text, label_begin, label_end));
                    for (usize i {}; i < document_.definitions_.len() && tick(); ++i) {
                        const auto& ref = document_.definitions_[i];
                        if (ref.label.as_str() == label.as_str()) {
                            auto remaining = document_.options_.max_expanded_bytes - expanded_;
                            if (ref.destination.len() > remaining ||
                                ref.title.len() > remaining - ref.destination.len()) {
                                error_ = Some(ParseError::ExpansionLimit(
                                    document_.options_.max_expanded_bytes));
                                return;
                            }
                            expanded_ += ref.destination.len() + ref.title.len();
                            definition = Some(usize(i));
                            target     = Some(Destination {
                                ref.destination.clone(), ref.title.clone(), full ? stop : at + 1 });
                            break;
                        }
                    }
                }
            }
            if (target.is_some()) {
                literal(copied, at);
                auto id                 = add(bracket.image ? NodeKind::Image : NodeKind::Link,
                                              content.span(bracket.begin, target->end),
                                              parent);
                get(id).label_span      = { content.offsets[usize(bracket.label)],
                                            content.offsets[usize(at)] };
                get(id).destination     = rstd::move(target->url);
                get(id).title           = rstd::move(target->title);
                get(id).link_form       = form;
                get(id).reference_label = rstd::move(label);
                get(id).definition      = definition;
                auto siblings           = rstd::move(get(parent).children);
                get(parent).children    = Vec<NodeId> {};
                for (usize i {}; i < siblings.len() && tick(); ++i) {
                    if (i < bracket.child_index)
                        get(parent).children.push(NodeId(siblings[i]));
                    else if (i == bracket.child_index)
                        get(parent).children.push(NodeId(id));
                    else if (i + usize(1) < siblings.len()) {
                        get(id).children.push(NodeId(siblings[i]));
                        get(siblings[i]).parent = Some(id);
                    }
                }
                if (! bracket.image)
                    for (auto& open : brackets)
                        if (! open.image) open.active = false;
                at = copied = target->end;
            } else
                ++at;
            continue;
        }
        if (c == '*' || c == '_' || (c == '~' && document_.options_.extensions.strikethrough)) {
            literal(copied, at);
            auto stop = at + 1;
            while (stop < end && byte(text, stop) == c) ++stop;
            if (c == '~' && stop - at > 2) {
                copied = at;
                at     = stop;
                continue;
            }
            auto before = codepoint(text, at, true), after = codepoint(text, stop, false);
            auto left =
                ! after.is_whitespace() && (! unicode_punctuation(after) ||
                                            before.is_whitespace() || unicode_punctuation(before));
            auto right =
                ! before.is_whitespace() && (! unicode_punctuation(before) ||
                                             after.is_whitespace() || unicode_punctuation(after));
            auto id         = add(NodeKind::Text, content.span(at, stop), parent);
            get(id).literal = String::make(cut(text, at, stop));
            delimiters_.push(
                Delimiter { id,
                            c,
                            stop - at,
                            left && (c != '_' || ! right || unicode_punctuation(before)),
                            right && (c != '_' || ! left || unicode_punctuation(after)) });
            at = copied = stop;
            continue;
        }
        if (c == '<') {
            auto close = at + 1;
            while (close < end && byte(text, close) != '>' && ! whitespace(byte(text, close)) &&
                   byte(text, close) != '<' && tick())
                ++close;
            if (byte(text, close) == '>') {
                auto inside = cut(text, at + 1, close);
                auto colon  = inside->find(":"_str);
                bool scheme = colon.is_some() && *colon >= usize(2) && *colon <= usize(32) &&
                              ascii_letter(byte(inside, 0));
                if (scheme)
                    for (Offset j = 1; j < colon->to_primitive(); ++j)
                        if (! ascii_letter(byte(inside, j)) && ! digit(byte(inside, j)) &&
                            byte(inside, j) != '+' && byte(inside, j) != '.' &&
                            byte(inside, j) != '-')
                            scheme = false;
                auto separator = inside->find("@"_str);
                bool email     = ! scheme && separator.is_some() && *separator > usize() &&
                                 *separator + usize(1) < inside.len();
                if (email) {
                    for (Offset j {}; j < separator->to_primitive(); ++j) {
                        auto v = byte(inside, j);
                        if (! ascii_letter(v) && ! digit(v) &&
                            ! ".!#$%&'*+/=?^_`{|}~-"_str->contains(cut(inside, j, j + 1)))
                            email = false;
                    }
                    unsigned label_size {};
                    bool     hyphen {};
                    for (auto j = separator->to_primitive() + 1; j < inside.len().to_primitive();
                         ++j) {
                        auto v = byte(inside, j);
                        if (v == '.') {
                            if (! label_size || hyphen) email = false;
                            label_size = 0;
                        } else if (ascii_letter(v) || digit(v)) {
                            ++label_size;
                            hyphen = false;
                        } else if (v == '-' && label_size) {
                            ++label_size;
                            hyphen = true;
                        } else
                            email = false;
                        if (label_size > 63) email = false;
                    }
                    if (! label_size || hyphen) email = false;
                }
                if (scheme || email) {
                    literal(copied, at);
                    auto id           = add(NodeKind::Link, content.span(at, close + 1), parent);
                    get(id).link_form = LinkForm::Autolink;
                    if (email) get(id).destination.push_str("mailto:"_str);
                    get(id).destination.push_str(inside);
                    auto child         = add(NodeKind::Text, content.span(at + 1, close), id);
                    get(child).literal = String::make(inside);
                    for (auto& open : brackets)
                        if (! open.image) open.active = false;
                    at = copied = close + 1;
                    continue;
                }
            }
            auto stop = html_tag_end(text, at, end, budget_);
            if (! tick(stop - at)) return;
            if (stop > at) {
                literal(copied, at);
                auto id         = add(NodeKind::HtmlInline, content.span(at, stop), parent);
                get(id).literal = literal_text(cut(text, at, stop));
                at = copied = stop;
                continue;
            }
        }
        ++at;
    }
    literal(copied, end);
}

void Parser::emphasis(NodeId parent) {
    auto children = get(parent).children.clone();
    for (auto child : children) {
        if (get(child).kind == NodeKind::Link || get(child).kind == NodeKind::Image)
            emphasis(child);
        if (error_) return;
    }
    Vec<usize> openers;
    for (usize i {}; i < delimiters_.len() && tick(); ++i) {
        auto& closer = delimiters_[i];
        if (get(closer.node).parent.is_none() || *get(closer.node).parent != parent) continue;
        while (closer.closes && closer.length && tick()) {
            Option<usize> match;
            for (auto j = openers.len(); j > usize() && tick(); --j) {
                const auto& opener = delimiters_[openers[j - usize(1)]];
                if (! opener.length || opener.character != closer.character) continue;
                if (opener.character == '~' && opener.length != closer.length) continue;
                if (opener.character != '~' && (opener.closes || closer.opens) &&
                    (opener.length + closer.length) % 3 == 0 &&
                    (opener.length % 3 != 0 || closer.length % 3 != 0))
                    continue;
                match = Some(j - usize(1));
                break;
            }
            if (match.is_none()) break;
            auto& opener         = delimiters_[openers[*match]];
            auto  count          = opener.length >= 2 && closer.length >= 2 ? Offset(2) : Offset(1);
            auto  opening        = get(opener.node).span.end - usize(count);
            auto  closing        = get(closer.node).span.begin + usize(count);
            auto  id             = add(opener.character == '~' ? NodeKind::Strikethrough
                                       : count == 2            ? NodeKind::Strong
                                                               : NodeKind::Emphasis,
                                       { opening, closing },
                                       parent);
            auto  siblings       = rstd::move(get(parent).children);
            get(parent).children = Vec<NodeId> {};
            bool collecting {};
            for (auto child : siblings) {
                if (! tick()) return;
                if (child == id) continue;
                if (child == opener.node) {
                    if (opener.length > count) get(parent).children.push(NodeId(child));
                    get(parent).children.push(NodeId(id));
                    collecting = true;
                } else if (child == closer.node) {
                    collecting = false;
                    if (closer.length > count) get(parent).children.push(NodeId(child));
                } else if (collecting) {
                    get(id).children.push(NodeId(child));
                    get(child).parent = Some(id);
                } else
                    get(parent).children.push(NodeId(child));
            }
            opener.length -= count;
            closer.length -= count;
            get(opener.node).span.end -= usize(count);
            get(closer.node).span.begin += usize(count);
            get(opener.node).literal = String::make(
                source()->get(get(opener.node).span.begin, get(opener.node).span.end).unwrap());
            get(closer.node).literal = String::make(
                source()->get(get(closer.node).span.begin, get(closer.node).span.end).unwrap());
            while (openers.len() > *match + usize(1)) (void)openers.pop();
            if (! opener.length) (void)openers.pop();
        }
        if (closer.opens && closer.length) openers.push(usize(i));
    }
}
} // namespace limarkto
