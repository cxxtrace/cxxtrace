#include "event.h"
#include "gtest_scoped_trace.h"
#include "test_span.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cxxtrace/config.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/span.h>
#include <cxxtrace/thread.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>
#include <random>
#include <string_view>
#include <thread>
#include <vector>

#if defined(__clang__)
// Work around false positives in Clang's -Wunused-local-typedef diagnostics.
// Bug report: https://bugs.llvm.org/show_bug.cgi?id=24883
#define CXXTRACE_WORK_AROUND_CLANG_24883 1
#endif

#if CXXTRACE_WORK_AROUND_CLANG_24883
#include <cxxtrace/detail/warning.h>
#endif

#define CXXTRACE_SPAN(category, name)                                          \
  CXXTRACE_SPAN_WITH_CONFIG(this->get_cxxtrace_config(), category, name)

using testing::AnyOf;
using testing::ElementsAre;

namespace {
template<class T>
auto
do_not_optimize_away(const T&) noexcept -> void;
}

namespace cxxtrace_test {
TYPED_TEST(test_span, span_samples_include_thread_id)
{
  {
    auto main_span = CXXTRACE_SPAN("category", "main span");
  }
  auto main_thread_id = cxxtrace::get_current_thread_id();
  CXXTRACE_SCOPED_TRACE() << "main_thread_id: " << main_thread_id;

  auto thread_1_id = std::atomic<cxxtrace::thread_id>{};
  std::thread{
    [&] {
      auto thread_1_span = CXXTRACE_SPAN("category", "thread 1 span");
      thread_1_id = cxxtrace::get_current_thread_id();
    }
  }.join();
  CXXTRACE_SCOPED_TRACE() << "thread_1_id: " << thread_1_id;

  auto thread_2_id = std::atomic<cxxtrace::thread_id>{};
  std::thread{
    [&] {
      auto thread_2_span = CXXTRACE_SPAN("category", "thread 2 span");
      thread_2_id = cxxtrace::get_current_thread_id();
    }
  }.join();
  CXXTRACE_SCOPED_TRACE() << "thread_2_id: " << thread_2_id;

  ASSERT_NE(thread_1_id, main_thread_id);
  ASSERT_NE(thread_2_id, main_thread_id);
  ASSERT_NE(thread_1_id, thread_2_id);

  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  auto get_sample = [&samples](std::string_view name) -> cxxtrace::sample_ref {
    for (auto i = cxxtrace::samples_snapshot::size_type{ 0 };
         i < samples.size();
         ++i) {
      auto sample = cxxtrace::sample_ref{ samples.at(i) };
      if (sample.name() == name) {
        return sample;
      }
    }
    throw std::out_of_range("Could not find sample");
  };
  EXPECT_EQ(get_sample("main span").thread_id(), main_thread_id);
  EXPECT_EQ(get_sample("thread 1 span").thread_id(), thread_1_id);
  EXPECT_EQ(get_sample("thread 2 span").thread_id(), thread_2_id);
}

TYPED_TEST(test_span, span_samples_can_interleave_using_multiple_threads)
{
  {
    auto mutex = std::mutex{};
    auto cond_var = std::condition_variable{};
    auto thread_should_exit_span = false;

    auto main_span = CXXTRACE_SPAN("category", "main span");
    auto thread = std::thread{ [&] {
      auto thread_1_span = CXXTRACE_SPAN("category", "thread span");
      {
        auto lock = std::unique_lock{ mutex };
        cond_var.wait(lock, [&] { return thread_should_exit_span; });
      }
    } };
    {
      auto lock = std::lock_guard{ mutex };
      thread_should_exit_span = true;
      cond_var.notify_all();
    }
    thread.join();
  }

  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };

  using name_and_kind = std::pair<std::string, cxxtrace::sample_kind>;
  auto names_and_kinds = std::vector<name_and_kind>{};
  for (auto i = cxxtrace::samples_snapshot::size_type{ 0 }; i < samples.size();
       ++i) {
    auto sample = cxxtrace::sample_ref{ samples.at(i) };
    names_and_kinds.emplace_back(sample.name(), sample.kind());
  }
  auto enter = [](cxxtrace::czstring name) {
    return name_and_kind{ name, cxxtrace::sample_kind::enter_span };
  };
  auto exit = [](cxxtrace::czstring name) {
    return name_and_kind{ name, cxxtrace::sample_kind::exit_span };
  };
  EXPECT_THAT(names_and_kinds,
              AnyOf(ElementsAre(enter("main span"),
                                exit("main span"),
                                enter("thread span"),
                                exit("thread span")),
                    ElementsAre(enter("main span"),
                                enter("thread span"),
                                exit("main span"),
                                exit("thread span")),
                    ElementsAre(enter("main span"),
                                enter("thread span"),
                                exit("thread span"),
                                exit("main span")),
                    ElementsAre(enter("thread span"),
                                exit("thread span"),
                                enter("main span"),
                                exit("main span")),
                    ElementsAre(enter("thread span"),
                                enter("main span"),
                                exit("thread span"),
                                exit("main span")),
                    ElementsAre(enter("thread span"),
                                enter("main span"),
                                exit("main span"),
                                exit("thread span"))));
}

