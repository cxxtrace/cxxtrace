#include "gtest_scoped_trace.h"
#include "sample_checker.h"
#include "test_processor_id_manual.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cxxtrace/clock.h>
#include <gtest/gtest.h>
#include <map>
#include <unordered_map>
#include <utility>

using cxxtrace::thread_id;
using cxxtrace::detail::processor_id;

namespace cxxtrace_test {
sample_checker::sample_checker(const thread_executions& executions)
{
  for (const auto& [processor_id, processor_executions] :
       executions.by_processor) {
    auto& processor_timeline = this->processor_timelines[processor_id];
    for (const auto& execution : processor_executions) {
      assert(execution.begin_timestamp < execution.end_timestamp);

      processor_timeline[execution.begin_timestamp] = execution.thread_id;
      assert(processor_timeline.count(execution.end_timestamp) == 0);
      processor_timeline[execution.end_timestamp] = invalid_thread_id;
    }
  }
}

auto
sample_checker::scheduled_thread_at_time(
  const std::map<cxxtrace::time_point, thread_id>& processor_timeline,
  cxxtrace::time_point t)
  -> std::map<cxxtrace::time_point, thread_id>::const_iterator
{
  if (processor_timeline.empty()) {
    return processor_timeline.end();
  }
  auto it = processor_timeline.lower_bound(t);
  if (it == processor_timeline.end()) {
    assert(it != processor_timeline.begin());
    --it;
  }
  for (;;) {
    assert(it != processor_timeline.end());
    if (it == processor_timeline.begin()) {
      break;
      // FIXME(strager): Should this be < or <=?
    } else if (it->first < t) {
      break;
    } else {
      --it;
    }
  }
  return it;
}

auto
sample_checker::check_sample(const processor_id_samples::sample& sample)
  -> testing::AssertionResult
{
  auto annotated_result =
    [&](testing::AssertionResult result) -> testing::AssertionResult {
    result << "\nSample: t=[" << sample.timestamp_before << ".."
           << sample.timestamp_after << "] thread_id=" << sample.thread_id
           << " processor_id=" << sample.processor_id
           << "\nThreads scheduled during this time range:";
    for (const auto& [processor_id, processor_timeline] :
         this->processor_timelines) {
      auto before_it = this->scheduled_thread_at_time(processor_timeline,
                                                      sample.timestamp_before);
      if (before_it == processor_timeline.end()) {
        continue;
      }
      auto end_it = processor_timeline.upper_bound(sample.timestamp_after);
      for (auto it = before_it; it != end_it; ++it) {
        result << "\n  Processor " << processor_id << ": t=" << it->first
               << " thread_id=";
        auto thread_id = it->second;
        if (thread_id == invalid_thread_id) {
          result << "(none)";
        } else {
          result << thread_id;
        }
      }
    }
    return result;
  };

  auto found_matching_processor = false;
  auto thread_was_scheduled_at_timestamp_before = false;
  auto thread_was_scheduled_at_timestamp_after = false;
  for (const auto& [processor_id, processor_timeline] :
       this->processor_timelines) {
    if (found_matching_processor && thread_was_scheduled_at_timestamp_before &&
        thread_was_scheduled_at_timestamp_after) {
      break;
    }

    if (!found_matching_processor ||
        !thread_was_scheduled_at_timestamp_before) {
      auto before_it = this->scheduled_thread_at_time(processor_timeline,
                                                      sample.timestamp_before);
      if (before_it != processor_timeline.end()) {
        if (processor_id == sample.processor_id) {
          auto end_it = processor_timeline.upper_bound(sample.timestamp_after);
          assert(!found_matching_processor);
          found_matching_processor = std::any_of(
            before_it,
            end_it,
            [&](const std::pair<const cxxtrace::time_point, thread_id>& entry) {
              return entry.second == sample.thread_id;
            });
        }

        if (!thread_was_scheduled_at_timestamp_before) {
          if (before_it->first <= sample.timestamp_before &&
              before_it->second == sample.thread_id) {
            thread_was_scheduled_at_timestamp_before = true;
          }
        }
      }
    }

    if (!thread_was_scheduled_at_timestamp_after) {
      auto after_it = this->scheduled_thread_at_time(processor_timeline,
                                                     sample.timestamp_after);
      if (after_it != processor_timeline.end() &&
          after_it->first <= sample.timestamp_after &&
          after_it->second == sample.thread_id) {
        thread_was_scheduled_at_timestamp_after = true;
      }
    }
  }

  if (!thread_was_scheduled_at_timestamp_before) {
    return annotated_result(testing::AssertionFailure()
                            << "Bad sample; thread was not running when "
                               "timestamp-before was measured");
  }
  if (!thread_was_scheduled_at_timestamp_after) {
    return annotated_result(testing::AssertionFailure()
                            << "Bad sample; thread was not running when "
                               "timestamp-after was measured");
  }
  if (!found_matching_processor) {
    return annotated_result(testing::AssertionFailure()
                            << "Bad sample; sampled processor did not schedule "
                               "thread during time range");
  }
  return testing::AssertionSuccess();
}

namespace {
struct thread_execution
{
  cxxtrace::time_point begin_timestamp;
  cxxtrace::time_point end_timestamp;
  cxxtrace::detail::processor_id processor_id;
  cxxtrace::thread_id thread_id;
};

template<std::size_t Size>
auto
check_sample(const thread_execution (&)[Size],
             const processor_id_samples::sample&) -> testing::AssertionResult;

auto operator""_t(unsigned long long time) -> cxxtrace::time_point
{
  return cxxtrace::time_point{ std::chrono::nanoseconds{ time } };
}
}

TEST(test_processor_id_manual_sample_checker,
     check_fails_if_no_thread_executions)
{
  auto thread = thread_id{ 1 };
  auto processor = processor_id{ 9 };
  auto checker = sample_checker{ thread_executions{} };
  EXPECT_FALSE(checker.check_sample({ 30_t, 70_t, processor, thread }));
}

TEST(test_processor_id_manual_sample_checker,
     check_passes_if_thread_was_ever_scheduled_on_sampled_processor)
{
  auto thread = thread_id{ 1 };
  auto sampled_processor = processor_id{ 9 };
  auto other_processor = processor_id{ 10 };
  {
    CXXTRACE_SCOPED_TRACE() << "thread was always on sampled processor";
    thread_execution thread_executions[] = {
      { 0_t, 100_t, sampled_processor, thread },
    };
    EXPECT_TRUE(check_sample(thread_executions,
                             { 30_t, 70_t, sampled_processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE()
      << "thread switched processors between sampled times";
    thread_execution thread_executions[] = {
      { 0_t, 49_t, sampled_processor, thread },
      { 50_t, 100_t, sampled_processor, thread },
    };
    EXPECT_TRUE(check_sample(thread_executions,
                             { 30_t, 70_t, sampled_processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE()
      << "thread switched to sampled processor between sampled times";
    thread_execution thread_executions[] = {
      { 0_t, 49_t, other_processor, thread },
      { 50_t, 100_t, sampled_processor, thread },
    };
    EXPECT_TRUE(check_sample(thread_executions,
                             { 30_t, 70_t, sampled_processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE()
      << "thread switched away from sampled processor between sampled times";
    thread_execution thread_executions[] = {
      { 0_t, 49_t, sampled_processor, thread },
      { 50_t, 100_t, other_processor, thread },
    };
    EXPECT_TRUE(check_sample(thread_executions,
                             { 30_t, 70_t, sampled_processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE() << "thread switched away from sampled processor "
                               "then back to sampled processor";
    thread_execution thread_executions[] = {
      { 0_t, 39_t, sampled_processor, thread },
      { 40_t, 59_t, other_processor, thread },
      { 60_t, 100_t, sampled_processor, thread },
    };
    EXPECT_TRUE(check_sample(thread_executions,
                             { 30_t, 70_t, sampled_processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE() << "thread switched to sampled processor then away "
                               "from sampled processor";
    thread_execution thread_executions[] = {
      { 0_t, 39_t, other_processor, thread },
      { 40_t, 59_t, sampled_processor, thread },
      { 60_t, 100_t, other_processor, thread },
    };
    EXPECT_TRUE(check_sample(thread_executions,
                             { 30_t, 70_t, sampled_processor, thread }));
  }
}

TEST(
  test_processor_id_manual_sample_checker,
  check_fails_if_thread_was_not_scheduled_on_sampled_processor_during_time_span)
{
  auto thread = thread_id{ 1 };
  auto other_thread = thread_id{ 2 };
  auto sampled_processor = processor_id{ 9 };
  auto other_processor = processor_id{ 10 };
  {
    CXXTRACE_SCOPED_TRACE() << "thread was never on any processor";
    thread_execution thread_executions[] = {
      { 0_t, 100_t, sampled_processor, other_thread },
    };
    EXPECT_FALSE(check_sample(thread_executions,
                              { 30_t, 70_t, sampled_processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE()
      << "thread was not on any processor during sample period";
    thread_execution thread_executions[] = {
      { 0_t, 20_t, other_processor, thread },
      { 80_t, 100_t, other_processor, thread },
    };
    EXPECT_FALSE(check_sample(thread_executions,
                              { 30_t, 70_t, sampled_processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE() << "thread was scheduled on different processor";
    thread_execution thread_executions[] = {
      { 0_t, 100_t, other_processor, thread },
    };
    EXPECT_FALSE(check_sample(thread_executions,
                              { 30_t, 70_t, sampled_processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE()
      << "thread was scheduled on processor after sample time range";
    thread_execution thread_executions[] = {
      { 0_t, 80_t, other_processor, thread },
      { 90_t, 100_t, sampled_processor, thread },
    };
    EXPECT_FALSE(check_sample(thread_executions,
                              { 30_t, 70_t, sampled_processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE()
      << "thread was scheduled on processor before sample time range";
    thread_execution thread_executions[] = {
      { 0_t, 10_t, sampled_processor, thread },
      { 20_t, 100_t, other_processor, thread },
    };
    EXPECT_FALSE(check_sample(thread_executions,
                              { 30_t, 70_t, sampled_processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE()
      << "thread was scheduled on processor before and after sample time range";
    thread_execution thread_executions[] = {
      { 0_t, 10_t, sampled_processor, thread },
      { 20_t, 80_t, other_processor, thread },
      { 90_t, 100_t, sampled_processor, thread },
    };
    EXPECT_FALSE(check_sample(thread_executions,
                              { 30_t, 70_t, sampled_processor, thread }));
  }
}

TEST(test_processor_id_manual_sample_checker,
     check_fails_if_thread_was_not_scheduled_on_any_processor_at_sampled_times)
{
  auto thread = thread_id{ 1 };
  auto processor = processor_id{ 9 };
  {
    CXXTRACE_SCOPED_TRACE()
      << "thread was scheduled only before timestamp-before";
    thread_execution thread_executions[] = {
      { 0_t, 10_t, processor, thread },
    };
    EXPECT_FALSE(
      check_sample(thread_executions, { 30_t, 70_t, processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE()
      << "thread was scheduled during timestamp-before but not timestamp-after";
    thread_execution thread_executions[] = {
      { 0_t, 50_t, processor, thread },
    };
    EXPECT_FALSE(
      check_sample(thread_executions, { 30_t, 70_t, processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE()
      << "thread was scheduled only after timestamp-after";
    thread_execution thread_executions[] = {
      { 90_t, 100_t, processor, thread },
    };
    EXPECT_FALSE(
      check_sample(thread_executions, { 30_t, 70_t, processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE()
      << "thread was scheduled during timestamp-after but not timestamp-before";
    thread_execution thread_executions[] = {
      { 50_t, 100_t, processor, thread },
    };
    EXPECT_FALSE(
      check_sample(thread_executions, { 30_t, 70_t, processor, thread }));
  }
  {
    CXXTRACE_SCOPED_TRACE()
      << "thread was scheduled between timestamp-before and timestamp-after";
    thread_execution thread_executions[] = {
      { 40_t, 60_t, processor, thread },
    };
    EXPECT_FALSE(
      check_sample(thread_executions, { 30_t, 70_t, processor, thread }));
  }
}

namespace {
auto
check_sample(const thread_execution* thread_executions_begin,
             const thread_execution* thread_executions_end,
             const processor_id_samples::sample& sample)
  -> testing::AssertionResult
{
  auto executions = thread_executions{};
  for (auto it = thread_executions_begin; it != thread_executions_end; ++it) {
    auto& processor_executions = executions.by_processor[it->processor_id];
    processor_executions.push_back(
      { it->begin_timestamp, it->end_timestamp, it->thread_id });
  }
  return sample_checker{ executions }.check_sample(sample);
}

template<std::size_t Size>
auto
check_sample(const thread_execution (&thread_executions)[Size],
             const processor_id_samples::sample& sample)
  -> testing::AssertionResult
{
  return check_sample(
    std::begin(thread_executions), std::end(thread_executions), sample);
}
}
}
