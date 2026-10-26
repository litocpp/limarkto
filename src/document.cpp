module limarkto;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace limarkto
{
auto Document::position(usize byte_offset) const -> Option<rstd::parse::SourcePosition> {
    if (byte_offset > source_.len()) return None();
    usize low {}, high = line_starts_.len();
    while (low < high) {
        auto middle = low + (high - low) / usize(2);
        if (line_starts_[middle] <= byte_offset)
            low = middle + usize(1);
        else
            high = middle;
    }
    auto line = low == usize() ? usize() : low - usize(1);
    return Some(rstd::parse::SourcePosition { line + usize(1),
                                              byte_offset - line_starts_[line] + usize(1) });
}

auto Document::images() const -> Vec<ImageView> {
    Vec<ImageView> result;
    Vec<NodeId>    pending;
    pending.push(NodeId {});
    while (! pending.is_empty()) {
        auto        id      = pending.pop().unwrap();
        const auto& current = nodes_[id.index];
        if (current.kind == NodeKind::Image) {
            result.push(ImageView {
                handle(id), current.span, current.destination.as_str(), current.title.as_str() });
            continue;
        }
        for (auto i = current.children.len(); i > usize(); --i)
            pending.push(NodeId(current.children[i - usize(1)]));
    }
    return result;
}
} // namespace limarkto
