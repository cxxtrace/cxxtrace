#ifndef CXXTRACE_TEST_PROCESSOR_ID_MANUAL_H
#define CXXTRACE_TEST_PROCESSOR_ID_MANUAL_H

#include "event.h"
#include <cstdint>
#include <cxxtrace/clock.h>
#include <cxxtrace/detail/processor.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <deque>
#include <iosfwd>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace cxxtrace_test {
struct thread_executions;

class thread_schedule_tracer
{
public:
  using clock = cxxtrace::apple_absolute_time_clock;
  using timestamp = clock::sample;

  explicit thread_schedule_tracer(::pid_t, clock*);

  thread_schedule_tracer(const thread_schedule_tracer&) = delete;
  thread_schedule_tracer& operator=(const thread_schedule_tracer&) = delete;

  ~thread_schedule_tracer();

  auto initialize() -> void;
  auto start() -> void;
  auto get_thread_executions() const -> thread_executions;
  auto stop() -> void;

private:
  struct impl;

  std::unique_ptr<impl> impl_;
};

struct thread_executions
{
  using processor_id = cxxtrace::detail::processor_id;

  struct thread_execution
  {
    cxxtrace::time_point begin_timestamp;
    cxxtrace::time_point end_timestamp;
    cxxtrace::thread_id thread_id;
  };

  std::unordered_map<processor_id, std::vector<thread_execution>> by_processor;
};

struct processor_id_samples
{
  struct sample
  {
    cxxtrace::time_point timestamp_before;
    cxxtrace::time_point timestamp_after;
    cxxtrace::detail::processor_id processor_id;
    cxxtrace::thread_id thread_id;
  };

  /*implicit*/ processor_id_samples() = default;

  template<std::size_t Size>
  /*implicit*/ processor_id_samples(sample (&samples)[Size])
    : processor_id_samples{ std::begin(samples), std::end(samples) }
  {}

  explicit processor_id_samples(sample* begin, sample* end)
    : samples{ begin, end }
  {}

  std::vector<sample> samples;
};

auto
dump_gnuplot(std::ostream&,
             const thread_executions&,
             const processor_id_samples&,
             cxxtrace::time_point begin_timestamp,
             cxxtrace::time_point end_timestamp) -> void;
}

#endif
