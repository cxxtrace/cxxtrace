#include "event.h"
#include "stringify.h"
#include "test_processor_id_manual.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cxxtrace/clock.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <deque>
#include <dtrace.h>
#include <limits>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

using apple_absolute_time_sample = cxxtrace::apple_absolute_time_clock::sample;

namespace cxxtrace_test {
class thread_schedule_dtrace_program;

class dtrace_client
{
public:
  explicit dtrace_client(thread_schedule_dtrace_program*) noexcept;

  dtrace_client(const dtrace_client&) = delete;
  dtrace_client& operator=(const dtrace_client&) = delete;

  auto initialize() -> void;
  auto start() -> void;
  auto stop() -> void;

private:
  auto run() -> void;

  [[noreturn]] auto fatal_error(cxxtrace::czstring message, int error) -> void;
  [[noreturn]] auto fatal_error(cxxtrace::czstring message) -> void;

  thread_schedule_dtrace_program* program;
  dtrace_hdl_t* dtrace{ nullptr };
  std::thread thread;

  event worker_started;
  std::atomic<bool> should_stop{ false };
};

class thread_schedule_dtrace_program
{
public:
  struct schedule_event
  {
    apple_absolute_time_sample raw_timestamp;
    cxxtrace::thread_id thread_id;
    bool on_cpu;
  };

  explicit thread_schedule_dtrace_program(::pid_t);

  auto program_text() const noexcept -> cxxtrace::czstring;

  auto on_probe(const dtrace_probedata_t&) noexcept -> int;

  auto get_thread_executions(thread_schedule_tracer::clock&) const
    -> thread_executions;

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

