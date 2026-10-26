#include <rstd/test/gtest.hpp>
import limarkto;
using namespace rstd::prelude;
using namespace rstd::literals;

TEST(Bounds, ReferenceExpansionAndDepthAreBounded) {
    limarkto::ParseOptions options;
    options.max_expanded_bytes = usize(5);
    EXPECT_TRUE(limarkto::parse(String::make("![x][a] ![x][a]\n\n[a]: long\n"_str), options)
                    .unwrap_err()
                    .is_ExpansionLimit());
    options           = {};
    options.max_depth = usize(4);
    EXPECT_TRUE(limarkto::parse(String::make("**********deep**********"_str), options)
                    .unwrap_err()
                    .is_DepthLimit());
}

TEST(Bounds, FailedHtmlAndDestinationScansConsumeTheSharedBudget) {
    for (auto fragment : array<ref<str>, 2> { "<a key='"_str, "![x](<"_str }) {
        String source = String::make("text "_str);
        source.push_str(fragment);
        for (unsigned i {}; i < 80; ++i) source.push_str("letters "_str);
        limarkto::ParseOptions options;
        options.max_work = source.len() * usize(3) + usize(100);
        auto result      = limarkto::parse(rstd::move(source), options);
        ASSERT_TRUE(result.is_err());
        EXPECT_TRUE(result.unwrap_err().is_WorkLimit());
    }
}

TEST(Bounds, SmallDeterministicInputsKeepRangesAndRewriteSemantics) {
    constexpr char alphabet[] = "ab  \n\r\t[]()!<>`*_~|&;:#'=\\/012";
    unsigned       state      = 13579;
    auto           random     = [&] {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    };
    for (unsigned sample {}; sample < 2000; ++sample) {
        String source;
        auto   size = random() % 256;
        for (unsigned i {}; i < size; ++i)
            source.push_ascii(alphabet[random() % (sizeof(alphabet) - 1)]);
        if (sample % 3 == 0) source.push_str("中 ![x](image)\n"_str);
        limarkto::ParseOptions options;
        options.max_work  = usize(200000);
        options.max_depth = usize(32);
        if (sample % 2) options.extensions = limarkto::Extensions::gfm();
        auto parsed = limarkto::parse(source.clone(), options);
        if (parsed.is_err()) continue;
        const auto& doc = *parsed;
        EXPECT_EQ(doc.source(), source.as_str());
        Vec<limarkto::NodeHandle> pending;
        pending.push(doc.root());
        usize visited {};
        while (! pending.is_empty()) {
            auto handle = pending.pop().unwrap();
            auto node   = doc.node(handle).unwrap();
            ASSERT_LE(++visited, doc.node_count());
            EXPECT_TRUE(doc.text(node->span).is_some());
            for (auto child : node->children) pending.push(doc.handle(child));
        }
        Vec<limarkto::ImageReplacement> edits;
        auto                            images = doc.images();
        for (const auto& image : images) edits.push({ image.node, String::make("/new?a&b|c"_str) });
        EXPECT_EQ(doc.rewrite_images({}).unwrap().source.as_str(), source.as_str());
        auto rewritten = doc.rewrite_images(edits.as_slice()).unwrap();
        auto again     = limarkto::parse(rewritten.source.clone(), options);
        ASSERT_TRUE(again.is_ok());
        ASSERT_EQ(again->images().len(), images.len());
        for (const auto& image : again->images()) EXPECT_EQ(image.destination, "/new?a&b|c"_str);
    }
}
