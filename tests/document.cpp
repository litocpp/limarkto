#include <rstd/test/gtest.hpp>
import limarkto;
using namespace rstd::prelude;
using namespace rstd::literals;

TEST(Document, OwnsSourceAndKeepsHandlesAcrossMoves) {
    auto doc  = limarkto::parse(String::make("中文\t![a](x)\r\nnext\rfinal\n"_str)).unwrap();
    auto root = doc.root();
    EXPECT_EQ(doc.position(usize(0))->line, usize(1));
    EXPECT_EQ(doc.position(usize(16))->line, usize(2));
    EXPECT_TRUE(doc.position(doc.source().len() + usize(1)).is_none());
    auto moved = rstd::move(doc);
    EXPECT_TRUE(moved.node(root).is_some());
    auto other = limarkto::parse(String::make("other"_str)).unwrap();
    EXPECT_TRUE(other.node(root).is_none());
    EXPECT_TRUE(moved.text({ usize(1), usize(2) }).is_none());
    EXPECT_TRUE(moved.text({ usize(3), usize(1) }).is_none());
}

TEST(Document, RejectsInvalidEncodingAndBoundsBeforeReturningATree) {
    limarkto::ParseOptions options;
    options.max_input_bytes = usize(2);
    EXPECT_TRUE(limarkto::parse(String::make("abc"_str), options).unwrap_err().is_InputLimit());
    options           = {};
    options.max_nodes = usize(1);
    EXPECT_TRUE(limarkto::parse(String::make("abc"_str), options).unwrap_err().is_NodeLimit());
    options          = {};
    options.max_work = usize(1);
    EXPECT_TRUE(
        limarkto::parse(String::make("some text"_str), options).unwrap_err().is_WorkLimit());
    options           = {};
    options.max_depth = usize();
    EXPECT_TRUE(limarkto::parse(String::make("x"_str), options).unwrap_err().is_InvalidOptions());
    auto invalid = String::from_utf8_unchecked(Vec<u8>::from("\xff"_bytes));
    EXPECT_TRUE(limarkto::parse(rstd::move(invalid)).unwrap_err().is_InvalidInput());
}
