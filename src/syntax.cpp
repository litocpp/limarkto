module limarkto;
import :syntax;
import liunicode;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace limarkto
{
struct Entity {
    const char*   name;
    unsigned char bytes[8];
};
static constexpr Entity entities[] = {
#include "entities.inc"
};

static auto entity_compare(ref<str> text, const char* other) -> int {
    auto   length = text.len().to_primitive();
    Offset i {};
    for (; i < length && other[i]; ++i) {
        if (byte(text, i) < static_cast<unsigned char>(other[i])) return -1;
        if (byte(text, i) > static_cast<unsigned char>(other[i])) return 1;
    }
    return i < length ? 1 : other[i] ? -1 : 0;
}
static auto named_entity(ref<str> name) -> Option<ref<str>> {
    Offset low {}, high = sizeof(entities) / sizeof(Entity);
    while (low < high) {
        auto middle = low + (high - low) / 2;
        auto order  = entity_compare(name, entities[middle].name);
        if (order < 0)
            high = middle;
        else if (order > 0)
            low = middle + 1;
        else {
            Offset length {};
            while (entities[middle].bytes[length]) ++length;
            auto bytes = slice<u8>::from_raw_parts(
                reinterpret_cast<const rstd::byte*>(entities[middle].bytes), usize(length));
            return Some(rstd::str_::from_utf8_unchecked(bytes));
        }
    }
    return None();
}
auto literal_text(ref<str> text) -> String {
    if (! text->contains("\0"_str)) return String::make(text);
    String result;
    auto   chars = text->chars();
    for (auto cp = chars.next(); cp.is_some(); cp = chars.next())
        result.push(cp->to_primitive() ? *cp : rstd::Char::REPLACEMENT_CHARACTER);
    return result;
}
auto decode(ref<str> text) -> String {
    String result;
    auto   length = text.len().to_primitive();
    Offset copied {};
    for (Offset i {}; i < length;) {
        if (byte(text, i) == '\\' && punctuation(byte(text, i + 1))) {
            result.push_str(cut(text, copied, i));
            result.push_ascii(static_cast<char>(byte(text, i + 1)));
            i += 2;
            copied = i;
        } else if (byte(text, i) == 0) {
            result.push_str(cut(text, copied, i));
            result.push(char32_t(0xfffd));
            copied = ++i;
        } else if (byte(text, i) == '&') {
            auto end = i + 1;
            while (end < length && end - i <= 33 && byte(text, end) != ';') ++end;
            bool found = false;
            if (end < length && byte(text, end) == ';') {
                auto name = cut(text, i + 1, end);
                if (byte(name, 0) == '#') {
                    unsigned value {}, count {};
                    Offset   at  = 1;
                    auto     hex = byte(name, at) == 'x' || byte(name, at) == 'X';
                    if (hex) ++at;
                    bool valid = true;
                    for (; at < name.len().to_primitive(); ++at) {
                        auto c      = byte(name, at);
                        auto number = digit(c)                      ? c - '0'
                                      : hex && c >= 'a' && c <= 'f' ? c - 'a' + 10
                                      : hex && c >= 'A' && c <= 'F' ? c - 'A' + 10
                                                                    : 16;
                        if (number >= (hex ? 16u : 10u)) {
                            valid = false;
                            break;
                        }
                        value = value * (hex ? 16 : 10) + number;
                        ++count;
                        if (count > (hex ? 6u : 7u)) {
                            valid = false;
                            break;
                        }
                    }
                    if (valid && count) {
                        if (! value || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
                            value = 0xfffd;
                        result.push_str(cut(text, copied, i));
                        result.push(char32_t(value));
                        found = true;
                    }
                } else if (auto entity = named_entity(name); entity.is_some()) {
                    result.push_str(cut(text, copied, i));
                    result.push_str(*entity);
                    found = true;
                }
            }
            if (found) {
                i      = end + 1;
                copied = i;
            } else
                ++i;
        } else
            ++i;
    }
    result.push_str(cut(text, copied, length));
    return result;
}
auto normalize_label(ref<str> text) -> String {
    String result;
    bool   pending_space = false;
    auto   folded        = liunicode::case_fold_utf8(text).unwrap();
    auto   chars         = folded.as_str()->chars();
    for (auto cp = chars.next(); cp.is_some(); cp = chars.next()) {
        if (cp->is_whitespace()) {
            if (! result.is_empty()) pending_space = true;
        } else {
            if (pending_space) result.push_ascii(' ');
            pending_space = false;
            result.push(*cp);
        }
    }
    return result;
}
auto destination(ref<str> text, Offset begin, bool inline_link, ScanBudget& budget)
    -> Option<Destination> {
    auto     length = text.len().to_primitive();
    auto     at     = begin;
    unsigned newlines {};
    auto     spaces = [&] {
        while (at < length && whitespace(byte(text, at))) {
            if (! budget.consume()) return;
            if (byte(text, at) == '\n') ++newlines;
            ++at;
        }
    };
    spaces();
    if (newlines > 1) return None();
    auto start = at, end = at;
    if (byte(text, at) == '<') {
        start = ++at;
        while (at < length && byte(text, at) != '>') {
            if (! budget.consume()) return None();
            if (byte(text, at) == '\n' || byte(text, at) == '<') return None();
            if (byte(text, at) == '\\' && punctuation(byte(text, at + 1))) ++at;
            ++at;
        }
        if (at == length) return None();
        end = at++;
    } else {
        unsigned depth {};
        while (at < length && ! whitespace(byte(text, at))) {
            if (! budget.consume()) return None();
            auto c = byte(text, at);
            if (c < 32 || c == 127) return None();
            if (c == '\\' && punctuation(byte(text, at + 1))) {
                at += 2;
                continue;
            }
            if (c == '(') {
                if (++depth > 32) return None();
            }
            if (c == ')') {
                if (! depth) break;
                --depth;
            }
            ++at;
        }
        if (depth) return None();
        end = at;
        if (end == start && ! inline_link) return None();
    }
    auto before_spaces = at;
    newlines           = 0;
    spaces();
    if (inline_link && newlines > 1) return None();
    String title;
    auto   c                 = byte(text, at);
    auto   finish_definition = [&](Offset pos) -> Option<Offset> {
        while (pos < length && space(byte(text, pos))) {
            if (! budget.consume()) return None();
            ++pos;
        }
        if (pos < length && byte(text, pos) != '\n') return None();
        return Some(pos < length ? pos + 1 : pos);
    };
    if (newlines <= 1 && at > before_spaces && (c == '\'' || c == '"' || c == '(')) {
        auto title_begin = at;
        auto close       = c == '(' ? ')' : c;
        auto title_start = ++at;
        while (at < length && byte(text, at) != close) {
            if (! budget.consume()) return None();
            if (byte(text, at) == '(' && close == ')') break;
            if (byte(text, at) == '\\' && punctuation(byte(text, at + 1))) ++at;
            ++at;
        }
        if (at < length && byte(text, at) == close &&
            ! cut(text, title_start, at)->contains("\n\n"_str)) {
            title = decode(cut(text, title_start, at));
            ++at;
            if (! inline_link) {
                auto finish = finish_definition(at);
                if (finish.is_some())
                    return Some(
                        Destination { decode(cut(text, start, end)), rstd::move(title), *finish });
            } else {
                newlines = 0;
                spaces();
                if (newlines <= 1 && byte(text, at) == ')')
                    return Some(
                        Destination { decode(cut(text, start, end)), rstd::move(title), at + 1 });
            }
        }
        if (inline_link || ! cut(text, before_spaces, title_begin)->contains("\n"_str))
            return None();
    }
    if (inline_link) {
        if (byte(text, at) != ')') return None();
        ++at;
    } else {
        auto finish = finish_definition(before_spaces);
        if (finish.is_none()) return None();
        at    = *finish;
        title = String {};
    }
    return Some(Destination { decode(cut(text, start, end)), rstd::move(title), at });
}
} // namespace limarkto
