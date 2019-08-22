#include "event.h"
#include "test_processor_id.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/kern_return.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <stdexcept>
#endif

using namespace std::literals::chrono_literals;

namespace {
#if defined(__APPLE__) && defined(__MACH__)
template<class T>
class mach_array
{
public:
  explicit mach_array() noexcept
    : data{ nullptr }
    , size{ 0 }
  {}

  explicit mach_array(T* data, ::mach_msg_type_number_t size) noexcept
    : data{ data }
    , size{ size }
  {}

  mach_array(mach_array&& other) noexcept
    : data{ std::exchange(other.data, nullptr) }
    , size{ std::exchange(other.size, 0) }
  {}

  mach_array& operator=(const mach_array&) = delete;

  ~mach_array() { this->reset(); }

  auto begin() noexcept -> T* { return this->data; }

  auto end() noexcept -> T* { return this->data + this->size; }

  auto reset() noexcept -> void
  {
    if (!this->data) {
      return;
    }

    auto task = ::mach_task_self();
    auto rc =
      ::mach_vm_deallocate(task,
                           reinterpret_cast<::mach_vm_address_t>(this->data),
                           this->size * sizeof(T));
    if (rc != KERN_SUCCESS) {
      ::mach_error("error: could not deallocate array", rc);
    }
    this->data = nullptr;
  }

  T* data;
  ::mach_msg_type_number_t size;
};

struct mach_task_ports
{
  mach_array<::mach_port_name_t> names;
  mach_array<::mach_port_type_t> types;
};

auto
get_mach_task_ports() -> mach_task_ports;
#endif
}

namespace cxxtrace_test {
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
  auto thread_port = ::mach_port_t{};
  std::thread{ [&thread_port] {
    thread_port = ::mach_thread_self();
    auto rc = ::mach_port_deallocate(::mach_task_self(), thread_port);
    ASSERT_EQ(rc, KERN_SUCCESS);

    cxxtrace::get_current_thread_mach_thread_id();
  } }
    .join();

  auto ports = get_mach_task_ports();
  EXPECT_EQ(std::count(ports.names.begin(), ports.names.end(), thread_port), 0);
}
#endif

#if defined(__APPLE__) && defined(__MACH__)
TEST(test_thread, getting_current_thread_name_by_id_does_not_leak_thread_port)
{
  auto thread_port = ::mach_port_t{};
  std::thread{ [&thread_port] {
    thread_port = ::mach_thread_self();
    auto rc = ::mach_port_deallocate(::mach_task_self(), thread_port);
    ASSERT_EQ(rc, KERN_SUCCESS);

    auto thread_names = cxxtrace::detail::thread_name_set{};
    thread_names.fetch_and_remember_name_of_current_thread_mach(
      cxxtrace::get_current_thread_id());
  } }
    .join();

  auto ports = get_mach_task_ports();
  EXPECT_EQ(std::count(ports.names.begin(), ports.names.end(), thread_port), 0);
}
#endif

#if defined(__APPLE__) && defined(__MACH__)
TEST(test_thread, task_ports_includes_port_of_live_thread)
{
  auto test_done_event = event{};

  auto thread_port = mach_port_t{};
  auto thread_ready_event = event{};
  auto thread = std::thread{ [&] {
    thread_port = ::mach_thread_self();

    thread_ready_event.set();
    test_done_event.wait();

    auto rc = ::mach_port_deallocate(::mach_task_self(), thread_port);
    ASSERT_EQ(rc, KERN_SUCCESS);
  } };
  thread_ready_event.wait();

  auto ports = get_mach_task_ports();
  EXPECT_EQ(std::count(ports.names.begin(), ports.names.end(), thread_port), 1);

  test_done_event.set();
  thread.join();
}
#endif

#if defined(__APPLE__) && defined(__MACH__)
TEST(test_get_mach_task_ports, excludes_port_of_dead_thread)
{
  auto thread_port = ::mach_port_t{};
  std::thread{ [&thread_port] {
    thread_port = ::mach_thread_self();
    auto rc = ::mach_port_deallocate(::mach_task_self(), thread_port);
    ASSERT_EQ(rc, KERN_SUCCESS);
  } }
    .join();

  auto ports = get_mach_task_ports();
  EXPECT_EQ(std::count(ports.names.begin(), ports.names.end(), thread_port), 0);
}
#endif