TYPED_TEST(test_span, snapshot_includes_spans_from_other_running_threads)
{
  auto thread_did_exit_span = event{};
  auto thread_should_exit = event{};

  auto thread_id = std::atomic<cxxtrace::thread_id>{};
  auto thread = std::thread{ [&] {
    {
      auto thread_span = CXXTRACE_SPAN("category", "thread span");
      thread_id = cxxtrace::get_current_thread_id();
    }
    auto incomplete_thread_span =
      CXXTRACE_SPAN("category", "incomplete thread span");
    thread_did_exit_span.set();
    thread_should_exit.wait();
  } };
  thread_did_exit_span.wait();

  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 3);
  EXPECT_STREQ(samples.at(0).name(), "thread span");
  EXPECT_STREQ(samples.at(1).name(), "thread span");
  EXPECT_STREQ(samples.at(2).name(), "incomplete thread span");
  EXPECT_EQ(samples.at(0).thread_id(), thread_id);
  EXPECT_EQ(samples.at(1).thread_id(), thread_id);
  EXPECT_EQ(samples.at(2).thread_id(), thread_id);

  thread_should_exit.set();
  thread.join();
}

TYPED_TEST(test_span,
           resetting_storage_removes_samples_by_other_running_threads)
{
  auto thread_did_exit_span = event{};
  auto thread_should_exit = event{};

  auto thread = std::thread{ [&] {
    {
      auto thread_span = CXXTRACE_SPAN("category", "thread span");
    }
    thread_did_exit_span.set();
    thread_should_exit.wait();
  } };
  thread_did_exit_span.wait();
  this->reset_storage();

  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 0);

  thread_should_exit.set();
  thread.join();
}

TYPED_TEST(test_span, resetting_storage_removes_samples_by_exited_threads)
{
  auto thread = std::thread{ [&] {
    auto thread_span = CXXTRACE_SPAN("category", "thread span");
  } };
  thread.join();
  this->reset_storage();

  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 0);
}

TYPED_TEST(test_span, taking_snapshot_removes_samples_by_exited_threads)
{
  auto thread = std::thread{ [&] {
    auto thread_span = CXXTRACE_SPAN("category", "thread span");
  } };
  thread.join();
  auto samples_1 = cxxtrace::samples_snapshot{ this->take_all_samples() };
  auto samples_2 = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples_2.size(), 0);
}

namespace {
constexpr const auto max_work_size = std::size_t{ 10000 };
}

TYPED_TEST(test_span_thread_safe,
           span_enter_and_exit_synchronize_across_threads)
{
  using clock_type = typename TestFixture::clock_type;
  using storage_type = typename TestFixture::storage_type;

  // NOTE(strager): This test relies on dynamic analysis tools or crashes (due
  // to undefined behavior) to detect data races.

  static const auto work_iteration_count = 400;
  static const auto thread_count = 4;

  struct workload
  {
#if CXXTRACE_WORK_AROUND_CLANG_24883
    CXXTRACE_WARNING_PUSH
    CXXTRACE_WARNING_IGNORE_CLANG("-Wunused-local-typedef")
#endif
    using item_type = int;
#if CXXTRACE_WORK_AROUND_CLANG_24883
    CXXTRACE_WARNING_POP
#endif

    auto do_work() noexcept -> void
    {
      auto span = CXXTRACE_SPAN("workload", "do work");

      auto data = std::array<item_type, max_work_size>{};
      auto size = this->size_distribution(this->rng);
      assert(size <= data.size());
      std::generate_n(data.begin(), size, [this] {
        return this->item_distribution(this->rng);
      });
      std::sort(data.begin(), data.end());
      do_not_optimize_away(data);
    }

    auto get_cxxtrace_config() noexcept
      -> cxxtrace::basic_config<storage_type, clock_type>&
    {
      return this->cxxtrace_config;
    }

    cxxtrace::basic_config<storage_type, clock_type>& cxxtrace_config;

    std::mt19937 rng{};
    std::uniform_int_distribution<std::size_t> size_distribution{
      0,
      max_work_size
    };
    std::uniform_int_distribution<item_type> item_distribution{ 0, 10 };
  };

  auto thread_routine = [this]() noexcept->void
  {
    auto thread_span = CXXTRACE_SPAN("workload", "thread");
    auto work = workload{ this->get_cxxtrace_config() };
    for (auto i = 0; i < work_iteration_count; ++i) {
      work.do_work();
    }
  };

  auto threads = std::vector<std::thread>{};
  for (auto i = 0; i < thread_count; ++i) {
    threads.emplace_back(thread_routine);
  }
  for (auto& thread : threads) {
    thread.join();
  }
}
}

namespace {
template<class T>
auto
do_not_optimize_away(const T&) noexcept -> void
{
  // TODO
}
}
