#include <cassert>
#include <cerrno>
#include <cxxtrace/detail/file_descriptor.h>
#include <cxxtrace/detail/have.h>
#include <system_error>
#include <utility>

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

file_descriptor::file_descriptor(file_descriptor&& other) noexcept
  : file_descriptor{ std::exchange(other.fd_, this->invalid_fd) }
{}

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
file_descriptor::reset(int new_fd) noexcept(false) -> void
{
  if (this->valid()) {
    // TODO(strager): If this fails, should we close new_fd?
    this->close();
  }
  [[maybe_unused]] auto old_fd = std::exchange(this->fd_, new_fd);
  assert(old_fd == this->invalid_fd);
}

auto
file_descriptor::valid() const noexcept -> bool
{
  return this->fd_ != this->invalid_fd;
}
#endif
}
}
