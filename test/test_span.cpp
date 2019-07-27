#include "stringify.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cxxtrace/clock.h>
#include <cxxtrace/config.h>
#include <cxxtrace/ring_queue_storage.h>
#include <cxxtrace/ring_queue_thread_local_storage.h>
#include <cxxtrace/ring_queue_unsafe_storage.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/span.h>
#include <cxxtrace/spsc_ring_queue_thread_local_storage.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <cxxtrace/unbounded_storage.h>
#include <cxxtrace/unbounded_unsafe_storage.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>
#include <random>
#include <string_view>
#include <thread>

#if defined(__clang__)
// Work around false positives in Clang's -Wunused-local-typedef diagnostics.
// Bug report: https://bugs.llvm.org/show_bug.cgi?id=24883
#define CXXTRACE_WORK_AROUND_CLANG_24883 1
#endif

#define CXXTRACE_SPAN(category, name)                                          \
  CXXTRACE_SPAN_WITH_CONFIG(this->get_cxxtrace_config(), category, name)

using namespace std::literals::string_view_literals;
using cxxtrace::stringify;
using testing::AnyOf;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

namespace {
template<class T>
auto
do_not_optimize_away(const T&) noexcept -> void;

class event
{
public:
  auto set() -> void;
  auto wait() -> void;

private:
  std::mutex mutex{};
  std::condition_variable cond_var{};
  bool value{ false };
};

struct ring_queue_thread_local_test_storage_tag
{};
template<std::size_t CapacityPerThread, class ClockSample>
using ring_queue_thread_local_test_storage =
  cxxtrace::ring_queue_thread_local_storage<
    CapacityPerThread,
    ring_queue_thread_local_test_storage_tag,
    ClockSample>;

struct spsc_ring_queue_thread_local_test_storage_tag
{};
template<std::size_t CapacityPerThread, class ClockSample>
using spsc_ring_queue_thread_local_test_storage =
  cxxtrace::spsc_ring_queue_thread_local_storage<
    CapacityPerThread,
    spsc_ring_queue_thread_local_test_storage_tag,
    ClockSample>;

using clock = cxxtrace::fake_clock;
using clock_sample = clock::sample;

template<class Storage>
class test_span : public testing::Test
{
public:
  explicit test_span()
    : cxxtrace_config{ cxxtrace_storage, clock_ }
  {}

  auto TearDown() -> void override { this->reset_storage(); }

protected:
  using clock_type = clock;
  using storage_type = Storage;

  auto get_cxxtrace_config() noexcept
    -> cxxtrace::basic_config<storage_type, clock_type>&
  {
    return this->cxxtrace_config;
  }

  auto take_all_samples() -> cxxtrace::samples_snapshot
  {
    return this->cxxtrace_storage.take_all_samples(this->clock());
  }

  auto reset_storage() -> void { this->cxxtrace_storage.reset(); }

  auto clock() noexcept -> clock_type& { return this->clock_; }

private:
  clock_type clock_{};
  storage_type cxxtrace_storage{};
  cxxtrace::basic_config<storage_type, clock_type> cxxtrace_config;
};

using test_span_types = ::testing::Types<
  cxxtrace::ring_queue_storage<1024, clock_sample>,
  cxxtrace::ring_queue_unsafe_storage<1024, clock_sample>,
  cxxtrace::unbounded_storage<clock_sample>,
  cxxtrace::unbounded_unsafe_storage<clock_sample>,
  ring_queue_thread_local_test_storage<1024, clock_sample>,
  spsc_ring_queue_thread_local_test_storage<1024, clock_sample>>;
TYPED_TEST_CASE(test_span, test_span_types, );

template<class Storage>
class test_span_thread_safe : public test_span<Storage>
{};

using test_span_thread_safe_types = ::testing::Types<
  cxxtrace::ring_queue_storage<1024, clock_sample>,
  cxxtrace::unbounded_storage<clock_sample>,
  ring_queue_thread_local_test_storage<1024, clock_sample>,
  spsc_ring_queue_thread_local_test_storage<1024, clock_sample>>;
TYPED_TEST_CASE(test_span_thread_safe, test_span_thread_safe_types, );

TYPED_TEST(test_span, no_samples_exist_by_default)
{
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 0);
}

