#ifndef CXXTRACE_SAMPLE_CHECKER_H
#define CXXTRACE_SAMPLE_CHECKER_H

#include "test_processor_id_manual.h"
#include <gtest/gtest.h>
#include <map>
#include <unordered_map>

namespace cxxtrace_test {
class sample_checker
{
public:
  explicit sample_checker(const thread_executions&);

  auto check_sample(const processor_id_samples::sample&)
    -> testing::AssertionResult;

private:
  using timestamp = thread_schedule_tracer::timestamp;

  static constexpr auto invalid_thread_id = ~cxxtrace::thread_id{ 0 };

  static auto scheduled_thread_at_time(
    const std::map<timestamp, cxxtrace::thread_id>&,
    timestamp) -> std::map<timestamp, cxxtrace::thread_id>::const_iterator;

  std::unordered_map<cxxtrace::detail::processor_id,
                     std::map<timestamp, cxxtrace::thread_id>>
    processor_timelines;
};
}

#endif
