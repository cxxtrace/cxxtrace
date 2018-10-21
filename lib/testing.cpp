#include <cxxtrace/detail/sample.h>
#include <cxxtrace/testing.h>
#include <vector>

namespace cxxtrace {
auto
clear_all_samples() noexcept -> void
{
  detail::g_samples.clear();
}
}
