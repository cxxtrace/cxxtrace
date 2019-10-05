#ifndef CXXTRACE_MEMORY_RESOURCE_H
#define CXXTRACE_MEMORY_RESOURCE_H

#include <cstddef>
#include <experimental/memory_resource>

namespace cxxtrace_test {
// TODO(strager): Use the standard library's monotonic_buffer_resource
// implementation instead.
class monotonic_buffer_resource : public std::experimental::pmr::memory_resource
{
public:
  monotonic_buffer_resource(void* buffer, std::size_t buffer_size) noexcept;

  ~monotonic_buffer_resource() override;

  monotonic_buffer_resource(const monotonic_buffer_resource&) = delete;
  monotonic_buffer_resource& operator=(const monotonic_buffer_resource&) =
    delete;

protected:
  auto do_allocate(std::size_t size, std::size_t alignment) -> void* override;
  auto do_deallocate(void*, std::size_t size, std::size_t alignment)
    -> void override;
  auto do_is_equal(const std::experimental::pmr::memory_resource&) const
    noexcept -> bool override;

private:
  void* buffer;
  std::size_t buffer_size;
  void* free_space;
};
}

#endif