#if defined(__APPLE__) && defined(__MACH__)
TEST(test_get_mach_task_ports, includes_port_of_dead_referenced_thread)
{
  auto thread_port = mach_port_t{};
  std::thread{ [&] { thread_port = ::mach_thread_self(); } }.join();

  auto ports = get_mach_task_ports();
  EXPECT_EQ(std::count(ports.names.begin(), ports.names.end(), thread_port), 1);

  auto rc = ::mach_port_deallocate(::mach_task_self(), thread_port);
  ASSERT_EQ(rc, KERN_SUCCESS);
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
    thread.name = stringify("thread ", thread_index, "name");
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

  auto names = cxxtrace::detail::thread_name_set{};
  for (auto& thread : threads) {
    names.fetch_and_remember_thread_name_for_id(thread.id);
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

  // HACK(strager): Work around flakiness caused by std::thread::join not
  // waiting for kernel resources to be released (with Clang 8 on macOS 10.12).
  std::this_thread::sleep_for(10ms);

  auto thread_names = cxxtrace::detail::thread_name_set{};
  thread_names.fetch_and_remember_thread_name_for_id(thread_id);
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
  thread_names.fetch_and_remember_thread_name_for_id(thread_id);
  EXPECT_STREQ(thread_names.name_of_thread_by_id(thread_id),
               "first thread name");

  rc = ::pthread_setname_np("second thread name");
  ASSERT_EQ(rc, 0) << std::strerror(rc);
  thread_names.fetch_and_remember_thread_name_for_id(thread_id);
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
  thread_names.fetch_and_remember_thread_name_for_id(thread_1_id);
  EXPECT_STREQ(thread_names.name_of_thread_by_id(thread_1_id), "thread 1 name");
  EXPECT_FALSE(thread_names.name_of_thread_by_id(thread_2_id));
  thread_names.fetch_and_remember_thread_name_for_id(thread_2_id);
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
  names.fetch_and_remember_thread_name_for_id(thread_id);
  EXPECT_STREQ(names.name_of_thread_by_id(thread_id), "original thread name");

  kill_thread.set();
  thread.join();
  // HACK(strager): Work around flakiness caused by std::thread::join not
  // waiting for kernel resources to be released (with Clang 8 on macOS 10.12).
  std::this_thread::sleep_for(10ms);

  names.fetch_and_remember_thread_name_for_id(thread_id);
  EXPECT_STREQ(names.name_of_thread_by_id(thread_id), "original thread name")
    << "Updated thread name should not be fetched (because thread thread died "
       "before we queried its name)";
}
#endif

TEST_P(test_processor_id,
       current_processor_id_is_no_greater_than_maximum_processor_id)
{
  // TODO(strager): Investigate CPU hot-plugging.
  auto maximum_processor_id = cxxtrace::detail::get_maximum_processor_id();
  std::fprintf(stderr,
               "maximum_processor_id = %ju\n",
               std::uintmax_t{ maximum_processor_id });

  auto check = [maximum_processor_id, this] {
    for (auto i = 0; i < 100; ++i) {
      EXPECT_LE(this->get_current_processor_id(), maximum_processor_id);
    }
  };
  auto thread_routine = [&check] {
    // TODO(strager): Use a semaphore to make all threads start at the same
    // time.
    check();
    std::this_thread::sleep_for(100ms);
    check();
  };

  // Create more threads than processors in an attempt to get all processors to
  // schedule our threads.
  auto thread_count = std::size_t{ maximum_processor_id } * 2;
  auto threads = std::vector<std::thread>{};
  for (auto i = std::size_t{ 0 }; i < thread_count; ++i) {
    threads.emplace_back(thread_routine);
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

INSTANTIATE_TEST_CASE_P(,
                        test_processor_id,
                        test_processor_id::param_values,
                        test_processor_id::param_name);
}

namespace {
#if defined(__APPLE__) && defined(__MACH__)
auto
get_mach_task_ports() -> mach_task_ports
{
  ::mach_task_ports ports;
  auto rc = ::mach_port_names(::mach_task_self(),
                              &ports.names.data,
                              &ports.names.size,
                              &ports.types.data,
                              &ports.types.size);
  if (rc != KERN_SUCCESS) {
    ::mach_error("error: could not query ports", rc);
    throw std::runtime_error("mach_port_names failed");
  }
  return ports;
}
#endif
}
