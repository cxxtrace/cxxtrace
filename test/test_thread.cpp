#include "event.h"
#include <chrono>
#include <cstring>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/kern_return.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <pthread.h>
#include <stdexcept>
#endif

namespace {
using namespace std::literals::chrono_literals;

#if defined(__APPLE__) && defined(__MACH__)
template<class Func>
auto
poll_for(std::chrono::milliseconds timeout, Func&& predicate) -> void;
template<class Clock, class Func>
auto
poll_until(std::chrono::time_point<Clock> deadline, Func&& predicate) -> void;

auto
get_mach_port_type(mach_port_t port) -> mach_port_type_t;
#endif

#if defined(__APPLE__) && defined(__MACH__)
TEST(test_thread_pthread_thread_id, current_thread_id_matches_mach_thread_id)
{
  EXPECT_EQ(cxxtrace::get_current_thread_pthread_thread_id(),
            cxxtrace::get_current_thread_mach_thread_id());
  std::thread{ [] {
    EXPECT_EQ(cxxtrace::get_current_thread_pthread_thread_id(),
              cxxtrace::get_current_thread_mach_thread_id());
  } }
    .join();
}
#endif

#if defined(__APPLE__) && defined(__MACH__)
TEST(test_thread_mach_thread_id,
     getting_current_thread_id_does_not_leak_thread_port)
{
  auto thread_port = mach_port_t{};
  std::thread{ [&thread_port] {
    thread_port = mach_thread_self();
    auto type = get_mach_port_type(thread_port);
    EXPECT_TRUE(type & MACH_PORT_TYPE_SEND);
    EXPECT_FALSE(type & MACH_PORT_TYPE_DEAD_NAME);

    cxxtrace::get_current_thread_mach_thread_id();
  } }
    .join();

  auto thread_port_is_dead = [&thread_port]() -> bool {
    auto type = get_mach_port_type(thread_port);
    return type & MACH_PORT_TYPE_DEAD_NAME;
  };
  poll_for(500ms, thread_port_is_dead);
  EXPECT_TRUE(thread_port_is_dead());
}
#endif

TEST(test_thread_name, default_thread_name_is_empty)
{
  std::thread{ [] {
    auto thread_names = cxxtrace::detail::thread_name_set{};
    thread_names.fetch_and_remember_name_of_current_thread();
    EXPECT_STREQ(
      thread_names.name_of_thread_by_id(cxxtrace::get_current_thread_id()), "");
  } }
    .join();
}

#if defined(__APPLE__) && defined(__MACH__)
TEST(test_thread_name, current_thread_name_matches_pthread_setname_np)
{
  std::thread{ [] {
    auto rc = ::pthread_setname_np("my thread name");
    ASSERT_EQ(rc, 0) << std::strerror(rc);
    auto thread_names = cxxtrace::detail::thread_name_set{};
    thread_names.fetch_and_remember_name_of_current_thread();
    EXPECT_STREQ(
      thread_names.name_of_thread_by_id(cxxtrace::get_current_thread_id()),
      "my thread name");
  } }
    .join();
}
#endif

TEST(test_thread_name, current_thread_name_implementations_agree)
{
  std::thread{ [] {
    auto rc = ::pthread_setname_np("my thread name");
    ASSERT_EQ(rc, 0) << std::strerror(rc);

    using cxxtrace::detail::thread_name_set;

    auto thread_id = cxxtrace::get_current_thread_id();

    auto thread_name = [thread_id](auto&& fetch) -> std::optional<std::string> {
      auto names = cxxtrace::detail::thread_name_set{};
      fetch(names);
      auto name = cxxtrace::czstring{ names.name_of_thread_by_id(thread_id) };
      if (name) {
        return std::string{ name };
      } else {
        return std::nullopt;
      }
    };

    auto expected_thread_name_optional =
      thread_name([&](thread_name_set& names) {
        names.fetch_and_remember_name_of_current_thread();
      });
    ASSERT_TRUE(expected_thread_name_optional.has_value());
    auto expected_thread_name = *expected_thread_name_optional;

    {
      auto fetch = [&](thread_name_set& names) {
        names.fetch_and_remember_name_of_current_thread(thread_id);
      };
      EXPECT_EQ(thread_name(fetch), expected_thread_name);
    }

#if defined(__APPLE__) && defined(__MACH__)
    {
      auto fetch = [&](thread_name_set& names) {
        names.fetch_and_remember_name_of_current_thread_libproc(thread_id);
      };
      EXPECT_EQ(thread_name(fetch), expected_thread_name);
    }
#endif

#if defined(__APPLE__) && defined(__MACH__)
    {
      auto fetch = [&](thread_name_set& names) {
        names.fetch_and_remember_name_of_current_thread_mach(thread_id);
      };
      EXPECT_EQ(thread_name(fetch), expected_thread_name);
    }
#endif

#if defined(__APPLE__) && defined(__MACH__)
    {
      auto fetch = [&](thread_name_set& names) {
        names.fetch_and_remember_name_of_current_thread_pthread(thread_id);
      };
      EXPECT_EQ(thread_name(fetch), expected_thread_name);
    }
#endif

#if defined(__APPLE__) && defined(__MACH__)
    {
      auto fetch = [&](thread_name_set& names) {
        names.fetch_and_remember_thread_name_for_id(thread_id);
      };
      EXPECT_EQ(thread_name(fetch), expected_thread_name);
    }
#endif

#if defined(__APPLE__) && defined(__MACH__)
    {
      auto fetch = [&](thread_name_set& names) {
        names.fetch_and_remember_thread_name_for_id_libproc(thread_id);
      };
      EXPECT_EQ(thread_name(fetch), expected_thread_name);
    }
#endif

#if defined(__APPLE__) && defined(__MACH__)
    {
      auto fetch = [&](thread_name_set& names) {
        names.fetch_and_remember_thread_names_for_ids(&thread_id,
                                                      &thread_id + 1);
      };
      EXPECT_EQ(thread_name(fetch), expected_thread_name);
    }
#endif

#if defined(__APPLE__) && defined(__MACH__)
    {
      auto fetch = [&](thread_name_set& names) {
        names.fetch_and_remember_thread_names_for_ids_libproc(&thread_id,
                                                              &thread_id + 1);
      };
      EXPECT_EQ(thread_name(fetch), expected_thread_name);
    }
#endif
  } }
    .join();
}

#if defined(__APPLE__) && defined(__MACH__)
TEST(test_thread_name, names_of_other_live_threads_match_pthread_setname_np)
{
  auto test_done = cxxtrace_test::event{};

  struct test_thread
  {
    std::thread thread{};
    cxxtrace_test::event ready{};

    std::string name{};
    cxxtrace::thread_id id /* uninititialized */;
  };

  constexpr auto thread_count = 4;
  auto threads = std::array<test_thread, thread_count>{};
  for (auto thread_index = 0; thread_index < thread_count; ++thread_index) {
    auto& thread = threads[thread_index];
    thread.name = cxxtrace::stringify("thread ", thread_index, "name");
    thread.thread = std::thread{ [&, thread_index] {
      auto& thread = threads[thread_index];

      auto rc = ::pthread_setname_np(thread.name.c_str());
      ASSERT_EQ(rc, 0) << std::strerror(rc);

      thread.id = cxxtrace::get_current_thread_id();
      thread.ready.set();
      test_done.wait();
    } };
  }

  for (auto& thread : threads) {
    thread.ready.wait();
  }
  auto thread_ids = std::vector<cxxtrace::thread_id>{};
  for (auto& thread : threads) {
    thread_ids.emplace_back(thread.id);
  }

  auto names = cxxtrace::detail::thread_name_set{};
  names.fetch_and_remember_thread_names_for_ids(
    thread_ids.data(), thread_ids.data() + thread_ids.size());
  for (auto& thread : threads) {
    EXPECT_STREQ(names.name_of_thread_by_id(thread.id), thread.name.c_str());
  }

  test_done.set();
  for (auto& thread : threads) {
    thread.thread.join();
  }
}
#endif

#if defined(__APPLE__) && defined(__MACH__)
TEST(test_thread_name, dead_threads_have_no_name)
{
  auto thread_id = cxxtrace::thread_id{};
  std::thread{ [&] { thread_id = cxxtrace::get_current_thread_id(); } }.join();

  auto thread_names = cxxtrace::detail::thread_name_set{};
  thread_names.fetch_and_remember_thread_names_for_ids(&thread_id,
                                                       &thread_id + 1);
  EXPECT_FALSE(thread_names.name_of_thread_by_id(thread_id));
}
#endif

#if defined(__APPLE__) && defined(__MACH__)
TEST(test_thread_name, getting_name_of_live_thread_updates_set)
{
  auto rc = int{};

  auto thread_id = cxxtrace::get_current_thread_id();
  auto thread_names = cxxtrace::detail::thread_name_set{};

  rc = ::pthread_setname_np("first thread name");
  ASSERT_EQ(rc, 0) << std::strerror(rc);
  thread_names.fetch_and_remember_thread_names_for_ids(&thread_id,
                                                       &thread_id + 1);
  EXPECT_STREQ(thread_names.name_of_thread_by_id(thread_id),
               "first thread name");

  rc = ::pthread_setname_np("second thread name");
  ASSERT_EQ(rc, 0) << std::strerror(rc);
  thread_names.fetch_and_remember_thread_names_for_ids(&thread_id,
                                                       &thread_id + 1);
  EXPECT_STREQ(thread_names.name_of_thread_by_id(thread_id),
               "second thread name");
}
#endif

#if defined(__APPLE__) && defined(__MACH__)
TEST(test_thread_name, getting_name_of_new_live_thread_updates_set)
{
  auto test_done = cxxtrace_test::event{};

  auto thread_1_id = cxxtrace::thread_id{};
  auto thread_1_ready = cxxtrace_test::event{};
  auto thread_1 = std::thread{ [&] {
    auto rc = ::pthread_setname_np("thread 1 name");
    ASSERT_EQ(rc, 0) << std::strerror(rc);

    thread_1_id = cxxtrace::get_current_thread_id();
    thread_1_ready.set();
    test_done.wait();
  } };

  auto thread_2_id = cxxtrace::thread_id{};
  auto thread_2_ready = cxxtrace_test::event{};
  auto thread_2 = std::thread{ [&] {
    auto rc = ::pthread_setname_np("thread 2 name");
    ASSERT_EQ(rc, 0) << std::strerror(rc);

    thread_2_id = cxxtrace::get_current_thread_id();
    thread_2_ready.set();
    test_done.wait();
  } };

  thread_1_ready.wait();
  thread_2_ready.wait();

  ASSERT_NE(thread_1_id, thread_2_id);

  auto thread_names = cxxtrace::detail::thread_name_set{};
  thread_names.fetch_and_remember_thread_names_for_ids(&thread_1_id,
                                                       &thread_1_id + 1);
  EXPECT_STREQ(thread_names.name_of_thread_by_id(thread_1_id), "thread 1 name");
  EXPECT_FALSE(thread_names.name_of_thread_by_id(thread_2_id));
  thread_names.fetch_and_remember_thread_names_for_ids(&thread_2_id,
                                                       &thread_2_id + 1);
  EXPECT_STREQ(thread_names.name_of_thread_by_id(thread_1_id), "thread 1 name")
    << "get_thread_names_by_id should not change thread 1's name";
  EXPECT_STREQ(thread_names.name_of_thread_by_id(thread_2_id), "thread 2 name");

  test_done.set();
  thread_1.join();
  thread_2.join();
}
#endif

#if defined(__APPLE__) && defined(__MACH__)
TEST(test_thread_name, getting_name_of_dead_thread_does_not_update_set)
{
  auto kill_thread = cxxtrace_test::event{};

  auto thread_id = cxxtrace::thread_id{};
  auto thread_ready = cxxtrace_test::event{};
  auto thread = std::thread{ [&] {
    auto rc = int{};

    rc = ::pthread_setname_np("original thread name");
    ASSERT_EQ(rc, 0) << std::strerror(rc);

    thread_id = cxxtrace::get_current_thread_id();
    thread_ready.set();
    kill_thread.wait();

    rc = ::pthread_setname_np("updated thread name");
    ASSERT_EQ(rc, 0) << std::strerror(rc);
  } };

  thread_ready.wait();

  auto names = cxxtrace::detail::thread_name_set{};
  names.fetch_and_remember_thread_names_for_ids(&thread_id, &thread_id + 1);
  EXPECT_STREQ(names.name_of_thread_by_id(thread_id), "original thread name");

  kill_thread.set();
  thread.join();

  names.fetch_and_remember_thread_names_for_ids(&thread_id, &thread_id + 1);
  EXPECT_STREQ(names.name_of_thread_by_id(thread_id), "original thread name")
    << "Updated thread name should not be fetched (because thread thread died "
       "before we queried its name)";
}
#endif

#if defined(__APPLE__) && defined(__MACH__)
template<class Func>
auto
poll_for(std::chrono::milliseconds timeout, Func&& predicate) -> void
{
  poll_until(std::chrono::steady_clock::now() + timeout,
             std::forward<Func>(predicate));
}

template<class Clock, class Func>
auto
poll_until(std::chrono::time_point<Clock> deadline, Func&& predicate) -> void
{
  auto past_deadline = [&]() { return Clock::now() >= deadline; };
  for (;;) {
    if (predicate()) {
      return;
    }
    if (past_deadline()) {
      throw std::runtime_error("Timeout expired");
    }
    std::this_thread::sleep_for(1ms);
  }
}

auto
get_mach_port_type(mach_port_t port) -> mach_port_type_t
{
  auto type = mach_port_type_t{};
  auto rc = mach_port_type(mach_task_self(), port, &type);
  if (rc != KERN_SUCCESS) {
    mach_error("error: could not determine type of port:", rc);
    throw std::runtime_error("mach_port_type failed");
  }
  return type;
}
#endif
}
