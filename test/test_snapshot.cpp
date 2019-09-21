#include "event.h"
#include "stringify.h"
#include "test_span.h"
#include "thread.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/span.h>
#include <cxxtrace/thread.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>
#include <pthread.h>
#include <string>
#include <thread>
#include <vector>

#define CXXTRACE_SAMPLE() CXXTRACE_SAMPLE_WITH_NAME("name")

#define CXXTRACE_SAMPLE_WITH_NAME(name)                                        \
  {                                                                            \
    auto span = CXXTRACE_SPAN_WITH_CONFIG(                                     \
      this->get_cxxtrace_config(), "category", name);                          \
  }

using namespace std::chrono_literals;
using testing::UnorderedElementsAre;

namespace cxxtrace_test {
template<class Storage>
class test_snapshot : public test_span<Storage>
{};

using test_snapshot_types = ::testing::Types<
  cxxtrace::mpsc_ring_queue_storage<1024, clock_sample>,
  cxxtrace::ring_queue_storage<1024, clock_sample>,
  cxxtrace::unbounded_storage<clock_sample>,
  mpsc_ring_queue_processor_local_test_storage<1024, clock_sample>,
  ring_queue_thread_local_test_storage<1024, clock_sample>,
  spsc_ring_queue_processor_local_test_storage<1024, clock_sample>,
  spsc_ring_queue_thread_local_test_storage<1024, clock_sample>>;
TYPED_TEST_CASE(test_snapshot, test_snapshot_types, );

TYPED_TEST(test_snapshot, name_of_live_threads)
{
  auto test_done = event{};

  auto thread_1_id = cxxtrace::thread_id{};
  auto thread_1_ready = event{};
  auto thread_1 = std::thread{ [&] {
    set_current_thread_name("thread 1 name");

    CXXTRACE_SAMPLE();
    thread_1_id = cxxtrace::get_current_thread_id();
    thread_1_ready.set();
    test_done.wait();
  } };

  auto thread_2_id = cxxtrace::thread_id{};
  auto thread_2_ready = event{};
  auto thread_2 = std::thread{ [&] {
    set_current_thread_name("thread 2 name");

    CXXTRACE_SAMPLE();
    thread_2_id = cxxtrace::get_current_thread_id();
    thread_2_ready.set();
    test_done.wait();
  } };

  thread_1_ready.wait();
  thread_2_ready.wait();

  SCOPED_TRACE(stringify("thread_1_id: ", thread_1_id));
  SCOPED_TRACE(stringify("thread_2_id: ", thread_2_id));

  auto snapshot = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_STREQ(snapshot.thread_name(thread_1_id), "thread 1 name");
  EXPECT_STREQ(snapshot.thread_name(thread_2_id), "thread 2 name");

  test_done.set();
  thread_1.join();
  thread_2.join();
}

TYPED_TEST(test_snapshot, name_of_dead_threads)
{
  auto thread_1_id = cxxtrace::thread_id{};
  auto thread_1 = std::thread{ [&] {
    set_current_thread_name("thread 1 name");

    thread_1_id = cxxtrace::get_current_thread_id();
    CXXTRACE_SAMPLE();

    cxxtrace::remember_current_thread_name_for_next_snapshot(
      this->get_cxxtrace_config());
  } };

  auto thread_2_id = cxxtrace::thread_id{};
  auto thread_2 = std::thread{ [&] {
    set_current_thread_name("thread 2 name");

    thread_2_id = cxxtrace::get_current_thread_id();
    CXXTRACE_SAMPLE();

    cxxtrace::remember_current_thread_name_for_next_snapshot(
      this->get_cxxtrace_config());
  } };

  thread_1.join();
  thread_2.join();

  SCOPED_TRACE(stringify("thread_1_id: ", thread_1_id));
  SCOPED_TRACE(stringify("thread_2_id: ", thread_2_id));

  auto snapshot = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_STREQ(snapshot.thread_name(thread_1_id), "thread 1 name");
  EXPECT_STREQ(snapshot.thread_name(thread_2_id), "thread 2 name");
}

TYPED_TEST(test_snapshot, name_of_live_thread_ignores_remembering)
{
  // This test documents an odd, possibly confusing behavior of
  // remember_current_thread_name_for_next_snapshot. See
  // remember_current_thread_name_for_next_snapshot's documentation for more
  // details.

  auto test_done = event{};

  auto thread_id = cxxtrace::thread_id{};
  auto thread_ready = event{};
  auto thread = std::thread{ [&] {
    CXXTRACE_SAMPLE();

    set_current_thread_name("before remember");
    cxxtrace::remember_current_thread_name_for_next_snapshot(
      this->get_cxxtrace_config());
    set_current_thread_name("after remember");

    thread_id = cxxtrace::get_current_thread_id();
    thread_ready.set();
    test_done.wait();
  } };
  thread_ready.wait();

  auto snapshot = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_STREQ(snapshot.thread_name(thread_id), "after remember");

  test_done.set();
  thread.join();
}

TYPED_TEST(test_snapshot, take_all_samples_is_thread_safe)
{
  // NOTE(strager): This test relies on dynamic analysis tools or crashes (due
  // to undefined behavior) to detect data races.

  static const auto iterations_between_done_checks = 100;
  static const auto thread_count = 4;

  auto snapshots = std::vector<cxxtrace::samples_snapshot>{};
  auto snapshots_mutex = std::mutex{};

  auto done = std::atomic<bool>{ false };

  auto thread_routine =
    [ this, &done, &snapshots, &snapshots_mutex ]() noexcept->void
  {
    auto check_samples = [&] {
      auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
      if (!samples.empty()) {
        auto guard = std::lock_guard<std::mutex>{ snapshots_mutex };
        snapshots.emplace_back(samples);
      }
    };

    for (;;) {
      for (auto i = 0; i < iterations_between_done_checks; ++i) {
        check_samples();
      }
      // We want to avoid artifically synchronizing take_all_samples.
      // (take_all_samples should perform its own synchronization.) Call
      // done.load infrequently to reduce the chance of artifically
      // synchronizing take_all_samples.
      if (done.load()) {
        check_samples();
        return;
      }
    }
  };

  auto threads = std::vector<std::thread>{};
  for (auto i = 0; i < thread_count; ++i) {
    threads.emplace_back(thread_routine);
  }

  CXXTRACE_SAMPLE_WITH_NAME("span 1");
  std::this_thread::sleep_for(10ms);
  CXXTRACE_SAMPLE_WITH_NAME("span 2");
  done.store(true);

  for (auto& thread : threads) {
    thread.join();
  }

  auto sample_names = std::vector<std::string>{};
  for (auto& snapshot : snapshots) {
    for (auto i = cxxtrace::samples_snapshot::size_type{ 0 };
         i < snapshot.size();
         ++i) {
      sample_names.emplace_back(snapshot.at(i).name());
    }
  }
  EXPECT_THAT(sample_names,
              UnorderedElementsAre("span 1", "span 1", "span 2", "span 2"));
}
}
