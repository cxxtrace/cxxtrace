#include "memory_resource.h"
#include <cstddef>
#include <memory>
#include <new>

namespace cxxtrace_test {
monotonic_buffer_resource::monotonic_buffer_resource(
  void* buffer,
  std::size_t buffer_size) noexcept
  : buffer{ buffer }
  , buffer_size{ buffer_size }
  , free_space{ buffer }
{}

monotonic_buffer_resource::~monotonic_buffer_resource() = default;

auto
monotonic_buffer_resource::do_allocate(std::size_t size, std::size_t alignment)
  -> void*
{
  auto free_space_size =
    this->buffer_size -
    (static_cast<std::byte*>(free_space) - static_cast<std::byte*>(buffer));
  auto* p = this->free_space;
  p = std::align(alignment, size, p, free_space_size);
  if (!p) {
    throw std::bad_alloc{};
  }
  this->free_space = static_cast<std::byte*>(p) + size;
  return p;
}

auto
monotonic_buffer_resource::do_deallocate(void*, std::size_t, std::size_t)
  -> void
{
  // Do nothing.
}

auto
monotonic_buffer_resource::do_is_equal(
  const std::experimental::pmr::memory_resource& other) const noexcept -> bool
{
  return this == &other;
}
}
