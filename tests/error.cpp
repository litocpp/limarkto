#include <rstd/test/gtest.hpp>
import limarkto;
using namespace rstd::prelude;
using namespace rstd::literals;

static_assert(rstd::Impled<limarkto::ParseError, rstd::error::Error>);
static_assert(rstd::Impled<limarkto::RewriteError, rstd::error::Error>);

TEST(Error, EveryParseVariantSupportsDisplayAndDebug) {
    using limarkto::ParseError;
    struct Case {
        ParseError error;
        ref<str>   message;
    };
    auto cases = array<Case, 8> {
        Case { ParseError::InvalidInput(rstd::str_::Utf8Error(usize(2), Some(u8(1)))),
               "invalid Markdown input: invalid utf-8 sequence of 1 bytes from index 2"_str },
        Case { ParseError::InvalidOptions(), "invalid Markdown options"_str },
        Case { ParseError::InputLimit(usize(10), usize(11)),
               "Markdown input exceeds 10 bytes; found 11"_str },
        Case { ParseError::NodeLimit(usize(12)), "Markdown node limit of 12 exceeded"_str },
        Case { ParseError::DepthLimit(usize(13)), "Markdown depth limit of 13 exceeded"_str },
        Case { ParseError::ReferenceLimit(usize(14)),
               "Markdown reference limit of 14 exceeded"_str },
        Case { ParseError::ExpansionLimit(usize(15)),
               "Markdown reference expansion exceeds 15 bytes"_str },
        Case { ParseError::WorkLimit(usize(16)), "Markdown work limit of 16 exceeded"_str }
    };
    for (const auto& value : cases) {
        EXPECT_EQ(rstd::format("{}", value.error).as_str(), value.message);
        EXPECT_EQ(rstd::format("{:?}", value.error).as_str(), value.message);
        EXPECT_EQ(rstd::as<rstd::error::Error>(value.error).source().is_some(),
                  value.error.is_InvalidInput());
    }
}

TEST(Error, EveryRewriteVariantSupportsDisplayAndDebug) {
    using limarkto::RewriteError;
    struct Case {
        RewriteError error;
        ref<str>     message;
    };
    auto cases = array<Case, 4> {
        Case { RewriteError::InvalidHandle(usize(2)),
               "invalid Markdown image handle at replacement 2"_str },
        Case { RewriteError::InvalidTarget(usize(3)),
               "invalid Markdown image target at replacement 3"_str },
        Case { RewriteError::OverlappingEdits(), "overlapping Markdown edits"_str },
        Case { RewriteError::OutputLimit(usize(10)), "Markdown output exceeds 10 bytes"_str }
    };
    for (const auto& value : cases) {
        EXPECT_EQ(rstd::format("{}", value.error).as_str(), value.message);
        EXPECT_EQ(rstd::format("{:?}", value.error).as_str(), value.message);
        EXPECT_TRUE(rstd::as<rstd::error::Error>(value.error).source().is_none());
    }
}

TEST(Error, InvalidInputRetainsUtf8CauseThroughTypeErasure) {
    for (bool incomplete : array<bool, 2> { false, true }) {
        auto bytes = incomplete ? "ok\xe2\x82"_bytes : "ok\xff"_bytes;
        auto failure =
            limarkto::parse(String::from_utf8_unchecked(Vec<u8>::from(bytes))).unwrap_err();
        ASSERT_TRUE(failure.is_InvalidInput());
        EXPECT_EQ(failure.as_InvalidInput().cause.valid_up_to(), usize(2));
        auto erased = Box<rstd::dyn<rstd::error::Error>>::make(rstd::move(failure));
        auto cause  = erased->source();
        ASSERT_TRUE(cause.is_some());
        auto utf8 = rstd::error::downcast_ref<rstd::str_::Utf8Error>(*cause);
        ASSERT_TRUE(utf8.is_some());
        EXPECT_EQ((**utf8).valid_up_to(), usize(2));
        EXPECT_EQ((**utf8).error_len().is_none(), incomplete);
        EXPECT_TRUE((*cause)->source().is_none());
    }
}