TYPED_TEST(test_span, span_adds_sample_at_scope_enter)
{
  auto span = CXXTRACE_SPAN("span category", "span name");
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 1);
  auto sample = cxxtrace::sample_ref{ samples.at(0) };
  EXPECT_STREQ(sample.category(), "span category");
  EXPECT_STREQ(sample.name(), "span name");
  EXPECT_EQ(sample.kind(), cxxtrace::sample_kind::enter_span);
}

TYPED_TEST(test_span, span_adds_sample_at_scope_exit)
{
  {
    auto span = CXXTRACE_SPAN("span category", "span name");
  }
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 2);
  auto sample = cxxtrace::sample_ref{ samples.at(samples.size() - 1) };
  EXPECT_STREQ(sample.category(), "span category");
  EXPECT_STREQ(sample.name(), "span name");
  EXPECT_EQ(sample.kind(), cxxtrace::sample_kind::exit_span);
}

TYPED_TEST(test_span, enter_samples_for_nested_spans_are_ordered)
{
  auto span_1 = CXXTRACE_SPAN("span category", "span name 1");
  auto span_2 = CXXTRACE_SPAN("span category", "span name 2");
  auto span_3 = CXXTRACE_SPAN("span category", "span name 3");
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 3);
  auto sample_1 = cxxtrace::sample_ref{ samples.at(0) };
  EXPECT_STREQ(sample_1.name(), "span name 1");
  auto sample_2 = cxxtrace::sample_ref{ samples.at(1) };
  EXPECT_STREQ(sample_2.name(), "span name 2");
  auto sample_3 = cxxtrace::sample_ref{ samples.at(2) };
  EXPECT_STREQ(sample_3.name(), "span name 3");
}

TYPED_TEST(test_span, exit_samples_for_nested_spans_are_ordered_and_adjacent)
{
  {
    auto span_1 = CXXTRACE_SPAN("span category", "span name 1");
    auto span_2 = CXXTRACE_SPAN("span category", "span name 2");
    auto span_3 = CXXTRACE_SPAN("span category", "span name 3");
  }
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 6);
  EXPECT_STREQ(samples.at(samples.size() - 3).name(), "span name 3");
  EXPECT_STREQ(samples.at(samples.size() - 2).name(), "span name 2");
  EXPECT_STREQ(samples.at(samples.size() - 1).name(), "span name 1");
}

TYPED_TEST(test_span,
           samples_for_incomplete_span_appears_after_sibling_complete_span)
{
  {
    auto complete_span = CXXTRACE_SPAN("span category", "complete span");
  }
  auto incomplete_span = CXXTRACE_SPAN("span category", "incomplete span");
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 3);
  EXPECT_STREQ(samples.at(0).name(), "complete span");
  EXPECT_STREQ(samples.at(1).name(), "complete span");
  EXPECT_STREQ(samples.at(2).name(), "incomplete span");
}

TYPED_TEST(test_span,
           sample_for_complete_span_appears_after_parent_incomplete_span)
{
  auto incomplete_span = CXXTRACE_SPAN("span category", "incomplete span");
  {
    auto complete_span = CXXTRACE_SPAN("span category", "complete span");
  }
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 3);
  EXPECT_STREQ(samples.at(0).name(), "incomplete span");
  EXPECT_STREQ(samples.at(1).name(), "complete span");
  EXPECT_STREQ(samples.at(2).name(), "complete span");
}

TYPED_TEST(test_span, later_snapshot_includes_only_new_spans)
{
  {
    auto span_before = CXXTRACE_SPAN("category", "before");
  }
  auto samples_1 = cxxtrace::samples_snapshot{ this->take_all_samples() };
  {
    auto span_after = CXXTRACE_SPAN("category", "after");
  }
  auto samples_2 = cxxtrace::samples_snapshot{ this->take_all_samples() };

  EXPECT_EQ(samples_1.size(), 2);
  EXPECT_STREQ(samples_1.at(0).name(), "before");
  EXPECT_STREQ(samples_1.at(1).name(), "before");
  EXPECT_EQ(samples_2.size(), 2);
  EXPECT_STREQ(samples_2.at(0).name(), "after");
  EXPECT_STREQ(samples_2.at(1).name(), "after");
}

