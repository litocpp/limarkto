#include <rstd/test/gtest.hpp>
import limarkto;
using namespace rstd::prelude;
using namespace rstd::literals;

TEST(Rewrite, NoOpIsByteExactAndSharedDefinitionsAreNotChanged) {
    auto source =
        "中文\t![Image][pic]\r\n\r\n[pic]: ../images/a.png 'title'\r\n\r\n```md\r\n![no](missing)\r\n```\r\n"_str;
    auto doc = limarkto::parse(String::make(source)).unwrap();
    EXPECT_EQ(doc.rewrite_images({}).unwrap().source.as_str(), source);
    auto images = doc.images();
    ASSERT_EQ(images.len(), usize(1));
    Vec<limarkto::ImageReplacement> replacements;
    replacements.push({ images[usize()].node, String::make("/api/resources/image/content"_str) });
    auto changed = doc.rewrite_images(replacements.as_slice()).unwrap();
    EXPECT_TRUE(changed.source.as_str()->contains(
        "中文\t![Image](/api/resources/image/content \"title\")\r\n"_str));
    EXPECT_TRUE(changed.source.as_str()->contains("[pic]: ../images/a.png 'title'\r\n"_str));
    EXPECT_TRUE(changed.source.as_str()->contains("```md\r\n![no](missing)\r\n```\r\n"_str));
    EXPECT_EQ(changed.images[usize()].as_str(), "/api/resources/image/content"_str);
    EXPECT_EQ(doc.rewrite_images({}).unwrap().source.as_str(), source);
}

TEST(Rewrite, TargetEscapingRoundTripsAndPreservesOuterLink) {
    auto doc =
        limarkto::parse(String::make("[![alt *text*](old \"title\")](outer)\n"_str)).unwrap();
    Vec<limarkto::ImageReplacement> replacements;
    auto                            url = "a<> &\\\"(b)\n中"_str;
    replacements.push({ doc.images()[usize()].node, String::make(url) });
    auto changed = doc.rewrite_images(replacements.as_slice()).unwrap();
    auto parsed  = limarkto::parse(changed.source.clone()).unwrap();
    ASSERT_EQ(parsed.images().len(), usize(1));
    EXPECT_EQ(parsed.images()[usize()].destination, url);
    EXPECT_TRUE(changed.source.as_str()->ends_with("](outer)\n"_str));
    EXPECT_EQ(parsed.images()[usize()].title, "title"_str);
}

TEST(Rewrite, RejectsForeignDuplicateAndInvalidTargetsAtomically) {
    auto doc   = limarkto::parse(String::make("![x](old)"_str)).unwrap();
    auto other = limarkto::parse(String::make("![x](other)"_str)).unwrap();
    Vec<limarkto::ImageReplacement> edits;
    edits.push({ other.images()[usize()].node, String::make("x"_str) });
    EXPECT_TRUE(doc.rewrite_images(edits.as_slice()).unwrap_err().is_InvalidHandle());
    edits.clear();
    edits.push({ doc.images()[usize()].node, String::make("x"_str) });
    edits.push({ doc.images()[usize()].node, String::make("y"_str) });
    EXPECT_TRUE(doc.rewrite_images(edits.as_slice()).unwrap_err().is_OverlappingEdits());
    edits.clear();
    edits.push({ doc.images()[usize()].node, String::make("\0"_str) });
    EXPECT_TRUE(doc.rewrite_images(edits.as_slice()).unwrap_err().is_InvalidTarget());
    EXPECT_TRUE(doc.rewrite_images({}, usize(2)).unwrap_err().is_OutputLimit());
    EXPECT_EQ(doc.source(), "![x](old)"_str);
}

TEST(Rewrite, MultilineQuoteAndTableTargetsRetainContainerSyntax) {
    for (auto source :
         array<ref<str>, 2> { "> ![a\n> ](old)\n"_str, "a | b\n--|--\n![a](old) | text\n"_str }) {
        limarkto::ParseOptions options;
        options.extensions = limarkto::Extensions::gfm();
        auto doc           = limarkto::parse(String::make(source), options).unwrap();
        ASSERT_EQ(doc.images().len(), usize(1));
        Vec<limarkto::ImageReplacement> edits;
        edits.push({ doc.images()[usize()].node, String::make("new|target"_str) });
        auto changed = doc.rewrite_images(edits.as_slice()).unwrap();
        auto again   = limarkto::parse(changed.source.clone(), options).unwrap();
        ASSERT_EQ(again.images().len(), usize(1));
        EXPECT_EQ(again.images()[usize()].destination, "new|target"_str);
    }
}