    auto read_machtimestamp(const char* record_data) noexcept
      -> apple_absolute_time_sample;
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

dtrace_client::dtrace_client(thread_schedule_dtrace_program* program) noexcept
  : program{ program }
{}

auto
dtrace_client::initialize() -> void
{
  assert(!this->dtrace);

  auto error = int{};
  this->dtrace = ::dtrace_open(DTRACE_VERSION, DTRACE_O_LP64, &error);
  if (!this->dtrace) {
    this->fatal_error("dtrace_open failed", error);
  }

  // TODO(strager): Set up an error handler with ::dtrace_handle_err.
  // TODO(strager): Set up a drop handler with ::dtrace_handle_drop.

  if (::dtrace_setopt(this->dtrace, "bufsize", "4m") == -1) {
    this->fatal_error("dtrace_setopt(bufsize) failed");
  }
  if (::dtrace_setopt(this->dtrace, "switchrate", "100ms") == -1) {
    this->fatal_error("dtrace_setopt(switchrate) failed");
  }

  auto* program = ::dtrace_program_strcompile(this->dtrace,
                                              this->program->program_text(),
                                              DTRACE_PROBESPEC_NAME,
                                              DTRACE_C_PSPEC,
                                              0,
                                              nullptr);
  if (!program) {
    this->fatal_error("dtrace_program_strcompile failed");
  }

  auto program_info = ::dtrace_proginfo_t{};
  if (::dtrace_program_exec(this->dtrace, program, &program_info) == -1) {
    this->fatal_error("dtrace_program_exec failed");
  }
}

auto
dtrace_client::start() -> void
{
  assert(!this->thread.joinable());
  this->thread = std::thread{ [this] { this->run(); } };
  this->worker_started.wait();
}

auto
dtrace_client::stop() -> void
{
  this->should_stop.store(true);
  this->thread.join();
}

auto
dtrace_client::run() -> void
{
  auto on_probe =
    [](const ::dtrace_probedata_t* data, void* opaque) noexcept->int
  {
    auto program = static_cast<thread_schedule_dtrace_program*>(opaque);
    return program->on_probe(*data);
  };

  if (::dtrace_go(this->dtrace) == -1) {
    this->fatal_error("dtrace_go failed");
  }
  this->worker_started.set();

  auto stopped = false;
  for (;;) {
    if (!stopped) {
      ::dtrace_sleep(dtrace);
    }

    auto rc =
      ::dtrace_work(this->dtrace, stderr, on_probe, nullptr, this->program);
    switch (rc) {
      case DTRACE_WORKSTATUS_DONE:
        return;
      case DTRACE_WORKSTATUS_OKAY:
        break;
      case DTRACE_WORKSTATUS_ERROR:
        this->fatal_error("dtrace_work failed");
        break;
      default:
        std::fprintf(
          stderr, "warning: dtrace_work returned unexpected code: %#x\n", rc);
        break;
    }

    if (!stopped && this->should_stop.load()) {
      if (::dtrace_stop(this->dtrace) == -1) {
        this->fatal_error("dtrace_stop failed");
      }
      stopped = true;
    }
  }
}

[[noreturn]] auto
dtrace_client::fatal_error(cxxtrace::czstring message, int error) -> void
{
  std::fprintf(
    stderr, "fatal: %s: %s\n", message, ::dtrace_errmsg(this->dtrace, error));
  std::terminate();
}

[[noreturn]] auto
dtrace_client::fatal_error(cxxtrace::czstring message) -> void
{
  assert(this->dtrace);
  this->fatal_error(message, ::dtrace_errno(this->dtrace));
}

thread_schedule_dtrace_program::thread_schedule_dtrace_program(
  ::pid_t process_id)
{
  this->program_text_ = stringify("sched:::on-cpu, sched:::off-cpu\n"
                                  "/pid==",
                                  process_id,
                                  "/ {\n"
                                  "  printf(\"%d %d\", tid, machtimestamp);\n"
                                  "}\n");
}

auto
thread_schedule_dtrace_program::program_text() const noexcept
  -> cxxtrace::czstring
{
  return this->program_text_.c_str();
}

auto
thread_schedule_dtrace_program::on_probe(
  const dtrace_probedata_t& probe_data) noexcept -> int
{
  auto description =
    this->get_probe_description(probe_data.dtpda_edesc->dtepd_epid,
                                *probe_data.dtpda_pdesc,
                                *probe_data.dtpda_edesc);
  const auto* record = probe_data.dtpda_data;
  switch (description.kind) {
    case probe_kind::off_cpu:
    case probe_kind::on_cpu: {
      auto event = schedule_event{};
      event.raw_timestamp = description.read_machtimestamp(record);
      event.thread_id = description.read_tid(record);
      event.on_cpu = description.kind == probe_kind::on_cpu;
      this->events_by_processor_[probe_data.dtpda_cpu].push_back(event);
      break;
    }

    case probe_kind::unknown:
      break;
  }

  return DTRACE_CONSUME_NEXT;
}

auto
thread_schedule_dtrace_program::get_thread_executions(
  thread_schedule_tracer::clock& clock) const -> struct thread_executions
{
  auto executions = thread_executions{};
  for (const auto& [processor_id, events] : this->events_by_processor_) {
    auto& processor_executions = executions.by_processor[processor_id];
    const thread_schedule_dtrace_program::schedule_event* last_event = nullptr;
    for (const auto& event : events) {
      if (event.on_cpu) {
        if (last_event) {
          assert(!last_event->on_cpu);
        }
      } else {
        if (last_event) {
          assert(event.thread_id == last_event->thread_id);
          assert(last_event->on_cpu);
          assert(event.raw_timestamp >= last_event->raw_timestamp);

          processor_executions.emplace_back(thread_executions::thread_execution{
            clock.make_time_point(last_event->raw_timestamp),
            clock.make_time_point(event.raw_timestamp),
            event.thread_id });
        } else {
          // Ignore this off-cpu event. We don't have a corresponding on-cpu
          // event, so we can't determine begin_timestamp.
        }
      }
      last_event = &event;
    }
  }
  return executions;
}

auto
thread_schedule_dtrace_program::probe_description::read_machtimestamp(
  const char* record_data) noexcept -> apple_absolute_time_sample
{
  static_assert(std::is_same_v<thread_schedule_tracer::clock,
                               cxxtrace::apple_absolute_time_clock>);

  auto machtimestamp = apple_absolute_time_sample{};
  std::memcpy(&machtimestamp,
              record_data + this->machtimestamp_offset,
              sizeof(machtimestamp));
  return machtimestamp;
}

auto
thread_schedule_dtrace_program::probe_description::read_tid(
  const char* record_data) noexcept -> cxxtrace::thread_id
{
  auto tid = cxxtrace::thread_id{};
  std::memcpy(&tid, record_data + this->tid_offset, sizeof(tid));
  return tid;
}

auto
thread_schedule_dtrace_program::get_probe_description(
  ::dtrace_epid_t epid,
  const ::dtrace_probedesc& probe,
  const ::dtrace_eprobedesc& eprobe) -> probe_description
{
  auto it = std::find_if(
    this->descriptions_.begin(),
    this->descriptions_.end(),
    [&](const auto& description) { return description.epid == epid; });
  if (it != this->descriptions_.end()) {
    return *it;
  }

  auto description = this->get_probe_description_uncached(epid, probe, eprobe);
  this->descriptions_.push_back(description);
  return description;
}

auto
thread_schedule_dtrace_program::get_probe_description_uncached(
  ::dtrace_epid_t epid,
  const ::dtrace_probedesc& probe,
  const ::dtrace_eprobedesc& eprobe) -> probe_description
{
  auto description = probe_description{};
  description.epid = epid;
  description.kind = kind_of_probe(probe);
  switch (description.kind) {
    case probe_kind::off_cpu:
    case probe_kind::on_cpu: {
      auto tid_arg_index = -1;
      auto machtimestamp_arg_index = -1;
      for (auto arg_index = 0; arg_index < eprobe.dtepd_nrecs; ++arg_index) {
        const auto& arg_record = eprobe.dtepd_rec[arg_index];
        if (arg_record.dtrd_action == DTRACEACT_PRINTF) {
          if (tid_arg_index == -1) {
            tid_arg_index = arg_index;
          } else if (machtimestamp_arg_index == -1) {
            machtimestamp_arg_index = arg_index;
          } else {
            std::fprintf(stderr, "warning: unexpected printf argument\n");
          }
        }
      }

      using offset_type = decltype(description.tid_offset);
      using offset_type_limits = std::numeric_limits<offset_type>;

      if (tid_arg_index == -1) {
        std::fprintf(stderr, "fatal: missing tid printf argument\n");
        std::terminate();
      }
      auto tid_offset = eprobe.dtepd_rec[tid_arg_index].dtrd_offset;
      if (!(offset_type_limits::lowest() <= tid_offset &&
            tid_offset <= offset_type_limits::max())) {
        std::fprintf(stderr,
                     "fatal: tid printf argument has unexpected out-of-range "
                     "offset: %ju\n",
                     std::uintmax_t{ tid_offset });
        std::terminate();
      }
      description.tid_offset = static_cast<offset_type>(tid_offset);

      if (machtimestamp_arg_index == -1) {
        std::fprintf(stderr, "fatal: missing machtimestamp printf argument\n");
        std::terminate();
      }
      auto machtimestamp_offset =
        eprobe.dtepd_rec[machtimestamp_arg_index].dtrd_offset;
      if (!(offset_type_limits::lowest() <= machtimestamp_offset &&
            machtimestamp_offset <= offset_type_limits::max())) {
        std::fprintf(stderr,
                     "fatal: machtimestamp printf argument has unexpected "
                     "out-of-range offset: %ju\n",
                     std::uintmax_t{ machtimestamp_offset });
        std::terminate();
      }
      description.machtimestamp_offset =
        static_cast<offset_type>(machtimestamp_offset);

      break;
    }
    default:
      break;
  }
  return description;
}

auto
thread_schedule_dtrace_program::kind_of_probe(
  const dtrace_probedesc& probe) noexcept -> probe_kind
{
  const char(&provider)[DTRACE_PROVNAMELEN] = probe.dtpd_provider;
  const char(&name)[DTRACE_NAMELEN] = probe.dtpd_name;
  if (std::strncmp(provider, "sched", sizeof(provider)) == 0) {
    if (std::strncmp(name, "on-cpu", sizeof(name)) == 0) {
      return probe_kind::on_cpu;
    }
    if (std::strncmp(name, "off-cpu", sizeof(name)) == 0) {
      return probe_kind::off_cpu;
    }
  }
  return probe_kind::unknown;
}

struct thread_schedule_tracer::impl
{
  explicit impl(::pid_t process_id, thread_schedule_tracer::clock* clock)
    : program{ process_id }
    , dtrace{ &this->program }
    , clock{ clock }
  {}

  thread_schedule_dtrace_program program;
  dtrace_client dtrace;
  thread_schedule_tracer::clock* clock;
};

thread_schedule_tracer::thread_schedule_tracer(
  ::pid_t process_id,
  thread_schedule_tracer::clock* clock,
  cxxtrace::detail::processor_id_namespace)
  : impl_{ std::make_unique<impl>(process_id, clock) }
{}

thread_schedule_tracer::~thread_schedule_tracer() = default;

auto
thread_schedule_tracer::initialize() -> void
{
  this->impl_->dtrace.initialize();
}

auto
thread_schedule_tracer::start() -> void
{
  this->impl_->dtrace.start();
}

auto
thread_schedule_tracer::get_thread_executions() const -> thread_executions
{
  return this->impl_->program.get_thread_executions(*this->impl_->clock);
}

auto
thread_schedule_tracer::stop() -> void
{
  this->impl_->dtrace.stop();
}
}
