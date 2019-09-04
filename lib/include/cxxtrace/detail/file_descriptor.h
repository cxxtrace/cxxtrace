#ifndef CXXTRACE_DETAIL_FILE_DESCRIPTOR_H
#define CXXTRACE_DETAIL_FILE_DESCRIPTOR_H

#include <cxxtrace/detail/have.h>

namespace cxxtrace {
namespace detail {
#if CXXTRACE_HAVE_POSIX_FD
class file_descriptor
{
public:
  static inline constexpr auto invalid_fd = int{ -1 };

  explicit file_descriptor(int fd) noexcept;

  file_descriptor(const file_descriptor&) = delete;
  file_descriptor& operator=(const file_descriptor&) = delete;

  ~file_descriptor() noexcept(false);

  auto close() noexcept(false) -> void;
  auto get() const noexcept -> int;
  auto reset() noexcept(false) -> void;
  auto valid() const noexcept -> bool;

private:
  int fd_{ invalid_fd };
};
#endif
}
}

#endif
