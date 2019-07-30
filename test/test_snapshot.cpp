#include "event.h"
#include "stringify.h"
#include "test_span.h"
#include <cstring>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/span.h>
#include <cxxtrace/thread.h>
#include <pthread.h>
#include <thread>

#define CXXTRACE_SAMPLE()                                                      \
  {                                                                            \
    auto span = CXXTRACE_SPAN_WITH_CONFIG(                                     \
      this->get_cxxtrace_config(), "category", "name");                        \
  }

using cxxtrace::stringify;

namespace cxxtrace_test {
template<class Storage>
class test_snapshot : public test_span<Storage>
{};

using test_snapshot_types = ::testing::Types<
  cxxtrace::ring_queue_storage<1024, clock_sample>,
  cxxtrace::unbounded_storage<clock_sample>,
  ring_queue_thread_local_test_storage<1024, clock_sample>,
  spsc_ring_queue_thread_local_test_storage<1024, clock_sample>>;
TYPED_TEST_CASE(test_snapshot, test_snapshot_types, );

TYPED_TEST(test_snapshot, name_of_live_threads)
{
  auto test_done = event{};

  auto thread_1_id = cxxtrace::thread_id{};
  auto thread_1_ready = event{};
  auto thread_1 = std::thread{ [&] {
    auto rc = ::pthread_setname_np("thread 1 name");
    ASSERT_EQ(rc, 0) << std::strerror(rc);

    CXXTRACE_SAMPLE();
    thread_1_id = cxxtrace::get_current_thread_id();
    thread_1_ready.set();
    test_done.wait();
  } };

  auto thread_2_id = cxxtrace::thread_id{};
  auto thread_2_ready = event{};
  auto thread_2 = std::thread{ [&] {
    auto rc = ::pthread_setname_np("thread 2 name");
    ASSERT_EQ(rc, 0) << std::strerror(rc);

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
    auto rc = ::pthread_setname_np("thread 1 name");
    ASSERT_EQ(rc, 0) << std::strerror(rc);

    thread_1_id = cxxtrace::get_current_thread_id();
    CXXTRACE_SAMPLE();

    cxxtrace::remember_current_thread_name_for_next_snapshot(
      this->get_cxxtrace_config());
  } };

  auto thread_2_id = cxxtrace::thread_id{};
  auto thread_2 = std::thread{ [&] {
    auto rc = ::pthread_setname_np("thread 2 name");
    ASSERT_EQ(rc, 0) << std::strerror(rc);

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
    auto rc = int{};

    CXXTRACE_SAMPLE();

    rc = ::pthread_setname_np("thread name before remember");
    ASSERT_EQ(rc, 0) << std::strerror(rc);
    cxxtrace::remember_current_thread_name_for_next_snapshot(
      this->get_cxxtrace_config());
    rc = ::pthread_setname_np("thread name after remember");
    ASSERT_EQ(rc, 0) << std::strerror(rc);

    thread_id = cxxtrace::get_current_thread_id();
    thread_ready.set();
    test_done.wait();
  } };
  thread_ready.wait();

  auto snapshot = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_STREQ(snapshot.thread_name(thread_id), "thread name after remember");

  test_done.set();
  thread.join();
}
}