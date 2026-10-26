module limarkto:syntax;
import rstd;
import rstd.parse;
using namespace rstd::prelude;
using namespace rstd::literals;
namespace limarkto
{
using Offset = rstd::size_t;
struct ScanBudget {
    Offset remaining;
    bool   exhausted {};
    auto   consume(Offset amount = 1) -> bool {
        if (exhausted || amount > remaining) {
            exhausted = true;
            return false;
        }
        remaining -= amount;
        return true;
    }
};
inline auto byte(ref<str> text, Offset at) -> unsigned {
    return at < text.len().to_primitive() ? text.as_bytes()[usize(at)].to_primitive() : 0;
}
inline auto cut(ref<str> text, Offset begin, Offset end) -> ref<str> {
    return text->get(usize(begin), usize(end)).unwrap();
}
inline auto space(unsigned value) -> bool {
    return value == ' ' || value == '\t';
}
inline auto whitespace(unsigned value) -> bool {
    return space(value) || value == '\n' || value == '\r';
}
inline auto punctuation(unsigned value) -> bool {
    return (value >= 33 && value <= 47) || (value >= 58 && value <= 64) ||
           (value >= 91 && value <= 96) || (value >= 123 && value <= 126);
}
inline auto ascii_letter(unsigned value) -> bool {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}
inline auto digit(unsigned value) -> bool {
    return value >= '0' && value <= '9';
}
auto decode(ref<str> text) -> String;
auto literal_text(ref<str> text) -> String;
auto normalize_label(ref<str> text) -> String;
struct MappedText {
    String     text;
    Vec<usize> offsets;
    Vec<usize> ends;
    auto       span(Offset begin, Offset end) const -> rstd::parse::Span;
};
struct Line {
    Offset   begin {}, end {}, next {};
    unsigned column {}, padding {};
    bool     lazy {};
};
struct Destination {
    String url;
    String title;
    Offset end {};
};
auto destination(ref<str> text, Offset begin, bool inline_link, ScanBudget& budget)
    -> Option<Destination>;
auto html_tag_end(ref<str> text, Offset begin, Offset end, ScanBudget& budget) -> Offset;
} // namespace limarkto
