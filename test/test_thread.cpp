#include <chrono>
#include <cxxtrace/thread.h>
#include <gtest/gtest.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/kern_return.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <stdexcept>
#include <thread>
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
