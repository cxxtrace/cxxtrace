#include "sample_checker.h"
#include "test_processor_id_manual.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cxxtrace/clock.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <filesystem>
#include <fstream>
#include <getopt.h>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace std::literals::chrono_literals;
using namespace std::literals::string_view_literals;

using cxxtrace::thread_id;
using cxxtrace::detail::processor_id;

namespace testing::internal {
extern bool g_help_flag;
auto
ParseGoogleTestFlagsOnly(int* argc, char** argv) -> void;
}

namespace cxxtrace_test {
namespace {
std::chrono::milliseconds g_test_duration = 20ms;
int g_thread_count = 4;

template<class ProcessorIDFunc>
auto
sample_processor_id_on_threads(ProcessorIDFunc get_current_processor_id)
  -> std::vector<processor_id_samples>;

auto
massage_processor_id_samples(
  const std::vector<processor_id_samples>& samples_by_thread)
  -> processor_id_samples;

auto
create_world_readable_temporary_file() -> std::string;

class any_processor_id_lookup
{
public:
  constexpr explicit any_processor_id_lookup(cxxtrace::czstring name) noexcept
    : name{ name }
  {}

  virtual ~any_processor_id_lookup() = default;

  virtual auto get_current_processor_id() const noexcept -> processor_id = 0;

  cxxtrace::czstring name;
};

class test_processor_id_manual
  : public testing::TestWithParam<any_processor_id_lookup*>
{};

TEST_P(test_processor_id_manual,
       current_processor_id_agrees_with_dtrace_scheduler)
{
  auto clock = dtrace_clock{};

  auto tracer = thread_schedule_tracer{ ::getpid() };
  auto dtrace = dtrace_client{ &tracer };
  dtrace.initialize();
  dtrace.start();

  auto begin_timestamp = clock.query();
  auto processor_id_samples_by_thread = sample_processor_id_on_threads(
    [this]() { return this->GetParam()->get_current_processor_id(); });
  auto end_timestamp = clock.query();

  dtrace.stop();

  auto thread_executions = tracer.get_thread_executions();

  {
    auto massaged_processor_id_samples =
      massage_processor_id_samples(processor_id_samples_by_thread);

    auto gnuplot_path = create_world_readable_temporary_file();
    auto gnuplot_stream = std::ofstream{};
    gnuplot_stream.exceptions(gnuplot_stream.badbit | gnuplot_stream.failbit);
    gnuplot_stream.open(gnuplot_path);

    std::fprintf(
      stderr, "Writing gnuplot script to file: %s\n", gnuplot_path.c_str());
    dump_gnuplot(gnuplot_stream,
                 thread_executions,
                 massaged_processor_id_samples,
                 begin_timestamp,
                 end_timestamp);
  }

  auto checker = sample_checker{ thread_executions };
  auto errors = 0;
  for (const auto& thread_samples : processor_id_samples_by_thread) {
    for (const auto& sample : thread_samples.samples) {
      if (errors > 10) {
        ADD_FAILURE() << "(giving up after too many errors)";
        return;
      }
      auto ok = checker.check_sample(sample);
      EXPECT_TRUE(ok);
      if (!ok) {
        errors += 1;
      }
    }
  }
}

template<class ProcessorIDLookup, int /*Counter*/>
struct type_erased_processor_id_lookup : public any_processor_id_lookup
{
  using any_processor_id_lookup::any_processor_id_lookup;

  virtual auto get_current_processor_id() const noexcept -> processor_id
  {
    return lookup.get_current_processor_id();
  }

  static auto instance(cxxtrace::czstring name)
    -> type_erased_processor_id_lookup&
  {
    static auto instance = type_erased_processor_id_lookup{ name };
    return instance;
  }

