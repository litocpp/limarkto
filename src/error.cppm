module;
#include <rstd/enum.hpp>
export module limarkto:error;
export import rstd;
using namespace rstd::prelude;
using namespace rstd::literals;

export namespace limarkto
{
class ParseError {
    RSTD_ENUM(ParseError,
              (InvalidInput, (rstd::str_::Utf8Error cause;)),
              (InvalidOptions),
              (InputLimit, (usize limit; usize actual;)),
              (NodeLimit, (usize limit;)),
              (DepthLimit, (usize limit;)),
              (ReferenceLimit, (usize limit;)),
              (ExpansionLimit, (usize limit;)),
              (WorkLimit, (usize limit;)))
};
class RewriteError {
    RSTD_ENUM(RewriteError,
              (InvalidHandle, (usize replacement;)),
              (InvalidTarget, (usize replacement;)),
              (OverlappingEdits),
              (OutputLimit, (usize limit;)))
};
} // namespace limarkto

export namespace rstd
{
template<>
struct Impl<fmt::Display, limarkto::ParseError> : ImplBase<limarkto::ParseError> {
    auto fmt(fmt::Formatter& formatter) const -> bool {
        RSTD_MATCH(this->self()) {
            RSTD_CASE(InvalidInput, cause) {
                return formatter.write_fmt(
                    fmt::Arguments::make("invalid Markdown input: {}", cause));
            }
            RSTD_CASE(InvalidOptions) {
                return formatter.write_str("invalid Markdown options"_str);
            }
            RSTD_CASE(InputLimit, limit, actual) {
                return formatter.write_fmt(fmt::Arguments::make(
                    "Markdown input exceeds {} bytes; found {}", limit, actual));
            }
            RSTD_CASE(NodeLimit, limit) {
                return formatter.write_fmt(
                    fmt::Arguments::make("Markdown node limit of {} exceeded", limit));
            }
            RSTD_CASE(DepthLimit, limit) {
                return formatter.write_fmt(
                    fmt::Arguments::make("Markdown depth limit of {} exceeded", limit));
            }
            RSTD_CASE(ReferenceLimit, limit) {
                return formatter.write_fmt(
                    fmt::Arguments::make("Markdown reference limit of {} exceeded", limit));
            }
            RSTD_CASE(ExpansionLimit, limit) {
                return formatter.write_fmt(
                    fmt::Arguments::make("Markdown reference expansion exceeds {} bytes", limit));
            }
            RSTD_CASE(WorkLimit, limit) {
                return formatter.write_fmt(
                    fmt::Arguments::make("Markdown work limit of {} exceeded", limit));
            }
        }
        rstd::unreachable();
    }
};
template<>
struct Impl<fmt::Debug, limarkto::ParseError> : Impl<fmt::Display, limarkto::ParseError> {};
template<>
struct Impl<error::Error, limarkto::ParseError>
    : DefaultInImpl<error::Error, limarkto::ParseError> {
    auto source() const noexcept -> Option<error::ErrorRef> {
        RSTD_MATCH(this->self()) {
            RSTD_CASE(InvalidInput, cause) {
                return Some(dyn<error::Error>::from_ref(cause));
            }
            RSTD_CASE(InvalidOptions) {
                return None();
            }
            RSTD_CASE(InputLimit) {
                return None();
            }
            RSTD_CASE(NodeLimit) {
                return None();
            }
            RSTD_CASE(DepthLimit) {
                return None();
            }
            RSTD_CASE(ReferenceLimit) {
                return None();
            }
            RSTD_CASE(ExpansionLimit) {
                return None();
            }
            RSTD_CASE(WorkLimit) {
                return None();
            }
        }
        rstd::unreachable();
    }
};
template<>
struct Impl<fmt::Display, limarkto::RewriteError> : ImplBase<limarkto::RewriteError> {
    auto fmt(fmt::Formatter& formatter) const -> bool {
        RSTD_MATCH(this->self()) {
            RSTD_CASE(InvalidHandle, replacement) {
                return formatter.write_fmt(fmt::Arguments::make(
                    "invalid Markdown image handle at replacement {}", replacement));
            }
            RSTD_CASE(InvalidTarget, replacement) {
                return formatter.write_fmt(fmt::Arguments::make(
                    "invalid Markdown image target at replacement {}", replacement));
            }
            RSTD_CASE(OverlappingEdits) {
                return formatter.write_str("overlapping Markdown edits"_str);
            }
            RSTD_CASE(OutputLimit, limit) {
                return formatter.write_fmt(
                    fmt::Arguments::make("Markdown output exceeds {} bytes", limit));
            }
        }
        rstd::unreachable();
    }
};
template<>
struct Impl<fmt::Debug, limarkto::RewriteError> : Impl<fmt::Display, limarkto::RewriteError> {};
template<>
struct Impl<error::Error, limarkto::RewriteError>
    : DefaultInImpl<error::Error, limarkto::RewriteError> {};
} // namespace rstd
