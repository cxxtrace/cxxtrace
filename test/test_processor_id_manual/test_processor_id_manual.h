#ifndef CXXTRACE_TEST_PROCESSOR_ID_MANUAL_H
#define CXXTRACE_TEST_PROCESSOR_ID_MANUAL_H

#include "event.h"
#include <atomic>
#include <cstdint>
#include <cxxtrace/clock.h>
#include <cxxtrace/detail/processor.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <deque>
#include <dtrace.h>
#include <iosfwd>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace cxxtrace_test {
using dtrace_clock = cxxtrace::apple_absolute_time_clock;
using timestamp = dtrace_clock::sample;

struct thread_executions
{
  using processor_id = cxxtrace::detail::processor_id;

  struct thread_execution
  {
    timestamp begin_timestamp;
    timestamp end_timestamp;
    cxxtrace::thread_id thread_id;
  };

  std::unordered_map<processor_id, std::vector<thread_execution>> by_processor;
};

struct processor_id_samples
{
  struct sample
  {
    timestamp timestamp_before;
    timestamp timestamp_after;
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

class thread_schedule_tracer;

class dtrace_client
{
public:
  explicit dtrace_client(thread_schedule_tracer*) noexcept;

  dtrace_client(const dtrace_client&) = delete;
  dtrace_client& operator=(const dtrace_client&) = delete;

  auto initialize() -> void;
  auto start() -> void;
  auto stop() -> void;

private:
  auto run() -> void;

  [[noreturn]] auto fatal_error(cxxtrace::czstring message, int error) -> void;
  [[noreturn]] auto fatal_error(cxxtrace::czstring message) -> void;

  thread_schedule_tracer* program;
  dtrace_hdl_t* dtrace{ nullptr };
  std::thread thread;

  event worker_started;
  std::atomic<bool> should_stop{ false };
};

class thread_schedule_tracer
{
public:
  struct schedule_event
  {
    timestamp timestamp;
    cxxtrace::thread_id thread_id;
    bool on_cpu;
  };

  explicit thread_schedule_tracer(::pid_t);

  auto program_text() const noexcept -> cxxtrace::czstring;

  auto on_probe(const dtrace_probedata_t&) noexcept -> int;

  auto get_thread_executions() const -> thread_executions;

private:
  enum class probe_kind : std::uint8_t
  {
    unknown = 0,
    on_cpu = 1,
    off_cpu = 2,
  };

  struct probe_description
  {
    ::dtrace_epid_t epid;
    probe_kind kind;

    std::uint8_t machtimestamp_offset;
    std::uint8_t tid_offset;

    auto read_machtimestamp(const char* record_data) noexcept -> timestamp;
    auto read_tid(const char* record_data) noexcept -> cxxtrace::thread_id;
  };

  auto get_probe_description(::dtrace_epid_t epid,
                             const ::dtrace_probedesc& probe,
                             const ::dtrace_eprobedesc& eprobe)
    -> probe_description;

  static auto get_probe_description_uncached(::dtrace_epid_t epid,
                                             const ::dtrace_probedesc& probe,
                                             const ::dtrace_eprobedesc& eprobe)
    -> probe_description;

  static auto kind_of_probe(const dtrace_probedesc& probe) noexcept
    -> probe_kind;

  std::string program_text_;
  std::vector<probe_description> descriptions_;
  std::unordered_map<cxxtrace::detail::processor_id, std::deque<schedule_event>>
    events_by_processor_;
};

auto
dump_gnuplot(std::ostream&,
             const thread_executions&,
             const processor_id_samples&,
             timestamp begin_timestamp,
             timestamp end_timestamp) -> void;
}

#endif
