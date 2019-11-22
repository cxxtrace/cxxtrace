#include <cxxtrace/detail/iostream.h>
#include <ostream>

namespace cxxtrace {
namespace detail {
// Reset the formatting options to their defaults.
//
// See std::basic_ios<>::init for details.
auto
reset_ostream_formatting(std::ostream& stream) -> void
{
  stream.fill(stream.widen(' '));
  stream.flags(std::ios_base::dec | std::ios_base::skipws);
  stream.precision(6);
  stream.width(0);
}
}
}
