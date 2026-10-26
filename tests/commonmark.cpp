#include <rstd/test/gtest.hpp>
import limarkto;
import rstd.json;
using namespace rstd::prelude;
using namespace rstd::literals;
using namespace limarkto;

namespace
{
auto escape(ref<str> text, bool url = false) -> String {
    String         output;
    constexpr char hex[] = "0123456789ABCDEF";
    auto           chars = text->chars();
    for (auto cp = chars.next(); cp.is_some(); cp = chars.next()) {
        auto c = cp->to_primitive();
        if (url && (c <= 32 || c >= 127 || c == '<' || c == '>' || c == '\\' || c == '"' ||
                    c == '`' || c == '[' || c == ']')) {
            rstd::byte bytes[4];
            auto       length = cp->encode_utf8(bytes);
            for (usize i {}; i < length; ++i) {
                auto b = static_cast<unsigned>(bytes[i.to_primitive()]);
                output.push_ascii('%');
                output.push_ascii(hex[b >> 4]);
                output.push_ascii(hex[b & 15]);
            }
        } else if (c == '&')
            output.push_str("&amp;"_str);
        else if (c == '<')
            output.push_str("&lt;"_str);
        else if (c == '>')
            output.push_str("&gt;"_str);
        else if (c == '"')
            output.push_str("&quot;"_str);
        else
            output.push(*cp);
    }
    return output;
}
void render_node(const Document& doc,
                 NodeHandle      handle,
                 String&         out,
                 bool            tight = false,
                 bool            alt   = false) {
    const auto& node  = doc.node(handle).unwrap().get();
    auto        write = [&](ref<str> text) {
        out.push_str(text);
    };
    auto children = [&](bool child_tight = false, bool child_alt = false) {
        for (auto child : node.children)
            render_node(doc, doc.handle(child), out, child_tight, child_alt);
    };
    if (alt) {
        if (node.kind == NodeKind::Text || node.kind == NodeKind::Code ||
            node.kind == NodeKind::HtmlInline)
            write(escape(node.literal.as_str()).as_str());
        else if (node.kind == NodeKind::SoftBreak || node.kind == NodeKind::HardBreak)
            write(" "_str);
        else
            children(false, true);
        return;
    }
    switch (node.kind) {
    case NodeKind::Document: children(); break;
    case NodeKind::Text: write(escape(node.literal.as_str()).as_str()); break;
    case NodeKind::Paragraph:
        if (! tight) write("<p>"_str);
        children();
        if (! tight) write("</p>\n"_str);
        break;
    case NodeKind::Heading:
        write(rstd::format("<h{}>", node.heading_level).as_str());
        children();
        write(rstd::format("</h{}>\n", node.heading_level).as_str());
        break;
    case NodeKind::ThematicBreak: write("<hr />\n"_str); break;
    case NodeKind::Code:
        write("<code>"_str);
        write(escape(node.literal.as_str()).as_str());
        write("</code>"_str);
        break;
    case NodeKind::CodeBlock: {
        write("<pre><code"_str);
        if (! node.info.is_empty()) {
            auto info = node.info.as_str();
            auto end  = usize();
            for (auto c : info.as_bytes()) {
                if (c == u8(' ') || c == u8('\t')) break;
                ++end;
            }
            write(" class=\"language-"_str);
            write(escape(info->get(usize(), end).unwrap()).as_str());
            write("\""_str);
        }
        write(">"_str);
        write(escape(node.literal.as_str()).as_str());
        write("</code></pre>\n"_str);
        break;
    }
    case NodeKind::BlockQuote:
        write("<blockquote>\n"_str);
        children();
        write("</blockquote>\n"_str);
        break;
    case NodeKind::List:
        if (node.ordered && node.list_start != 1)
            write(rstd::format("<ol start=\"{}\">\n", node.list_start).as_str());
        else
            write(node.ordered ? "<ol>\n"_str : "<ul>\n"_str);
        children(node.tight);
        write(node.ordered ? "</ol>\n"_str : "</ul>\n"_str);
        break;
    case NodeKind::Item: {
        write("<li>"_str);
        if (node.checked.is_some())
            write(*node.checked ? "<input type=\"checkbox\" checked=\"\" disabled=\"\" /> "_str
                                : "<input type=\"checkbox\" disabled=\"\" /> "_str);
        bool first = true;
        bool previous_paragraph {};
        for (auto child : node.children) {
            const auto& value = doc.node(doc.handle(child)).unwrap().get();
            if (first && (! tight || value.kind != NodeKind::Paragraph))
                write("\n"_str);
            else if (! first && tight && previous_paragraph)
                write("\n"_str);
            render_node(doc, doc.handle(child), out, tight);
            first              = false;
            previous_paragraph = value.kind == NodeKind::Paragraph;
        }
        write("</li>\n"_str);
        break;
    }
    case NodeKind::SoftBreak: write("\n"_str); break;
    case NodeKind::HardBreak: write("<br />\n"_str); break;
    case NodeKind::Emphasis:
        write("<em>"_str);
        children();
        write("</em>"_str);
        break;
    case NodeKind::Strong:
        write("<strong>"_str);
        children();
        write("</strong>"_str);
        break;
    case NodeKind::Strikethrough:
        write("<del>"_str);
        children();
        write("</del>"_str);
        break;
    case NodeKind::Table: {
        write("<table>\n<thead>\n"_str);
        bool first = true;
        for (auto child : node.children) {
            if (! first && doc.node(doc.handle(child)).unwrap()->table_header == false &&
                child == node.children[usize(1)])
                write("<tbody>\n"_str);
            render_node(doc, doc.handle(child), out);
            if (first) write("</thead>\n"_str);
            first = false;
        }
        if (node.children.len() > usize(1)) write("</tbody>\n"_str);
        write("</table>\n"_str);
        break;
    }
    case NodeKind::TableRow:
        write("<tr>\n"_str);
        children();
        write("</tr>\n"_str);
        break;
    case NodeKind::TableCell:
        write(node.table_header ? "<th"_str : "<td"_str);
        if (node.alignment != TableAlignment::None) {
            write(" align=\""_str);
            write(node.alignment == TableAlignment::Left    ? "left"_str
                  : node.alignment == TableAlignment::Right ? "right"_str
                                                            : "center"_str);
            write("\""_str);
        }
        write(">"_str);
        children();
        write(node.table_header ? "</th>\n"_str : "</td>\n"_str);
        break;
    case NodeKind::Link:
    case NodeKind::Image:
        write(node.kind == NodeKind::Image ? "<img src=\""_str : "<a href=\""_str);
        write(escape(node.destination.as_str(), true).as_str());
        write("\""_str);
        if (node.kind == NodeKind::Image) {
            write(" alt=\""_str);
            children(false, true);
            write("\""_str);
        }
        if (! node.title.is_empty()) {
            write(" title=\""_str);
            write(escape(node.title.as_str()).as_str());
            write("\""_str);
        }
        if (node.kind == NodeKind::Image)
            write(" />"_str);
        else {
            write(">"_str);
            children();
            write("</a>"_str);
        }
        break;
    case NodeKind::HtmlBlock:
    case NodeKind::HtmlInline: write(node.literal.as_str()); break;
    }
}
} // namespace

