#include <rstd/test/gtest.hpp>
import limarkto;
using namespace rstd::prelude;
using namespace rstd::literals;

static auto text_of(const limarkto::Document& document) -> String {
    String text;
    for (usize i {}; i < document.node_count(); ++i) {
        auto node = document.node(document.handle({ i })).unwrap();
        if (node->kind == limarkto::NodeKind::Text) text.push_str(node->literal.as_str());
    }
    return text;
}

TEST(Entities, NamedReferencesPreserveUnicodeSequences) {
    auto doc =
        limarkto::parse(
            String::make(
                "&AMP; &amp; &Aopf; &NotEqualTilde; &CounterClockwiseContourIntegral; &Tab; &NewLine;"_str))
            .unwrap();
    EXPECT_EQ(text_of(doc).as_str(),
              "& & \xf0\x9d\x94\xb8 \xe2\x89\x82\xcc\xb8 \xe2\x88\xb3 \t \n"_str);
}

TEST(Entities, LookupOrdersNamesWithoutTheSemicolon) {
    auto doc = limarkto::parse(String::make("&sup; &sup1; &sup2; &sup3; &supE;"_str)).unwrap();
    EXPECT_EQ(text_of(doc).as_str(), "\xe2\x8a\x83 \xc2\xb9 \xc2\xb2 \xc2\xb3 \xe2\xab\x86"_str);
}

TEST(Entities, CommonMarkRequiresSemicolonsAndPreservesCode) {
    auto doc =
        limarkto::parse(
            String::make(
                "&amp &AMP &Amp; &unknown; &#65; &#x1F600; `&amp;`\n\n```\n&amp;\n```\n"_str))
            .unwrap();
    EXPECT_EQ(text_of(doc).as_str(), "&amp &AMP &Amp; &unknown; A \xf0\x9f\x98\x80 "_str);
    unsigned code_count {};
    for (usize i {}; i < doc.node_count(); ++i) {
        auto node = doc.node(doc.handle({ i })).unwrap();
        if (node->kind == limarkto::NodeKind::Code || node->kind == limarkto::NodeKind::CodeBlock) {
            EXPECT_TRUE(node->literal.as_str()->starts_with("&amp;"_str));
            ++code_count;
        }
    }
    EXPECT_EQ(code_count, 2u);
}
