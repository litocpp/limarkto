module limarkto;
import :syntax;
using namespace rstd::prelude;
using namespace rstd::literals;
namespace limarkto
{
static auto escaped_target(ref<str> target, bool title) -> String {
    String result;
    auto   chars = target->chars();
    for (auto cp = chars.next(); cp.is_some(); cp = chars.next()) {
        auto c = cp->to_primitive();
        if (c == '&')
            result.push_str("&amp;"_str);
        else if (c == '<')
            result.push_str("&#60;"_str);
        else if (c == '>')
            result.push_str("&#62;"_str);
        else if (c == '\\')
            result.push_str("&#92;"_str);
        else if (c == '|')
            result.push_str("&#124;"_str);
        else if ((title && c == '"') || c < 32 || c == 127)
            result.push_str(rstd::format("&#{};", static_cast<unsigned>(c)).as_str());
        else
            result.push(*cp);
    }
    return result;
}
auto Document::rewrite_images(slice<ImageReplacement> replacements, usize max_output_bytes) const
    -> Result<RewriteResult, RewriteError> {
    if (replacements.len() > nodes_.len()) return Err(RewriteError::OverlappingEdits());
    auto                     uses = images();
    Vec<bool>                eligible;
    Vec<Option<ref<String>>> targets;
    for (usize i {}; i < nodes_.len(); ++i) {
        eligible.push(false);
        targets.push(None());
    }
    for (const auto& image : uses) eligible[image.node.id().index] = true;
    for (usize replacement_index {}; replacement_index < replacements.len(); ++replacement_index) {
        const auto& replacement = replacements[replacement_index];
        auto        selected    = node(replacement.node);
        if (selected.is_none() || ! eligible[replacement.node.id().index])
            return Err(RewriteError::InvalidHandle(replacement_index));
        auto index = replacement.node.id().index;
        if (targets[index].is_some()) return Err(RewriteError::OverlappingEdits());
        if (replacement.destination.len() > max_output_bytes)
            return Err(RewriteError::OutputLimit(max_output_bytes));
        if (rstd::str_::from_utf8(replacement.destination.as_str().as_bytes()).is_err() ||
            replacement.destination.as_str()->contains("\0"_str))
            return Err(RewriteError::InvalidTarget(replacement_index));
        targets[index] =
            Some(ref<String>::from_raw_parts(rstd::addressof(replacement.destination)));
    }
    RewriteResult result;
    usize         copied {};
    auto          append = [&](ref<str> text) -> bool {
        if (text.len() > max_output_bytes - result.source.len()) return false;
        result.source.push_str(text);
        return true;
    };
    for (const auto& image : uses) {
        auto target = targets[image.node.id().index];
        auto url    = target.is_some() ? (*target)->as_str() : image.destination;
        result.images.push(String::make(url));
        if (target.is_none() || url == image.destination) continue;
        if (image.span.begin < copied) return Err(RewriteError::OverlappingEdits());
        const auto& selected = nodes_[image.node.id().index];
        auto angle = url->contains(" "_str) || url->contains("\t"_str) || url->contains("\n"_str) ||
                     url->contains("\r"_str) || url->contains("("_str) || url->contains(")"_str);
        if (! append(text({ copied, image.span.begin }).unwrap()) ||
            ! append(text({ image.span.begin, selected.label_span.end + usize(1) }).unwrap()) ||
            ! append(angle ? "(<"_str : "("_str) || ! append(escaped_target(url, false).as_str()) ||
            (angle && ! append(">"_str)))
            return Err(RewriteError::OutputLimit(max_output_bytes));
        if (! selected.title.is_empty()) {
            if (! append(" \""_str) ||
                ! append(escaped_target(selected.title.as_str(), true).as_str()) ||
                ! append("\""_str))
                return Err(RewriteError::OutputLimit(max_output_bytes));
        }
        if (! append(")"_str)) return Err(RewriteError::OutputLimit(max_output_bytes));
        copied = image.span.end;
    }
    if (! append(text({ copied, source_.len() }).unwrap()))
        return Err(RewriteError::OutputLimit(max_output_bytes));
    return Ok(rstd::move(result));
}
} // namespace limarkto
