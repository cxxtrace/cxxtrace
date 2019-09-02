#if !CXXTRACE_ENABLE_CDSCHECKER
#error "CXXTRACE_ENABLE_CDSCHECKER must be defined and non-zero."
#endif

#include <cxxtrace/detail/cdschecker_mutex.h>
#include <cxxtrace/detail/debug_source_location.h>
#include <mutex>
#include <new>

// NOTE(strager): In this translation unit, <mutex> and std::mutex refer to
// CDSChecker's mutex, not the standard library's mutex.
using cdschecker_mutex_impl = std::mutex;
static_assert(alignof(std::mutex_state) == alignof(cdschecker_mutex_impl));
static_assert(sizeof(std::mutex_state) == sizeof(cdschecker_mutex_impl));

namespace cxxtrace {
namespace detail {
namespace {
auto
get(cdschecker_mutex* mutex) noexcept -> cdschecker_mutex_impl*
{
  static_assert(alignof(decltype(mutex->storage_)) ==
                alignof(cdschecker_mutex_impl));
  static_assert(sizeof(decltype(mutex->storage_)) ==
                sizeof(cdschecker_mutex_impl));

  return reinterpret_cast<cdschecker_mutex_impl*>(&mutex->storage_);
}
}

cdschecker_mutex::cdschecker_mutex()
{
  new (get(this)) std::mutex();
}

cdschecker_mutex::~cdschecker_mutex()
{
  get(this)->~mutex();
}

auto cdschecker_mutex::lock(cxxtrace::detail::debug_source_location) noexcept
  -> void
{
  get(this)->lock();
}

auto cdschecker_mutex::unlock(cxxtrace::detail::debug_source_location) noexcept
  -> void
{
  get(this)->unlock();
}
}
}
