#include <rstd/test/gtest.hpp>
import limarkto;
using namespace rstd::prelude;
using namespace rstd::literals;

static auto parse_math(ref<str> source) -> limarkto::Document {
    limarkto::ParseOptions options;
    options.extensions      = limarkto::Extensions::gfm();
    options.extensions.math = true;
    return limarkto::parse(String::make(source), options).unwrap();
}
static auto count_kind(const limarkto::Document& document, limarkto::NodeKind kind) -> unsigned {
    unsigned count {};
    for (usize i {}; i < document.node_count(); ++i)
        if (document.node(document.handle({ i }))->get().kind == kind) ++count;
    return count;
}
TEST(Math, DefaultIsDisabledAndCodeIsLiteral) {
    auto plain = limarkto::parse(String::make("$x$\n\n$$\ny\n$$"_str)).unwrap();
    EXPECT_EQ(count_kind(plain, limarkto::NodeKind::InlineMath), 0u);
    EXPECT_EQ(count_kind(plain, limarkto::NodeKind::Math), 0u);
    auto code = parse_math("`$x$`\n\n```text\n$$\ny\n$$\n```\n\n\\$x and $20."_str);
    EXPECT_EQ(count_kind(code, limarkto::NodeKind::InlineMath), 0u);
    EXPECT_EQ(count_kind(code, limarkto::NodeKind::Math), 0u);
}
TEST(Math, InlineDelimiterRunsPreserveTexAndOriginalRanges) {
    auto     doc = parse_math("中文 $ x_1 &amp; \\alpha $ and $$a$b$$ then $a\nb$."_str);
    unsigned found {};
    ref<str> expected[] { "x_1 &amp; \\alpha"_str, "a$b"_str, "a\nb"_str };
    for (usize i {}; i < doc.node_count(); ++i) {
        auto node = doc.node(doc.handle({ i })).unwrap();
        if (node->kind != limarkto::NodeKind::InlineMath) continue;
        ASSERT_LT(found, 3u);
        EXPECT_EQ(node->literal.as_str(), expected[found++]);
        EXPECT_TRUE(doc.text(node->span).unwrap()->starts_with("$"_str));
    }
    EXPECT_EQ(found, 3u);
}
TEST(Math, BlockFencesInterruptParagraphsAndRetainClosure) {
    auto doc  = parse_math("Before\n$$ math\n\\frac{1}{2}\n$$$\nAfter\n\n$$\nunfinished"_str);
    auto root = doc.node(doc.root()).unwrap();
    ASSERT_EQ(root->children.len(), usize(4));
    auto math = doc.node(doc.handle(root->children[usize(1)])).unwrap();
    EXPECT_EQ(math->kind, limarkto::NodeKind::Math);
    EXPECT_EQ(math->literal.as_str(), "\\frac{1}{2}"_str);
    EXPECT_EQ(math->info.as_str(), "math"_str);
    EXPECT_TRUE(math->closed);
    EXPECT_FALSE(doc.node(doc.handle(root->children[usize(3)]))->get().closed);
}
TEST(Math, ContainersAndTablesUseTheSameParser) {
    auto doc = parse_math("> $$\n> x\n> $$\n\n- $y$\n\n| formula |\n| --- |\n| $z$ |\n"_str);
    EXPECT_EQ(count_kind(doc, limarkto::NodeKind::Math), 1u);
    EXPECT_EQ(count_kind(doc, limarkto::NodeKind::InlineMath), 2u);
}
TEST(Math, UnclosedInlineAndMismatchedRunsRemainText) {
    auto doc = parse_math("Before $unclosed\n\n$one$$\n\n$$x$$"_str);
    EXPECT_EQ(count_kind(doc, limarkto::NodeKind::InlineMath), 1u);
    EXPECT_EQ(count_kind(doc, limarkto::NodeKind::Math), 0u);
}
