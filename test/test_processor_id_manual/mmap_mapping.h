#ifndef CXXTRACE_TEST_MMAP_MAPPING_H
#define CXXTRACE_TEST_MMAP_MAPPING_H

#include <cstddef>
#include <sys/mman.h>

namespace cxxtrace_test {
class mmap_mapping
{
public:
  /* implicit */ mmap_mapping() noexcept = default;
  explicit mmap_mapping(void* data, std::size_t size) noexcept;

  mmap_mapping(const mmap_mapping&) = delete;
  mmap_mapping& operator=(const mmap_mapping&) = delete;

  mmap_mapping(mmap_mapping&&) noexcept;
  mmap_mapping& operator=(mmap_mapping&&) noexcept(false);

  ~mmap_mapping() noexcept(false);

  static auto mmap_checked(void* addr,
                           std::size_t length,
                           int prot,
                           int flags,
                           int fd,
                           ::off_t offset) noexcept(false) -> mmap_mapping;

  auto data() const noexcept -> void*;
  auto size() const noexcept -> std::size_t;
  auto valid() const noexcept -> bool;

  auto reset() noexcept(false) -> void;
  auto reset(void* data, std::size_t size) noexcept(false) -> void;
  auto unmap() noexcept(false) -> void;

private:
  void* data_{ MAP_FAILED };
  std::size_t size_;
};
}

#endif