TYPED_TEST(test_span, later_snapshot_includes_exit_sample_of_completed_span)
{
  auto samples_1 = [&] {
    auto span = CXXTRACE_SPAN("category", "test span");
    return cxxtrace::samples_snapshot{ this->take_all_samples() };
  }();
  auto samples_2 = cxxtrace::samples_snapshot{ this->take_all_samples() };

  EXPECT_EQ(samples_1.size(), 1);
  EXPECT_STREQ(samples_1.at(0).name(), "test span");
  EXPECT_EQ(samples_1.at(0).kind(), cxxtrace::sample_kind::enter_span);
  EXPECT_EQ(samples_2.size(), 1);
  EXPECT_STREQ(samples_2.at(0).name(), "test span");
  EXPECT_EQ(samples_2.at(0).kind(), cxxtrace::sample_kind::exit_span);
}

TYPED_TEST(test_span, span_samples_include_thread_id)
{
  {
    auto main_span = CXXTRACE_SPAN("category", "main span");
  }
  auto main_thread_id = cxxtrace::get_current_thread_id();
  SCOPED_TRACE(stringify("main_thread_id: ", main_thread_id));

  auto thread_1_id = std::atomic<cxxtrace::thread_id>{};
  std::thread{ [&] {
    auto thread_1_span = CXXTRACE_SPAN("category", "thread 1 span");
    thread_1_id = cxxtrace::get_current_thread_id();
  } }
    .join();
  SCOPED_TRACE(stringify("thread_1_id: ", thread_1_id));

  auto thread_2_id = std::atomic<cxxtrace::thread_id>{};
  std::thread{ [&] {
    auto thread_2_span = CXXTRACE_SPAN("category", "thread 2 span");
    thread_2_id = cxxtrace::get_current_thread_id();
  } }
    .join();
  SCOPED_TRACE(stringify("thread_2_id: ", thread_2_id));

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

TYPED_TEST(test_span, span_samples_include_timestamps)
{
  auto& clock = this->clock();
  auto timestamp_before_span = clock.make_time_point(clock.query());
  auto timestamp_inside_span = [&] {
    auto span = CXXTRACE_SPAN("category", "span");
    return clock.make_time_point(clock.query());
  }();
  auto timestamp_after_span = clock.make_time_point(clock.query());

  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  auto span_begin_timestamp = samples.at(0).timestamp();
  auto span_end_timestamp = samples.at(1).timestamp();
  EXPECT_LE(timestamp_before_span, span_begin_timestamp);
  EXPECT_LE(span_begin_timestamp, timestamp_inside_span);
  EXPECT_LE(timestamp_inside_span, span_end_timestamp);
  EXPECT_LE(span_end_timestamp, timestamp_after_span);
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

TYPED_TEST(test_span_thread_safe,
           span_enter_and_exit_synchronize_across_threads)
{
  using clock_type = typename TestFixture::clock_type;
  using storage_type = typename TestFixture::storage_type;

  // NOTE(strager): This test relies on dynamic analysis tools or crashes (due
  // to undefined behavior) to detect data races.

  static constexpr const auto max_work_size = std::size_t{ 10000 };
  static const auto work_iteration_count = 400;
  static const auto thread_count = 4;

  struct workload
  {
#if CXXTRACE_WORK_AROUND_CLANG_24883
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
    using item_type = int;
#if CXXTRACE_WORK_AROUND_CLANG_24883
#pragma clang diagnostic pop
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

template<class T>
auto
do_not_optimize_away(const T&) noexcept -> void
{
  // TODO
}

auto
event::set() -> void
{
  auto lock = std::lock_guard{ this->mutex };
  this->value = true;
  this->cond_var.notify_all();
}

auto
event::wait() -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->cond_var.wait(lock, [&] { return this->value; });
}
}
