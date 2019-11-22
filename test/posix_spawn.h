#ifndef CXXTRACE_TEST_POSIX_SPAWN_H
#define CXXTRACE_TEST_POSIX_SPAWN_H

#include <cxxtrace/string.h>
#include <spawn.h>
#include <unistd.h>

namespace cxxtrace_test {
class spawn_file_actions
{
public:
  explicit spawn_file_actions() noexcept(false);

  spawn_file_actions(const spawn_file_actions&) = delete;
  spawn_file_actions& operator=(const spawn_file_actions&) = delete;

  spawn_file_actions(spawn_file_actions&&) = delete;
  spawn_file_actions& operator=(spawn_file_actions&&) = delete;

  ~spawn_file_actions() noexcept(false);

  auto get() const noexcept -> const posix_spawn_file_actions_t*;

  auto add_close(int fd) noexcept(false) -> void;
  auto add_dup2(int fd, int new_fd) noexcept(false) -> void;

private:
  posix_spawn_file_actions_t actions;
};

auto
posix_spawnp(cxxtrace::czstring file,
             const ::posix_spawn_file_actions_t* file_actions,
             const ::posix_spawnattr_t* attributes,
             const cxxtrace::czstring* argv,
             const cxxtrace::czstring* envp) -> ::pid_t;

auto
wait_for_child_process_to_exit(::pid_t process_id) -> int;

auto
get_exit_code_from_waitpid_status(int status) noexcept -> int;

cxxtrace::zstring*
get_current_process_environment() noexcept;
}

#endif
