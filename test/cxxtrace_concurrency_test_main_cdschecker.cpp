#if CXXTRACE_ENABLE_CDSCHECKER
#error "CXXTRACE_ENABLE_CDSCHECKER must be undefined or zero."
#endif

#include "cdschecker_allocator.h"
#include "cxxtrace_concurrency_test_base.h"
#include "stringify.h" // IWYU pragma: keep
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cxxtrace/detail/file_descriptor.h>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/string.h>
#include <dlfcn.h>
#include <iterator>
#include <ostream>
#include <spawn.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <vector>
// IWYU pragma: no_include "stringify_impl.h"

#if CXXTRACE_HAVE_NS_GET_EXECUTABLE_PATH
#include <mach-o/dyld.h>
#endif

#if CXXTRACE_HAVE_LINUX_PROCFS
#include <exception>
#include <unistd.h>
#endif

namespace {
cxxtrace_test::detail::concurrency_test* test_to_run{};

constexpr std::string_view select_test_argument_prefix{
  "--cxxtrace-concurrency-test-index="
};

struct child_process_result
{
  std::vector<std::byte> output;
  int exit_code;
};

[[noreturn]] auto
run_all_tests_in_child_processes(
  const std::vector<cxxtrace_test::detail::concurrency_test*>&,
  const std::vector<cxxtrace::czstring>& argv) -> void;

[[noreturn]] auto
run_single_cdschecker_test(cxxtrace_test::detail::concurrency_test*,
                           const std::vector<cxxtrace::czstring>& argv) -> void;

auto
did_cdschecker_test_succeed(const child_process_result&) -> bool;

auto
cdschecker_main(int argc, char** argv) -> int;

auto
default_test_options(const cxxtrace_test::detail::concurrency_test*)
  -> std::vector<std::string>;

auto
run_child_process(cxxtrace::czstring executable_path,
                  const std::vector<cxxtrace::czstring>& argv)
  -> child_process_result;

auto wait_for_child_process_to_exit(::pid_t) -> int;

auto
get_exit_code_from_waitpid_status(int status) noexcept -> int;

auto
current_executable_path() -> std::string;

auto
string_starts_with(std::string_view haystack, std::string_view needle) noexcept
  -> bool;
}

auto
main(int argc, char** argv) -> int
{
  auto argv_vector = std::vector<cxxtrace::czstring>{ &argv[0], &argv[argc] };

  cxxtrace_test::register_concurrency_tests();
  auto tests = cxxtrace_test::detail::get_registered_concurrency_tests();
  if (tests.empty()) {
    std::fprintf(stderr, "fatal: No concurrency tests were registered\n");
    return EXIT_FAILURE;
  }

  if (argv_vector.size() > 1 &&
      string_starts_with(argv_vector[1], select_test_argument_prefix)) {
    auto parse_select_test_argument = [](std::string_view argument) -> int {
      auto test_index_string = argument;
      test_index_string.remove_prefix(select_test_argument_prefix.size());
      auto test_index = int{};
      auto parse_result =
        std::from_chars(test_index_string.data(),
                        test_index_string.data() + test_index_string.size(),
                        test_index);
      if (parse_result.ec != std::errc{}) {
        throw std::system_error{ std::make_error_code(parse_result.ec),
                                 cxxtrace_test::stringify(
                                   "Parsing ", argument, " failed") };
      }
      return test_index;
    };

    auto test_index = parse_select_test_argument(argv_vector[1]);
    auto test_argv = argv_vector;
    test_argv.erase(test_argv.begin() + 1);
    run_single_cdschecker_test(tests.at(test_index), test_argv);
  } else {
    cxxtrace_test::configure_default_allocator();
    run_all_tests_in_child_processes(tests, argv_vector);
  }
}

extern "C"
{
  __attribute__((visibility("default"))) auto user_main(int, char**) -> int
  {
    cxxtrace_test::detail::run_concurrency_test_from_cdschecker(test_to_run);
    return 0;
  }
}

