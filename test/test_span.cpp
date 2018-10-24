#include <algorithm>
#include <array>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/span.h>
#include <cxxtrace/string.h>
#include <cxxtrace/testing.h>
#include <cxxtrace/thread.h>
#include <gtest/gtest.h>
#include <mutex>
#include <random>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>

using namespace std::literals::string_view_literals;

namespace {
template<class... Args>
auto
stringify(Args&&...) -> std::string;

template<class T>
auto
do_not_optimize_away(const T&) noexcept -> void;

class test_span : public testing::Test
{
protected:
  void TearDown() noexcept override { cxxtrace::clear_all_samples(); }
};

TEST_F(test_span, no_events_exist_by_default)
{
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 0);
}

TEST_F(test_span, span_adds_event_at_scope_enter)
{
  auto span = CXXTRACE_SPAN("span category", "span name");
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 1);
  auto event = cxxtrace::event_ref{ events.at(0) };
  EXPECT_STREQ(event.category(), "span category");
  EXPECT_STREQ(event.name(), "span name");
  EXPECT_EQ(event.kind(), cxxtrace::event_kind::incomplete_span);
}

TEST_F(test_span, incomplete_spans_includes_span_in_scope)
{
  auto span = CXXTRACE_SPAN("span category", "span name");
  auto spans =
    cxxtrace::incomplete_spans_snapshot{ cxxtrace::copy_incomplete_spans() };
  EXPECT_EQ(spans.size(), 1);
  EXPECT_STREQ(spans.at(0).category(), "span category");
  EXPECT_STREQ(spans.at(0).name(), "span name");
}

TEST_F(test_span, incomplete_spans_excludes_span_for_exited_scope)
{
  {
    auto span = CXXTRACE_SPAN("span category", "span name");
  }
  auto spans =
    cxxtrace::incomplete_spans_snapshot{ cxxtrace::copy_incomplete_spans() };
  EXPECT_EQ(spans.size(), 0);
}

TEST_F(test_span, span_adds_event_at_scope_exit)
{
  {
    auto span = CXXTRACE_SPAN("span category", "span name");
  }
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 1);
  auto event = cxxtrace::event_ref{ events.at(0) };
  EXPECT_STREQ(event.category(), "span category");
  EXPECT_STREQ(event.name(), "span name");
  EXPECT_EQ(event.kind(), cxxtrace::event_kind::span);
}

TEST_F(test_span, events_for_later_incomplete_spans_spans_appear_later)
{
  auto span_1 = CXXTRACE_SPAN("span category", "span name 1");
  auto span_2 = CXXTRACE_SPAN("span category", "span name 2");
  auto span_3 = CXXTRACE_SPAN("span category", "span name 3");
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 3);
  auto event_1 = cxxtrace::event_ref{ events.at(0) };
  EXPECT_STREQ(event_1.name(), "span name 1");
  auto event_2 = cxxtrace::event_ref{ events.at(1) };
  EXPECT_STREQ(event_2.name(), "span name 2");
  auto event_3 = cxxtrace::event_ref{ events.at(2) };
  EXPECT_STREQ(event_3.name(), "span name 3");
}

TEST_F(test_span, events_for_later_spans_appear_later)
{
  {
    auto span_1 = CXXTRACE_SPAN("span category", "span name 1");
    auto span_2 = CXXTRACE_SPAN("span category", "span name 2");
    auto span_3 = CXXTRACE_SPAN("span category", "span name 3");
  }
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 3);
  EXPECT_STREQ(events.at(0).name(), "span name 3");
  EXPECT_STREQ(events.at(1).name(), "span name 2");
  EXPECT_STREQ(events.at(2).name(), "span name 1");
}

TEST_F(test_span, event_for_incomplete_span_appears_after_sibling_complete_span)
{
  {
    auto complete_span = CXXTRACE_SPAN("span category", "complete span");
  }
  auto incomplete_span = CXXTRACE_SPAN("span category", "incomplete span");
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 2);
  EXPECT_STREQ(events.at(0).name(), "complete span");
  EXPECT_STREQ(events.at(1).name(), "incomplete span");
}

TEST_F(test_span, event_for_complete_span_appears_after_parent_incomplete_span)
{
  auto incomplete_span = CXXTRACE_SPAN("span category", "incomplete span");
  {
    auto complete_span = CXXTRACE_SPAN("span category", "complete span");
  }
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 2);
  EXPECT_STREQ(events.at(0).name(), "incomplete span");
  EXPECT_STREQ(events.at(1).name(), "complete span");
}

TEST_F(test_span, span_events_include_thread_id)
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

  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  auto get_event = [&events](std::string_view name) -> cxxtrace::event_ref {
    for (auto i = cxxtrace::events_snapshot::size_type{ 0 }; i < events.size();
         ++i) {
      auto event = cxxtrace::event_ref{ events.at(i) };
      if (event.name() == name) {
        return event;
      }
    }
    throw std::out_of_range("Could not find event");
  };
  EXPECT_EQ(get_event("main span").thread_id(), main_thread_id);
  EXPECT_EQ(get_event("thread 1 span").thread_id(), thread_1_id);
  EXPECT_EQ(get_event("thread 2 span").thread_id(), thread_2_id);
}

TEST_F(test_span, span_events_can_interleave_using_multiple_threads)
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

  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 2);
  EXPECT_NO_THROW(events.get_if([](const cxxtrace::event_ref& event) {
    return event.name() == "main span"sv;
  }));
  EXPECT_NO_THROW(events.get_if([](const cxxtrace::event_ref& event) {
    return event.name() == "thread span"sv;
  }));
}

TEST_F(test_span, span_enter_and_exit_synchronize_across_threads)
{
  // NOTE(strager): This test relies on dynamic analysis tools or crashes (due
  // to undefined behavior) to detect data races.

  static constexpr const auto max_work_size = std::size_t{ 10000 };
  static const auto work_iteration_count = 400;
  static const auto thread_count = 4;

  struct workload
  {
    using item_type = int;

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

    std::mt19937 rng{};
    std::uniform_int_distribution<std::size_t> size_distribution{
      0,
      max_work_size
    };
    std::uniform_int_distribution<item_type> item_distribution{ 0, 10 };
  };

  auto thread_routine = []() noexcept->void
  {
    auto thread_span = CXXTRACE_SPAN("workload", "thread");
    auto work = workload{};
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

template<class... Args>
auto
stringify(Args&&... args) -> std::string
{
  std::ostringstream stream;
  (stream << ... << std::forward<Args>(args));
  return stream.str();
}

template<class T>
auto
do_not_optimize_away(const T&) noexcept -> void
{
  // TODO
}
}
