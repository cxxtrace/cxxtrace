#include <cxxtrace/detail/storage.h>
#include <cxxtrace/testing.h>
#include <vector>

namespace cxxtrace {
auto
clear_all_samples() noexcept -> void
{
  detail::storage::clear_all_samples();
}
}