namespace {
[[noreturn]] auto
run_all_tests_in_child_processes(
  const std::vector<cxxtrace_test::detail::concurrency_test*>& tests,
  const std::vector<cxxtrace::czstring>& argv) -> void
{
  auto executable = current_executable_path();
  for (auto test_index = decltype(tests.size()){ 0 }; test_index < tests.size();
       ++test_index) {
    auto* test = tests[test_index];
    std::fprintf(stderr,
                 "Running test #%d %s...\n",
                 static_cast<int>(test_index),
                 test->name().c_str());

    auto test_argv = argv;
    const auto select_argument =
      cxxtrace_test::stringify(select_test_argument_prefix, test_index);
    test_argv.insert(test_argv.begin() + 1, select_argument.c_str());

    auto result = run_child_process(executable.c_str(), test_argv);
    if (!did_cdschecker_test_succeed(result)) {
      std::fprintf(stderr, "error: Test failed\n");
      std::exit(EXIT_FAILURE);
    }
  }
  std::fprintf(stderr, "All tests passed\n");
  std::exit(EXIT_SUCCESS);
}

auto
did_cdschecker_test_succeed(const child_process_result& result) -> bool
{
  if (result.exit_code != EXIT_SUCCESS) {
    return false;
  }

  constexpr auto success_message =
    std::string_view{ "Number of buggy executions: 0\n" };
  auto it = std::search(
    result.output.begin(),
    result.output.end(),
    success_message.begin(),
    success_message.end(),
    [](std::byte x, char y) noexcept->bool {
      return x == static_cast<std::byte>(y);
    });
  if (it == result.output.end()) {
    return false;
  }

  return true;
}

[[noreturn]] auto
run_single_cdschecker_test(cxxtrace_test::detail::concurrency_test* test,
                           const std::vector<cxxtrace::czstring>& argv) -> void
{
  auto test_argv = argv;
  auto test_options = default_test_options(test);
  std::transform(test_options.begin(),
                 test_options.end(),
                 std::inserter(test_argv, test_argv.begin() + 1),
                 [](const std::string& arg) { return arg.c_str(); });

  test_to_run = test;
  auto exit_code =
    cdschecker_main(test_argv.size(), const_cast<char**>(test_argv.data()));
  std::exit(exit_code);
}

auto
cdschecker_main(int argc, char** argv) -> int
{
  auto* cdschecker_main =
    reinterpret_cast<int (*)(int, char**)>(::dlsym(RTLD_NEXT, "main"));
  if (!cdschecker_main) {
    throw std::runtime_error{ cxxtrace_test::stringify(
      "Could not load CDSChecker: ", ::dlerror()) };
  }
  return cdschecker_main(argc, argv);
}

auto
default_test_options(const cxxtrace_test::detail::concurrency_test* test)
  -> std::vector<std::string>
{
  auto options = std::vector<std::string>{};
  switch (test->test_depth()) {
    case cxxtrace_test::concurrency_test_depth::shallow:
      options.emplace_back("--bound=60");
      break;
    case cxxtrace_test::concurrency_test_depth::full:
      options.emplace_back("--bound=0");
      break;
  }
  return options;
}

struct pipe_result
{
  cxxtrace::detail::file_descriptor reader;
  cxxtrace::detail::file_descriptor writer;
};

auto
create_pipe() -> pipe_result
{
  int fds[2]{};
  auto rc = ::pipe(fds);
  if (rc != 0) {
    throw std::system_error{ errno, std::generic_category(), "pipe failed" };
  }
  return pipe_result{ cxxtrace::detail::file_descriptor{ fds[0] },
                      cxxtrace::detail::file_descriptor{ fds[1] } };
}

class spawn_file_actions
{
public:
  explicit spawn_file_actions() noexcept(false)
  {
    auto rc = ::posix_spawn_file_actions_init(&this->actions);
    if (rc != 0) {
      throw std::system_error{ rc,
                               std::generic_category(),
                               "posix_spawn_file_actions_init failed" };
    }
  }

  spawn_file_actions(const spawn_file_actions&) = delete;
  spawn_file_actions& operator=(const spawn_file_actions&) = delete;

  spawn_file_actions(spawn_file_actions&&) = delete;
  spawn_file_actions& operator=(spawn_file_actions&&) = delete;

  ~spawn_file_actions() noexcept(false)
  {
    auto rc = ::posix_spawn_file_actions_destroy(&this->actions);
    if (rc != 0) {
      throw std::system_error{ rc,
                               std::generic_category(),
                               "posix_spawn_file_actions_destroy failed" };
    }
  }

  auto get() const noexcept -> const posix_spawn_file_actions_t*
  {
    return &this->actions;
  }

  auto add_close(int fd) noexcept(false) -> void
  {
    auto rc = ::posix_spawn_file_actions_addclose(&this->actions, fd);
    if (rc != 0) {
      throw std::system_error{ rc,
                               std::generic_category(),
                               "posix_spawn_file_actions_addclose failed" };
    }
  }

