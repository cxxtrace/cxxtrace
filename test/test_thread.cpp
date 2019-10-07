#include "event.h"
#include "thread.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if CXXTRACE_HAVE_PTHREAD_GETNAME_NP || CXXTRACE_HAVE_PTHREAD_THREADID_NP
#include <pthread.h>
#endif

#if CXXTRACE_HAVE_MACH_THREAD
#include <mach/kern_return.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_vm.h>
#include <stdexcept>
#endif

namespace {
#if CXXTRACE_HAVE_MACH_THREAD
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

  mach_array(const mach_array&) = delete;
  mach_array& operator=(const mach_array&) = delete;

  mach_array(mach_array&& other) noexcept
    : data{ std::exchange(other.data, nullptr) }
    , size{ std::exchange(other.size, 0) }
  {}

  mach_array& operator=(mach_array&&) = delete;

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
TEST(test_thread, thread_id_differs_on_different_live_threads)
{
  auto test_done_event = event{};

  auto thread_1_id = cxxtrace::thread_id{};
  auto thread_1_ready_event = event{};
  auto thread_1 = std::thread{ [&] {
    thread_1_id = cxxtrace::get_current_thread_id();
    thread_1_ready_event.set();
    test_done_event.wait();
  } };

  auto thread_2_id = cxxtrace::thread_id{};
  auto thread_2_ready_event = event{};
  auto thread_2 = std::thread{ [&] {
    thread_2_id = cxxtrace::get_current_thread_id();
    thread_2_ready_event.set();
    test_done_event.wait();
  } };

  thread_1_ready_event.wait();
  thread_2_ready_event.wait();

  auto main_thread_id = cxxtrace::get_current_thread_id();
  EXPECT_NE(main_thread_id, thread_1_id);
  EXPECT_NE(main_thread_id, thread_2_id);
  EXPECT_NE(thread_1_id, thread_2_id);

  test_done_event.set();
  thread_1.join();
  thread_2.join();
}

#if CXXTRACE_HAVE_MACH_THREAD && CXXTRACE_HAVE_PTHREAD_THREADID_NP
TEST(test_thread_pthread_thread_id, current_thread_id_matches_mach_thread_id)
{
  EXPECT_EQ(cxxtrace::get_current_thread_pthread_thread_id(),
            cxxtrace::get_current_thread_mach_thread_id());
  std::thread{
    [] {
      EXPECT_EQ(cxxtrace::get_current_thread_pthread_thread_id(),
                cxxtrace::get_current_thread_mach_thread_id());
    }
  }.join();
}
#endif

#if CXXTRACE_HAVE_MACH_THREAD
TEST(test_thread_mach_thread_id,
     getting_current_thread_id_does_not_leak_thread_port)
{
  auto thread_port = ::mach_port_t{};
  std::thread{
    [&thread_port] {
      thread_port = ::mach_thread_self();
      auto rc = ::mach_port_deallocate(::mach_task_self(), thread_port);
      ASSERT_EQ(rc, KERN_SUCCESS);

      cxxtrace::get_current_thread_mach_thread_id();
    }
  }.join();

  auto ports = get_mach_task_ports();
  EXPECT_EQ(std::count(ports.names.begin(), ports.names.end(), thread_port), 0);
}
#endif

#if CXXTRACE_HAVE_MACH_THREAD
TEST(test_thread, getting_current_thread_name_by_id_does_not_leak_thread_port)
{
  auto thread_port = ::mach_port_t{};
  std::thread{
    [&thread_port] {
      thread_port = ::mach_thread_self();
      auto rc = ::mach_port_deallocate(::mach_task_self(), thread_port);
      ASSERT_EQ(rc, KERN_SUCCESS);

      auto thread_names = cxxtrace::detail::thread_name_set{};
      thread_names.fetch_and_remember_name_of_current_thread_mach(
        cxxtrace::get_current_thread_id());
    }
  }.join();

  auto ports = get_mach_task_ports();
  EXPECT_EQ(std::count(ports.names.begin(), ports.names.end(), thread_port), 0);
}
#endif

#if CXXTRACE_HAVE_MACH_THREAD
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

#if CXXTRACE_HAVE_MACH_THREAD
TEST(test_get_mach_task_ports, excludes_port_of_dead_thread)
{
  auto thread_port = ::mach_port_t{};
  std::thread{
    [&thread_port] {
      thread_port = ::mach_thread_self();
      auto rc = ::mach_port_deallocate(::mach_task_self(), thread_port);
      ASSERT_EQ(rc, KERN_SUCCESS);
    }
  }.join();

  auto ports = get_mach_task_ports();
  EXPECT_EQ(std::count(ports.names.begin(), ports.names.end(), thread_port), 0);
}
#endif

#if CXXTRACE_HAVE_MACH_THREAD
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

#if defined(__APPLE__)
TEST(test_thread_name, default_thread_name_is_empty)
{
  std::thread{
    [] {
      auto thread_names = cxxtrace::detail::thread_name_set{};
      thread_names.fetch_and_remember_name_of_current_thread();
      EXPECT_STREQ(
        thread_names.name_of_thread_by_id(cxxtrace::get_current_thread_id()),
        "");
    }
  }.join();
}
#elif defined(__linux__)
TEST(test_thread_name, threads_inherit_name_from_parent)
{
  std::thread{
    [] {
      set_current_thread_name("parent");

      std::thread{
        [] {
          auto thread_names = cxxtrace::detail::thread_name_set{};
          thread_names.fetch_and_remember_name_of_current_thread();
          EXPECT_STREQ(thread_names.name_of_thread_by_id(
                         cxxtrace::get_current_thread_id()),
                       "parent");
        }
      }.join();
    }
  }.join();
}
#else
#error "Unknown platform"
#endif

#if CXXTRACE_HAVE_PTHREAD_SETNAME_NP
TEST(test_thread_name, current_thread_name_matches_pthread_setname_np)
{
  std::thread{
    [] {
      auto rc =
#if CXXTRACE_HAVE_PTHREAD_SETNAME_NP_1
        ::pthread_setname_np("my thread name")
#elif CXXTRACE_HAVE_PTHREAD_SETNAME_NP_2
        ::pthread_setname_np(::pthread_self(), "my thread name")
#else
#error "Unknown platform"
#endif
        ;
      ASSERT_EQ(rc, 0) << std::strerror(rc);
      auto thread_names = cxxtrace::detail::thread_name_set{};
      thread_names.fetch_and_remember_name_of_current_thread();
      EXPECT_STREQ(
        thread_names.name_of_thread_by_id(cxxtrace::get_current_thread_id()),
        "my thread name");
    }
  }.join();
}
#endif

TEST(test_thread_name, current_thread_name_implementations_agree)
{
  std::thread{ [] {
    set_current_thread_name("my thread name");

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

#if CXXTRACE_HAVE_PROC_PIDINFO
    {
      auto fetch = [&](thread_name_set& names) {
        names.fetch_and_remember_name_of_current_thread_libproc(thread_id);
      };
      EXPECT_EQ(thread_name(fetch), expected_thread_name);
    }
#endif

#if CXXTRACE_HAVE_MACH_THREAD
    {
      auto fetch = [&](thread_name_set& names) {
        names.fetch_and_remember_name_of_current_thread_mach(thread_id);
      };
      EXPECT_EQ(thread_name(fetch), expected_thread_name);
    }
#endif

#if CXXTRACE_HAVE_PTHREAD_GETNAME_NP
    {
      auto fetch = [&](thread_name_set& names) {
        names.fetch_and_remember_name_of_current_thread_pthread(thread_id);
      };
      EXPECT_EQ(thread_name(fetch), expected_thread_name);
    }
#endif

    {
      auto fetch = [&](thread_name_set& names) {
        names.fetch_and_remember_thread_name_for_id(thread_id);
      };
      EXPECT_EQ(thread_name(fetch), expected_thread_name);
    }

#if CXXTRACE_HAVE_PROC_PIDINFO
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

TEST(test_thread_name, names_of_other_live_threads)
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

      set_current_thread_name(thread.name.c_str());

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

TEST(test_thread_name, dead_threads_have_no_name)
{
  auto thread_id = cxxtrace::thread_id{};
  auto thread =
    std::thread{ [&] { thread_id = cxxtrace::get_current_thread_id(); } };
  thread.join();
  await_thread_exit(thread, thread_id);

  auto thread_names = cxxtrace::detail::thread_name_set{};
  thread_names.fetch_and_remember_thread_name_for_id(thread_id);
  EXPECT_FALSE(thread_names.name_of_thread_by_id(thread_id));
}

TEST(test_thread_name, getting_name_of_live_thread_updates_set)
{
  auto thread_id = cxxtrace::get_current_thread_id();
  auto thread_names = cxxtrace::detail::thread_name_set{};

  set_current_thread_name("first thread");
  thread_names.fetch_and_remember_thread_name_for_id(thread_id);
  EXPECT_STREQ(thread_names.name_of_thread_by_id(thread_id), "first thread");

  set_current_thread_name("second thread");
  thread_names.fetch_and_remember_thread_name_for_id(thread_id);
  EXPECT_STREQ(thread_names.name_of_thread_by_id(thread_id), "second thread");
}

TEST(test_thread_name, getting_name_of_new_live_thread_updates_set)
{
  auto test_done = cxxtrace_test::event{};

  auto thread_1_id = cxxtrace::thread_id{};
  auto thread_1_ready = cxxtrace_test::event{};
  auto thread_1 = std::thread{ [&] {
    set_current_thread_name("thread 1 name");

    thread_1_id = cxxtrace::get_current_thread_id();
    thread_1_ready.set();
    test_done.wait();
  } };

  auto thread_2_id = cxxtrace::thread_id{};
  auto thread_2_ready = cxxtrace_test::event{};
  auto thread_2 = std::thread{ [&] {
    set_current_thread_name("thread 2 name");

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

TEST(test_thread_name, getting_name_of_dead_thread_does_not_update_set)
{
  auto kill_thread = cxxtrace_test::event{};

  auto thread_id = cxxtrace::thread_id{};
  auto thread_ready = cxxtrace_test::event{};
  auto thread = std::thread{ [&] {
    set_current_thread_name("original name");

    thread_id = cxxtrace::get_current_thread_id();
    thread_ready.set();
    kill_thread.wait();

    set_current_thread_name("updated name");
  } };

  thread_ready.wait();

  auto names = cxxtrace::detail::thread_name_set{};
  names.fetch_and_remember_thread_name_for_id(thread_id);
  EXPECT_STREQ(names.name_of_thread_by_id(thread_id), "original name");

  kill_thread.set();
  thread.join();
  await_thread_exit(thread, thread_id);

  names.fetch_and_remember_thread_name_for_id(thread_id);
  EXPECT_STREQ(names.name_of_thread_by_id(thread_id), "original name")
    << "Updated thread name should not be fetched (because thread thread died "
       "before we queried its name)";
}
}

namespace {
#if CXXTRACE_HAVE_MACH_THREAD
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
