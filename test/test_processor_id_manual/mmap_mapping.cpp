#include "mmap_mapping.h"
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <sys/mman.h>
#include <system_error>
#include <utility>

namespace cxxtrace_test {
mmap_mapping::mmap_mapping(void* data, std::size_t size) noexcept
  : data_{ data }
  , size_{ size }
{}

mmap_mapping::mmap_mapping(mmap_mapping&&) noexcept
{
  __builtin_trap(); // TODO(strager): Implement.
}

mmap_mapping&
mmap_mapping::operator=(mmap_mapping&& other) noexcept(false)
{
  if (this == &other) {
    return *this;
  }
  this->reset(); // TODO(strager): If this fails, should we unmap other?
  std::swap(this->data_, other.data_);
  if (this->data_) {
    this->size_ = other.size_;
  } else {
    // other.size_ is uninitialized. Don't read it.
  }
  return *this;
}

mmap_mapping::~mmap_mapping() noexcept(false)
{
  this->reset();
}

auto
mmap_mapping::mmap_checked(void* addr,
                           std::size_t length,
                           int prot,
                           int flags,
                           int fd,
                           ::off_t offset) noexcept(false) -> mmap_mapping
{
  auto mapping =
    mmap_mapping{ ::mmap(addr, length, prot, flags, fd, offset), length };
  if (!mapping.valid()) {
    throw std::system_error{ errno, std::generic_category(), "mmap failed" };
  }
  return mapping;
}

auto
mmap_mapping::data() const noexcept -> void*
{
  assert(this->valid());
  return this->data_;
}

auto
mmap_mapping::valid() const noexcept -> bool
{
  return this->data_ != MAP_FAILED;
}

auto
mmap_mapping::reset() noexcept(false) -> void
{
  if (this->valid()) {
    this->unmap();
  }
}

auto
mmap_mapping::reset(void* new_data, std::size_t new_size) noexcept(false)
  -> void
{
  if (this->valid()) {
    // TODO(strager): If this fails, should we unmap new_data?
    this->unmap();
  }
  [[maybe_unused]] auto old_data = std::exchange(this->data_, new_data);
  assert(old_data == MAP_FAILED);
  this->size_ = new_size;
}

auto
mmap_mapping::unmap() noexcept(false) -> void
{
  assert(this->valid());
  auto rc = ::munmap(this->data_, this->size_);
  if (rc != 0) {
    throw std::system_error{ errno, std::generic_category(), "munmap failed" };
  }
  this->data_ = MAP_FAILED;
}
}