  auto add_dup2(int fd, int new_fd) noexcept(false) -> void
  {
    auto rc = ::posix_spawn_file_actions_adddup2(&this->actions, fd, new_fd);
    if (rc != 0) {
      throw std::system_error{ rc,
                               std::generic_category(),
                               "posix_spawn_file_actions_adddup2 failed" };
    }
  }

private:
  posix_spawn_file_actions_t actions;
};

auto
run_child_process(cxxtrace::czstring executable_path,
                  const std::vector<cxxtrace::czstring>& argv)
  -> child_process_result
{
  auto output_pipe = create_pipe();

  auto make_real_argv = [](const std::vector<cxxtrace::czstring>& const_argv)
    -> std::vector<cxxtrace::zstring> {
    auto real_argv = std::vector<cxxtrace::zstring>{};
    std::transform(const_argv.begin(),
                   const_argv.end(),
                   std::back_inserter(real_argv),
                   [](cxxtrace::czstring arg) {
                     return const_cast<cxxtrace::zstring>(arg);
                   });
    real_argv.emplace_back(nullptr);
    return real_argv;
  };

  auto create_process = [&]() -> ::pid_t {
    auto real_argv = make_real_argv(argv);
    char* const* envp{ nullptr };
    auto file_actions = spawn_file_actions{};
    file_actions.add_dup2(output_pipe.writer.get(), STDOUT_FILENO);
    file_actions.add_dup2(output_pipe.writer.get(), STDERR_FILENO);
    file_actions.add_close(output_pipe.reader.get());
    file_actions.add_close(output_pipe.writer.get());
    auto process_id = ::pid_t{};
    auto rc = ::posix_spawnp(&process_id,
                             executable_path,
                             file_actions.get(),
                             nullptr,
                             real_argv.data(),
                             envp);
    if (rc != 0) {
      throw std::system_error{ rc,
                               std::generic_category(),
                               cxxtrace_test::stringify(
                                 "posix_spawnp failed for executable ",
                                 executable_path) };
    }
    return process_id;
  };

  auto capture_and_forward_pipe_output =
    [](int reader_fd, int output_fd) -> std::vector<std::byte> {
    auto output = std::vector<std::byte>{};
    for (;;) {
      std::byte buffer[1024];
      auto read_size = ::read(reader_fd, buffer, sizeof(buffer));
      if (read_size == -1) {
        throw std::system_error{ errno,
                                 std::generic_category(),
                                 "Failed reading from child process pipe" };
      }
      if (read_size == 0) {
        break;
      }

      output.insert(output.end(), &buffer[0], &buffer[read_size]);

      auto write_size = ::write(output_fd, buffer, read_size);
      if (write_size == -1) {
        throw std::system_error{ errno,
                                 std::generic_category(),
                                 "Failed forwarding child process output" };
      }
      if (write_size != read_size) {
        std::fprintf(stderr,
                     "warning: could not forward child process output\n");
        // Keep going.
      }
    }
    return output;
  };

  auto process_id = create_process();

  output_pipe.writer.close();
  auto output =
    capture_and_forward_pipe_output(output_pipe.reader.get(), STDERR_FILENO);

  auto status = wait_for_child_process_to_exit(process_id);
  auto exit_code = get_exit_code_from_waitpid_status(status);
  return child_process_result{ output, exit_code };
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

auto
current_executable_path() -> std::string
{
#if CXXTRACE_HAVE_NS_GET_EXECUTABLE_PATH
  auto buffer = std::string{};
retry:
  auto old_buffer_size = static_cast<std::uint32_t>(buffer.size() + 1);
  auto buffer_size = old_buffer_size;
  auto rc = ::_NSGetExecutablePath(buffer.data(), &buffer_size);
  buffer.resize(buffer_size - 1);
  if (rc == -1) {
    assert(buffer_size > 1);
    assert(buffer_size > old_buffer_size);
    goto retry;
  }
  return buffer;
#elif CXXTRACE_HAVE_LINUX_PROCFS
  auto buffer = std::string{ '\0', 1 };
retry:
  assert(!buffer.empty());
  auto rc = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
  if (rc == -1) {
    std::perror("fatal: could not get path of current executable");
    std::terminate();
  }
  assert(rc >= 0);
  auto written_size = std::size_t(rc);
  assert(written_size <= buffer.size());
  if (written_size == buffer.size()) {
    auto new_buffer_size = buffer.size() * 2;
    assert(new_buffer_size > buffer.size());
    buffer.resize(new_buffer_size - 1);
    goto retry;
  }
  return buffer;
#else
#error "Unknown platform."
#endif
}

auto
string_starts_with(std::string_view haystack, std::string_view needle) noexcept
  -> bool
{
  if (needle.size() > haystack.size()) {
    return false;
  }
  return haystack.substr(0, needle.size()) == needle;
}
}
