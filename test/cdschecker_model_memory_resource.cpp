#if !CXXTRACE_ENABLE_CDSCHECKER
#error "CXXTRACE_ENABLE_CDSCHECKER must be defined and non-zero."
#endif

#include "cdschecker_model_memory_resource.h"
#include <experimental/memory_resource>

// HACK(strager): Unfortunately, it's difficult to include both <mymemory.h>
// (from CDSChecker) and <experimental/memory_resource> (from the C++ standard
// library) in the same translation unit. The following declarations are
// extracted from <mymemory.h> to avoid including the entire header.
auto model_malloc(std::size_t) -> void*;
auto
model_free(void*) -> void;

namespace cxxtrace_test {
namespace {
class cdschecker_model_memory_resource
  : public std::experimental::pmr::memory_resource
{
protected:
  auto do_allocate(std::size_t size, std::size_t alignment) -> void* override
  {
    if (alignment > alignof(std::max_align_t)) {
      throw std::bad_alloc{};
    }

    auto* p = ::model_malloc(size);
    if (!p) {
      throw std::bad_alloc{};
    }
    return p;
  }

  auto do_deallocate(void* p,
                     [[maybe_unused]] std::size_t size,
                     [[maybe_unused]] std::size_t alignment) -> void override
  {
    ::model_free(p);
  }

  auto do_is_equal(const std::experimental::pmr::memory_resource& other) const
    noexcept -> bool override
  {
    return &other == this;
  }
};
}

auto
get_cdschecker_model_memory_resource() noexcept
  -> std::experimental::pmr::memory_resource*
{
  static auto instance = cdschecker_model_memory_resource{};
  return &instance;
}
}
