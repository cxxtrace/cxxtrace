#ifndef CXXTRACE_DETAIL_IOSTREAM_H
#define CXXTRACE_DETAIL_IOSTREAM_H

#include <iosfwd>

namespace cxxtrace {
namespace detail {
// Reset the formatting options to their defaults.
//
// See std::basic_ios<>::init for details.
auto
reset_ostream_formatting(std::ostream&) -> void;
}
}

#endif