TEST(Error, ParserReportsConfiguredLimits) {
    limarkto::ParseOptions options;
    options.max_input_bytes = usize(2);
    auto failure            = limarkto::parse(String::make("abc"_str), options).unwrap_err();
    ASSERT_TRUE(failure.is_InputLimit());
    EXPECT_EQ(failure.as_InputLimit().limit, usize(2));
    EXPECT_EQ(failure.as_InputLimit().actual, usize(3));

    options           = {};
    options.max_nodes = usize(1);
    failure           = limarkto::parse(String::make("abc"_str), options).unwrap_err();
    ASSERT_TRUE(failure.is_NodeLimit());
    EXPECT_EQ(failure.as_NodeLimit().limit, usize(1));

    options           = {};
    options.max_depth = usize(2);
    failure           = limarkto::parse(String::make("*abc*"_str), options).unwrap_err();
    ASSERT_TRUE(failure.is_DepthLimit());
    EXPECT_EQ(failure.as_DepthLimit().limit, usize(2));

    options                = {};
    options.max_references = usize();
    failure = limarkto::parse(String::make("[a]: target\n"_str), options).unwrap_err();
    ASSERT_TRUE(failure.is_ReferenceLimit());
    EXPECT_EQ(failure.as_ReferenceLimit().limit, usize());

    options                    = {};
    options.max_expanded_bytes = usize(2);
    failure = limarkto::parse(String::make("[a]\n\n[a]: target\n"_str), options).unwrap_err();
    ASSERT_TRUE(failure.is_ExpansionLimit());
    EXPECT_EQ(failure.as_ExpansionLimit().limit, usize(2));

    options          = {};
    options.max_work = usize(1);
    failure          = limarkto::parse(String::make("abc"_str), options).unwrap_err();
    ASSERT_TRUE(failure.is_WorkLimit());
    EXPECT_EQ(failure.as_WorkLimit().limit, usize(1));
}

TEST(Error, RewriteReportsReplacementIndexAndOutputLimitWithoutChangingSource) {
    auto doc    = limarkto::parse(String::make("![a](first) ![b](second)"_str)).unwrap();
    auto images = doc.images();
    auto edits  = array<limarkto::ImageReplacement, 2> {
        limarkto::ImageReplacement { images[usize()].node, String::make("new"_str) },
        limarkto::ImageReplacement { doc.root(), String::make("new"_str) }
    };
    auto failure = doc.rewrite_images(edits.as_slice()).unwrap_err();
    ASSERT_TRUE(failure.is_InvalidHandle());
    EXPECT_EQ(failure.as_InvalidHandle().replacement, usize(1));

    edits[usize(1)] = { images[usize(1)].node, String::make("\0"_str) };
    failure         = doc.rewrite_images(edits.as_slice()).unwrap_err();
    ASSERT_TRUE(failure.is_InvalidTarget());
    EXPECT_EQ(failure.as_InvalidTarget().replacement, usize(1));

    edits[usize(1)] = { images[usize(1)].node,
                        String::from_utf8_unchecked(Vec<u8>::from("\xff"_bytes)) };
    failure         = doc.rewrite_images(edits.as_slice()).unwrap_err();
    ASSERT_TRUE(failure.is_InvalidTarget());
    EXPECT_EQ(failure.as_InvalidTarget().replacement, usize(1));

    failure = doc.rewrite_images({}, usize(1)).unwrap_err();
    ASSERT_TRUE(failure.is_OutputLimit());
    EXPECT_EQ(failure.as_OutputLimit().limit, usize(1));
    auto erased = Box<rstd::dyn<rstd::error::Error>>::make(rstd::move(failure));
    EXPECT_EQ(rstd::format("{}", erased).as_str(), "Markdown output exceeds 1 bytes"_str);
    EXPECT_TRUE(erased->source().is_none());
    EXPECT_EQ(doc.source(), "![a](first) ![b](second)"_str);
}