TEST(CommonMark, PinnedOfficialExamples) {
    auto input_path = rstd::env::var("LIMARKTO_COMMONMARK_INPUT"_str);
    ASSERT_TRUE(input_path.is_ok());
    auto output_path = rstd::env::var("LIMARKTO_COMMONMARK_OUTPUT"_str).unwrap();
    auto source =
        rstd::fs::read_to_string(rstd::path::PathBuf::from(input_path->as_str()).as_path())
            .unwrap();
    auto                   cases = rstd::json::from_str(source.as_str()).unwrap();
    Vec<rstd::json::Value> failures;
    usize                  passed {};
    usize                  ignored {};
    ParseOptions           options;
    auto                   trace = rstd::env::var("LIMARKTO_TRACE_CASES"_str).is_ok();
    if (rstd::env::var("LIMARKTO_GFM"_str).is_ok()) options.extensions = Extensions::gfm();
    for (const auto& item : cases.as_array()->get()) {
        if (trace)
            rstd::io::println("example {}", item.get("example"_str).unwrap()->as_u64().unwrap());
        auto markdown = item.get("markdown"_str).unwrap()->as_str().unwrap();
        auto expected = item.get("html"_str).unwrap()->as_str().unwrap();
        if (expected == "<IGNORE>\n"_str) {
            ++ignored;
            continue;
        }
        auto   parsed = limarkto::parse(String::make(markdown), options);
        String actual;
        if (parsed.is_ok())
            render_node(*parsed, parsed->root(), actual);
        else
            actual = rstd::format("parse error: {:?}", parsed.unwrap_err());
        if (actual.as_str() == expected) {
            ++passed;
            continue;
        }
        auto copy = item.clone();
        copy.as_object_mut().unwrap()->insert(String::make("actual"_str),
                                              rstd::json::Value::String(rstd::move(actual)));
        failures.push(rstd::move(copy));
    }
    rstd::io::println("Markdown examples: {} passed, {} failed, {} explicitly ignored by source",
                      passed,
                      failures.len(),
                      ignored);
    auto count   = failures.len();
    auto encoded = rstd::json::to_string(rstd::json::Value::Array(rstd::move(failures)));
    auto file =
        rstd::fs::File::create(rstd::path::PathBuf::from(output_path.as_str()).as_path()).unwrap();
    file.write_all(encoded.as_str().as_bytes()).unwrap();
    EXPECT_EQ(count, usize());
}
