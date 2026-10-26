#include <rstd/test/gtest.hpp>
import limarkto;
using namespace rstd::prelude;
using namespace rstd::literals;

TEST(Parser, ResolvesForwardReferencesAndIgnoresCodeAndHtmlBlocks) {
    auto source =
        "中文\t![Image][pic]\n\n[pic]: ../images/a.png\n\n```md\n![Not image](missing.png)\n```\n\n<div>\n![also not](missing)\n</div>\n"_str;
    auto doc    = limarkto::parse(String::make(source)).unwrap();
    auto images = doc.images();
    ASSERT_EQ(images.len(), usize(1));
    EXPECT_EQ(images[usize()].destination, "../images/a.png"_str);
    EXPECT_EQ(doc.text(images[usize()].span).unwrap(), "![Image][pic]"_str);
    EXPECT_EQ(doc.definitions().len(), usize(1));
    EXPECT_TRUE(doc.node(images[usize()].node).unwrap()->definition.is_some());
}

TEST(Parser, DecodesTargetsWithoutReinterpretingEscapesOrCode) {
    auto doc =
        limarkto::parse(
            String::make(
                "\\![no](skip) `![code](skip)` ![x](<a&amp;b&#x2e;png> \"a &copy; title\")"_str))
            .unwrap();
    auto images = doc.images();
    ASSERT_EQ(images.len(), usize(1));
    EXPECT_EQ(images[usize()].destination, "a&b.png"_str);
    EXPECT_EQ(images[usize()].title, "a © title"_str);
}

TEST(Parser, ReferenceLabelsUseFullCaseFoldAndFirstDefinition) {
    auto doc =
        limarkto::parse(String::make("![x][STRASSE]\n\n[straße]: first\n[STRASSE]: second\n"_str))
            .unwrap();
    ASSERT_EQ(doc.images().len(), usize(1));
    EXPECT_EQ(doc.images()[usize()].destination, "first"_str);
}

TEST(Parser, ContainersRetainOriginalImageRanges) {
    auto doc =
        limarkto::parse(
            String::make(
                "# Heading\n\n> ![quote](a)\n> next\n\n- ![item](b)\n  continuation\n\n    ![code](no)\n"_str))
            .unwrap();
    auto images = doc.images();
    ASSERT_GE(images.len(), usize(2));
    EXPECT_EQ(doc.text(images[usize()].span).unwrap(), "![quote](a)"_str);
    EXPECT_EQ(doc.text(images[usize(1)].span).unwrap(), "![item](b)"_str);
}

TEST(Parser, NestedImageAltIsNotAnIndependentResource) {
    auto doc    = limarkto::parse(String::make("[![outer ![inner](b)](a)](link)"_str)).unwrap();
    auto images = doc.images();
    ASSERT_EQ(images.len(), usize(1));
    EXPECT_EQ(images[usize()].destination, "a"_str);
    EXPECT_EQ(doc.text(images[usize()].span).unwrap(), "![outer ![inner](b)](a)"_str);
}

TEST(Parser, InvalidAutolinkAndRawHtmlClosersDoNotHideImages) {
    limarkto::ParseOptions options;
    options.extensions = limarkto::Extensions::gfm();
    auto doc =
        limarkto::parse(
            String::make("(_A_@_.A\n\n<SCRIPT>\n![hidden](no)\n</SCRIPT>\n![visible](yes)\n"_str),
            options)
            .unwrap();
    ASSERT_EQ(doc.images().len(), usize(1));
    EXPECT_EQ(doc.images()[usize()].destination, "yes"_str);
}

TEST(Parser, NullNormalizationPreservesOriginalByteCoordinates) {
    auto source = "![a](x\0y)\n\n```\n\0\n```\n"_str;
    auto doc    = limarkto::parse(String::make(source)).unwrap();
    ASSERT_EQ(doc.images().len(), usize(1));
    EXPECT_EQ(doc.images()[usize()].destination, "x\ufffdy"_str);
    EXPECT_EQ(doc.text(doc.images()[usize()].span).unwrap(), "![a](x\0y)"_str);
    EXPECT_EQ(doc.source(), source);
    auto root = doc.node(doc.root()).unwrap();
    auto code = doc.node(doc.handle(root->children[usize(1)])).unwrap();
    EXPECT_EQ(code->literal.as_str(), "\ufffd\n"_str);
}
