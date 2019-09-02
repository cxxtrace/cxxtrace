#ifndef CXXTRACE_DETAIL_CDSCHECKER_MUTEX_H
#define CXXTRACE_DETAIL_CDSCHECKER_MUTEX_H

#include <cxxtrace/detail/debug_source_location.h>
#include <type_traits>

namespace cxxtrace {
namespace detail {
#if CXXTRACE_ENABLE_CDSCHECKER
class cdschecker_mutex
{
public:
  explicit cdschecker_mutex();

  cdschecker_mutex(const cdschecker_mutex&) = delete;
  cdschecker_mutex& operator=(const cdschecker_mutex&) = delete;

  ~cdschecker_mutex();

  auto lock(cxxtrace::detail::debug_source_location) noexcept -> void;
  auto unlock(cxxtrace::detail::debug_source_location) noexcept -> void;

  std::aligned_storage_t<16, alignof(void*)> storage_;
};
#endif
}
}

#endif