  ProcessorIDLookup lookup{};
};

#define PROCESSOR_ID_LOOKUP(lookup)                                            \
  (&type_erased_processor_id_lookup<::cxxtrace::detail::lookup,                \
                                    __COUNTER__>::instance(#lookup))
INSTANTIATE_TEST_CASE_P(
  ,
  test_processor_id_manual,
  testing::Values(
    PROCESSOR_ID_LOOKUP(processor_id_lookup),
    PROCESSOR_ID_LOOKUP(processor_id_lookup_x86_cpuid_01h),
    PROCESSOR_ID_LOOKUP(processor_id_lookup_x86_cpuid_0bh),
    PROCESSOR_ID_LOOKUP(processor_id_lookup_x86_cpuid_1fh),
    PROCESSOR_ID_LOOKUP(processor_id_lookup_x86_cpuid_commpage_preempt_cached)),
  [](const auto& param) { return param.param->name; });
#undef PROCESSOR_ID_LOOKUP

template<class ProcessorIDFunc>
auto
sample_processor_id_on_threads(ProcessorIDFunc get_current_processor_id)
  -> std::vector<processor_id_samples>
{
  // NOTE(strager): On my machine, a single thread can sample about 2.5 million
  // times in 100 milliseconds (i.e. 25k samples/ms). Reserve vectors large
  // enough that they don't resize during testing.
  auto estimated_sample_count_per_thread =
    40'000LL *
    std::chrono::duration_cast<std::chrono::milliseconds>(g_test_duration)
      .count();

  auto samples_by_thread = std::vector<processor_id_samples>{};
  samples_by_thread.resize(g_thread_count);
  for (auto& samples : samples_by_thread) {
    samples.samples.reserve(estimated_sample_count_per_thread);
  }

  auto stop = std::atomic<bool>{ false };
  auto thread_routine =
    [&get_current_processor_id, &samples_by_thread, &stop](int thread_index) {
      auto clock = dtrace_clock{};
      auto thread_id = cxxtrace::get_current_thread_id();

      auto samples = std::move(samples_by_thread[thread_index]);

      while (!stop.load()) {
        auto local_sample_count = 100;
        auto samples_size_before = samples.samples.size();
        samples.samples.resize(samples_size_before + local_sample_count);
        auto* local_samples = &samples.samples[samples_size_before];
        auto last_timestamp = clock.query();
        for (auto i = 0; i < local_sample_count; ++i) {
          auto& sample = local_samples[i];
          sample.timestamp_before = last_timestamp;
          sample.processor_id = get_current_processor_id();
          auto timestamp_after = clock.query();
          sample.timestamp_after = timestamp_after;
          sample.thread_id = thread_id;
          last_timestamp = timestamp_after;
        }
      }

      samples_by_thread[thread_index] = std::move(samples);
    };

  auto threads = std::vector<std::thread>{};
  for (auto thread_index = 0; thread_index < g_thread_count; ++thread_index) {
    threads.emplace_back(thread_routine, thread_index);
  }
  assert(samples_by_thread.size() == threads.size());

  std::this_thread::sleep_for(g_test_duration);
  stop.store(true);

  for (auto& thread : threads) {
    thread.join();
  }

  for (const auto& samples : samples_by_thread) {
    if (samples.samples.empty()) {
      std::fprintf(stderr, "Thread collected no samples\n");
    } else {
      auto sample_count = samples.samples.size();
      std::fprintf(
        stderr,
        "Thread %ju collected %zu samples (%.2f MiB; %.1f%% of estimate)\n",
        std::uintmax_t{ samples.samples.front().thread_id },
        sample_count,
        sample_count * sizeof(samples.samples[0]) / (1024.0 * 1024.0),
        100.0 * sample_count / estimated_sample_count_per_thread);
    }
  }
  return samples_by_thread;
}

auto
massage_processor_id_samples(
  const std::vector<processor_id_samples>& samples_by_thread)
  -> processor_id_samples
{
  // Prevent gnuplot from exploding by artificially limiting the number of
  // samples.
  auto max_samples = 100'000;
  auto max_samples_per_thread =
    std::max(std::size_t{ 1 }, max_samples / samples_by_thread.size());
  assert(max_samples_per_thread > 0);
  auto samples = processor_id_samples{};
  samples.samples.reserve(max_samples);
  for (const auto& thread_samples : samples_by_thread) {
    if (thread_samples.samples.empty()) {
      continue;
    }
    auto sample_rate =
      std::max(std::size_t{ 1 },
               (thread_samples.samples.size() + max_samples_per_thread - 1) /
                 max_samples_per_thread);
    assert(sample_rate > 0);
    for (auto i = std::size_t{ 0 }; i < thread_samples.samples.size() - 1;
         i += sample_rate) {
      samples.samples.push_back(thread_samples.samples[i]);
    }
    // Always include the last sample (in addition to the first sample).
    samples.samples.push_back(thread_samples.samples.back());
  }
  return samples;
}

auto
create_world_readable_temporary_file() -> std::string
{
  auto path = (std::filesystem::temp_directory_path() /
               "test_processor_id_manual_gnuplot_XXXXXXXX")
                .string();
  auto fd = ::mkstemp(path.data());
  if (fd == -1) {
    auto error = errno;
    throw std::filesystem::filesystem_error{
      "Failed to create temporary file",
      path,
      std::error_code{ error, std::generic_category() }
    };
  }
  EXPECT_EQ(0, ::fchmod(fd, 0644)) << std::strerror(errno);
  EXPECT_EQ(0, ::close(fd)) << std::strerror(errno);
  return path;
}

auto
show_usage(cxxtrace::czstring program_name) -> void
{
  std::printf(
    "usage: sudo %s [options]\n"
    "\n"
    "Test cxxtrace's get_current_processor_id implementation.\n"
    "\n"
    "options:\n"
    "  --test-duration=INTEGERms  how long to sample get_current_processor_id "
    "(default: %jdms)\n"
    "  --thead-count=INTEGER      how many threads which call "
    "get_current_processor_id (default: %d)\n",
    program_name,
    std::intmax_t{
      std::chrono::duration_cast<std::chrono::milliseconds>(g_test_duration)
        .count() },
    g_thread_count);

  if (!testing::internal::g_help_flag) {
    std::printf("\n");
    auto argc = 2;
    cxxtrace::czstring argv[3] = { program_name, "--gtest_help", nullptr };
    testing::internal::ParseGoogleTestFlagsOnly(&argc,
                                                const_cast<char**>(argv));
  }
}

auto
parse_arguments(int argc, char** argv) -> std::vector<char*>
{
  static constexpr struct option options[] = {
    { "help", no_argument, nullptr, 'h' },
    { "test-duration", required_argument, nullptr, 'd' },
    { "thread-count", required_argument, nullptr, 'c' },
    {},
  };
  static constexpr char short_options[] = ":c:d:h";

  ::opterr = 0;
  ::optind = 1;
  auto unparsed_arguments = std::vector<char*>{ argv[0] };
  for (;;) {
    auto option =
      ::getopt_long_only(argc, argv, short_options, options, nullptr);
    if (option == -1) {
      break;
    }
    switch (option) {
      case 'c': { // --thread-count
        auto parsed_thread_count = int{};
        if (::optarg[0] == '\0') {
          std::fprintf(stderr,
                       "error: expected positive integer for --thread-count\n");
          std::exit(1);
        }
        auto result = std::from_chars(
          ::optarg, &::optarg[std::strlen(::optarg)], parsed_thread_count);
        if (*result.ptr != '\0' || result.ec != std::errc{} ||
            parsed_thread_count <= 0) {
          std::fprintf(
            stderr,
            "error: expected positive integer for --thread-count: %s\n",
            ::optarg);
          std::exit(1);
        }
        g_thread_count = parsed_thread_count;
        break;
      }

      case 'd': { // --test-duration
        auto parsed_test_duration = int{};
        if (::optarg[0] == '\0') {
          std::fprintf(
            stderr, "error: expected positive integer for --test-duration\n");
          std::exit(1);
        }
        auto result = std::from_chars(
          ::optarg, &::optarg[std::strlen(::optarg)], parsed_test_duration);
        if (result.ec != std::errc{} || parsed_test_duration <= 0) {
          std::fprintf(
            stderr,
            "error: expected positive integer for --test-duration: %s\n",
            ::optarg);
          std::exit(1);
        }
        if (result.ptr != "ms"sv) {
          std::fprintf(stderr,
                       "error: expected 'ms' suffix for --test-duration: %s\n",
                       ::optarg);
          std::exit(1);
        }
        g_test_duration = std::chrono::milliseconds{ parsed_test_duration };
        break;
      }

      case 'h': // --help
        show_usage(argv[0]);
        std::exit(0);

      case ':': {
        for (const auto& option : options) {
          if (option.name && !option.flag && option.val == ::optopt) {
            std::fprintf(
              stderr, "error: expected argument for --%s\n", option.name);
            std::exit(1);
          }
        }
        assert(false);
        std::fprintf(stderr, "error: expected argument\n");
        std::exit(1);
      }

      case '?':
        unparsed_arguments.push_back(argv[::optind - 1]);
        break;
    }
  }
  return unparsed_arguments;
}
}
}

auto
main(int argc, char** argv) -> int
{
  auto unparsed_arguments = cxxtrace_test::parse_arguments(argc, argv);
  auto unparsed_argc = static_cast<int>(unparsed_arguments.size());
  testing::InitGoogleTest(&unparsed_argc, unparsed_arguments.data());
  if (unparsed_argc != 1) {
    std::fprintf(stderr, "error: unknown option: %s\n", unparsed_arguments[1]);
    if (testing::internal::g_help_flag) {
      // A --gtest_ option was given but not recognized. gtest printed its usage
      // information already. Reduce noise; don't show our usage information.
    } else {
      // Neither we nor gtest recognized an option. gtest did not print its
      // usage information already.
      cxxtrace_test::show_usage(argv[0]);
    }
    return 1;
  }
  return RUN_ALL_TESTS();
}
