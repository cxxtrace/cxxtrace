#include "posix_spawn.h"
#include "stringify.h" // IWYU pragma: keep
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cxxtrace/string.h>
#include <ostream>
#include <spawn.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
// IWYU pragma: no_include "stringify_impl.h"

namespace cxxtrace_test {
spawn_file_actions::spawn_file_actions() noexcept(false)
{
  auto rc = ::posix_spawn_file_actions_init(&this->actions);
  if (rc != 0) {
    throw std::system_error{ rc,
                             std::generic_category(),
                             "posix_spawn_file_actions_init failed" };
  }
}

spawn_file_actions::~spawn_file_actions() noexcept(false)
{
  auto rc = ::posix_spawn_file_actions_destroy(&this->actions);
  if (rc != 0) {
    throw std::system_error{ rc,
                             std::generic_category(),
                             "posix_spawn_file_actions_destroy failed" };
  }
}

auto
spawn_file_actions::get() const noexcept -> const posix_spawn_file_actions_t*
{
  return &this->actions;
}

auto
spawn_file_actions::add_close(int fd) noexcept(false) -> void
{
  auto rc = ::posix_spawn_file_actions_addclose(&this->actions, fd);
  if (rc != 0) {
    throw std::system_error{ rc,
                             std::generic_category(),
                             "posix_spawn_file_actions_addclose failed" };
  }
}

auto
spawn_file_actions::add_dup2(int fd, int new_fd) noexcept(false) -> void
{
  auto rc = ::posix_spawn_file_actions_adddup2(&this->actions, fd, new_fd);
  if (rc != 0) {
    throw std::system_error{ rc,
                             std::generic_category(),
                             "posix_spawn_file_actions_adddup2 failed" };
  }
}

auto
posix_spawnp(cxxtrace::czstring file,
             const ::posix_spawn_file_actions_t* file_actions,
             const ::posix_spawnattr_t* attributes,
             const cxxtrace::czstring* argv,
             const cxxtrace::czstring* envp) -> ::pid_t
{
  assert(argv);
  ::pid_t process_id /* uninitialized */;
  auto rc = ::posix_spawnp(&process_id,
                           file,
                           file_actions,
                           attributes,
                           const_cast<char* const*>(argv),
                           const_cast<char* const*>(envp));
  if (rc != 0) {
    throw std::system_error{ rc,
                             std::generic_category(),
                             cxxtrace_test::stringify(
                               "posix_spawnp failed for executable ", file) };
  }
  return process_id;
}

auto
wait_for_child_process_to_exit(::pid_t process_id) -> int
{
  auto status = int{};
retry:
  auto rc = ::waitpid(process_id, &status, 0);
  if (rc == -1) {
    auto error = errno;
    if (error == EINTR) {
      goto retry;
    }
    throw std::system_error{ error, std::generic_category(), "waitpid failed" };
  }
  return status;
}

auto
get_exit_code_from_waitpid_status(int status) noexcept -> int
{
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    auto exit_code = WTERMSIG(status) + 128;
    assert(exit_code != EXIT_SUCCESS);
    return exit_code;
  }
  assert(!WIFSTOPPED(status));
  assert(false && "waitpid returned an unexpected status code");
  return EXIT_FAILURE;
}
}
