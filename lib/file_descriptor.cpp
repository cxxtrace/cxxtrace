#include <cassert>
#include <cerrno>
#include <cxxtrace/detail/file_descriptor.h>
#include <cxxtrace/detail/have.h>
#include <system_error>

#if CXXTRACE_HAVE_POSIX_FD
#include <unistd.h>
#endif

namespace cxxtrace {
namespace detail {
#if CXXTRACE_HAVE_POSIX_FD
file_descriptor::file_descriptor(int fd) noexcept
  : fd_{ fd }
{
  assert(fd == this->invalid_fd || fd >= 0);
}

file_descriptor::~file_descriptor() noexcept(false)
{
  this->reset();
}

auto
file_descriptor::close() noexcept(false) -> void
{
  assert(this->valid());
  auto rc = ::close(this->fd_);
  if (rc != 0) {
    throw std::system_error{ errno, std::generic_category(), "close failed" };
  }
  this->fd_ = this->invalid_fd;
}

auto
file_descriptor::get() const noexcept -> int
{
  assert(this->valid());
  return this->fd_;
}

auto
file_descriptor::reset() noexcept(false) -> void
{
  if (this->valid()) {
    this->close();
  }
}

auto
file_descriptor::valid() const noexcept -> bool
{
  return this->fd_ != this->invalid_fd;
}
#endif
}
}
