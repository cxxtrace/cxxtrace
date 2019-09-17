#include "atomic_ref.h"
#include "linux_proc_cpuinfo.h"
#include "mmap_mapping.h"
#include "test_processor_id_manual.h"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cxxtrace/detail/file_descriptor.h>
#include <cxxtrace/detail/processor.h>
#include <iomanip>
#include <iostream>
#include <linux/perf_event.h>
#include <memory>
#include <optional>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unordered_map>

// Terminal code to reset previously-written SGR codes. Specifically, ECMA-048
// SGR with the "default rendition" parameter.
#define ECMA_048_SGR_DEFAULT "\x1b[0m"

// Terminal code to make text faint. Specifically, ECMA-048 SGR with the "faint,
// decreased intensity" parameter.
#define ECMA_048_SGR_FAINT "\x1b[2m"

namespace cxxtrace_test {
namespace {
constexpr auto debug_logging = true;

auto
perf_event_open(::perf_event_attr* hw_event_uptr,
                ::pid_t pid,
                int cpu,
                int group_fd,
                unsigned long flags) noexcept -> int
{
  return int(
    ::syscall(SYS_perf_event_open, hw_event_uptr, pid, cpu, group_fd, flags));
}
}

struct thread_schedule_tracer::impl
{
  using processor_number = linux_proc_cpuinfo::processor_number;

  explicit impl(::pid_t process_id,
                thread_schedule_tracer::clock* clock,
                cxxtrace::detail::processor_id_namespace processor_id_namespace)
    : process_id{ process_id }
    , clock{ clock }
    , processor_id_namespace{ processor_id_namespace }
  {}

  struct processor_tracer
  {
    explicit processor_tracer(processor_number processor_number)
    {
      auto event_attributes = ::perf_event_attr{
        .type = PERF_TYPE_SOFTWARE,
        .size = sizeof(::perf_event_attr),
        .config = PERF_COUNT_SW_DUMMY,
        .sample_period = 1,
        .sample_type = impl::sample_type,
        .read_format = 0,
        .disabled = true,
        .inherit = false,
        .exclusive = false,
        .exclude_user = false,
        .exclude_kernel = false,
        .exclude_hv = false,
        .exclude_idle = false,
        .comm = false,
        .enable_on_exec = false,
        .task = false,
        .sample_id_all = true,
        .exclude_host = true,
        .exclude_guest = true,
        .use_clockid = true,
        .context_switch = true,
        .write_backward = false,
        .namespaces = false,
        .clockid = clock_id,
      };
      // TODO(strager): If we track specific pids, can we call perf_event_open
      // and run test_processor_id_manual as non-root?
      auto pid = ::pid_t{ -1 };
      auto group_fd = -1;
      this->perf_event.reset(perf_event_open(&event_attributes,
                                             pid,
                                             processor_number,
                                             group_fd,
                                             PERF_FLAG_FD_CLOEXEC));
      if (!this->perf_event.valid()) {
        std::perror("fatal: failed to open Linux perf event");
        std::terminate();
      }

      auto header_page_count = 1;
      auto data_page_count = 1 << 4; // Arbitrary.
      // NOTE(strager): PROT_WRITE is important, even if we don't write any data
      // to ring_buffer_mapping. PROT_WRITE causes the kernel to drop writes
      // when the ring buffer overflows. This behavior is easier to program for
      // than the default behavior (without PROT_WRITE).
      this->ring_buffer_mapping = mmap_mapping::mmap_checked(
        nullptr,
        ::getpagesize() * (header_page_count + data_page_count),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        this->perf_event.get(),
        0);
      assert(this->ring_buffer_mapping.valid());
    }

    auto header() const noexcept -> ::perf_event_mmap_page*
    {
      return static_cast<::perf_event_mmap_page*>(
        this->ring_buffer_mapping.data());
    }

    auto data_at(std::size_t byte_index) const noexcept -> const std::byte*
    {
      const auto* header = this->header();
      assert(byte_index < header->data_size);
      const auto* base =
        static_cast<const std::byte*>(this->ring_buffer_mapping.data());
      return base + header->data_offset + byte_index;
    }

    auto enable() -> void
    {
      auto rc = ::ioctl(this->perf_event.get(), PERF_EVENT_IOC_ENABLE, 0);
      if (rc == -1) {
        std::perror("fatal: failed to enable Linux perf event");
        std::terminate();
      }
    }

    auto disable() -> void
    {
      auto rc = ::ioctl(this->perf_event.get(), PERF_EVENT_IOC_DISABLE, 0);
      if (rc == -1) {
        std::perror("fatal: failed to disable Linux perf event");
        std::terminate();
      }
    }

    cxxtrace::detail::file_descriptor perf_event{};
    mmap_mapping ring_buffer_mapping{};
  };

  static constexpr auto sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_TIME;
  struct sample_id
  {
    // PERF_SAMPLE_TID
    std::uint32_t pid;
    std::uint32_t tid;
    // PERF_SAMPLE_TIME
    std::uint64_t time;
  };

  auto get_processor_id_from_processor_number(processor_number number)
    -> cxxtrace::detail::processor_id
  {
    switch (this->processor_id_namespace) {
      case cxxtrace::detail::processor_id_namespace::linux_getcpu:
        return number;
      case cxxtrace::detail::processor_id_namespace::initial_apic_id:
        return this->cpuinfo.get_initial_apicid(number).value();
    }
    __builtin_unreachable();
  }

  ::pid_t process_id;
  thread_schedule_tracer::clock* clock;
  cxxtrace::detail::processor_id_namespace processor_id_namespace;

  linux_proc_cpuinfo cpuinfo{ linux_proc_cpuinfo::parse() };
  std::unordered_map<processor_number, processor_tracer> processor_tracers;
};

thread_schedule_tracer::thread_schedule_tracer(
  ::pid_t process_id,
  thread_schedule_tracer::clock* clock,
  cxxtrace::detail::processor_id_namespace processor_id_namespace)
  : impl_{ std::make_unique<impl>(process_id, clock, processor_id_namespace) }
{}

thread_schedule_tracer::~thread_schedule_tracer() = default;

auto
thread_schedule_tracer::initialize() -> void
{
  for (auto processor_number : this->impl_->cpuinfo.get_processor_numbers()) {
    auto [it, inserted] = this->impl_->processor_tracers.emplace(
      processor_number, processor_number);
    assert(inserted);
  }
}

auto
thread_schedule_tracer::start() -> void
{
  for (auto& [processor_number, processor_tracer] :
       this->impl_->processor_tracers) {
    processor_tracer.enable();
  }
}

auto
thread_schedule_tracer::get_thread_executions() const -> thread_executions
{
  struct perf_switch_cpu_wide_sample
  {
    ::perf_event_header header;
    std::uint32_t next_prev_pid;
    std::uint32_t next_prev_tid;
    impl::sample_id sample_id;
  };

  struct scheduler_event
  {
    bool on_cpu;
    ::pid_t process_id;
    cxxtrace::thread_id thread_id;
    cxxtrace::time_point timestamp;
  };

  auto process_id = this->impl_->process_id;
  auto& clock = *this->impl_->clock;

  auto executions = thread_executions{};
  for (const auto& [processor_number, processor_tracer] :
       this->impl_->processor_tracers) {
    auto processor_id =
      this->impl_->get_processor_id_from_processor_number(processor_number);

    auto decode_scheduler_event = [&](const ::perf_event_header* event_header)
      -> std::optional<scheduler_event> {
      switch (event_header->type) {
        case PERF_RECORD_SWITCH_CPU_WIDE: {
          const auto* s =
            reinterpret_cast<const perf_switch_cpu_wide_sample*>(event_header);
          auto off_cpu = event_header->misc & PERF_RECORD_MISC_SWITCH_OUT;
          if (debug_logging) {
            constexpr auto emphasize = "** ";
            constexpr auto deemphasize = ECMA_048_SGR_FAINT "   ";
            auto& out = std::clog;
            auto old_flags = out.setf(out.left, out.adjustfield);
            out << (s->sample_id.pid == process_id ? emphasize : deemphasize)
                << '@' << s->sample_id.time << ": #" << processor_number
                << "(id=" << processor_id << ") " << (off_cpu ? "off" : "on ")
                << "  pid=" << std::setw(8)
                << static_cast<::pid_t>(s->next_prev_pid)
                << "  tid=" << std::setw(8)
                << static_cast<::pid_t>(s->next_prev_tid)
                << "  s.pid=" << std::setw(8)
                << static_cast<::pid_t>(s->sample_id.pid)
                << "  s.tid=" << std::setw(8)
                << static_cast<::pid_t>(s->sample_id.tid) << '\n'
                << ECMA_048_SGR_DEFAULT;
            out.setf(old_flags);
          }
          return scheduler_event{
            !off_cpu,
            ::pid_t(s->sample_id.pid),
            cxxtrace::thread_id(s->sample_id.tid),
            cxxtrace::time_point{
              std::chrono::nanoseconds{ s->sample_id.time } },
          };
        }
        default:
          return std::nullopt;
      }
    };

    auto decode_scheduler_events = [&]() -> std::vector<scheduler_event> {
      auto* ring_buffer = processor_tracer.header();
      auto ring_buffer_size = ring_buffer->data_size;

      auto end_vindex = atomic_ref{ ring_buffer->data_head }.load();
      auto begin_vindex = end_vindex / ring_buffer_size * ring_buffer_size;
      assert(begin_vindex == 0 &&
             "PROT_WRITE should have prevented data from being overwritten");
      auto scheduler_events = std::vector<scheduler_event>{};
      for (auto vindex = begin_vindex; vindex < end_vindex;) {
        const auto* event_header = reinterpret_cast<const ::perf_event_header*>(
          processor_tracer.data_at(vindex % ring_buffer_size));
        if (event_header->size == 0) {
          std::fprintf(stderr, "warning: got a 0-size event; halting\n");
          break;
        }
        if (auto event = decode_scheduler_event(event_header)) {
          scheduler_events.emplace_back(*event);
        }
        assert((vindex + event_header->size) / ring_buffer_size ==
                 vindex / ring_buffer_size &&
               "Wrapping shouldn't happen");
        vindex += event_header->size;
      }
      return scheduler_events;
    };

    auto& processor_executions = executions.by_processor[processor_id];
    auto last_event = std::optional<scheduler_event>{};
    for (auto& event : decode_scheduler_events()) {
      if (event.on_cpu) {
        if (last_event) {
          assert(!last_event->on_cpu);
        }
      } else {
        if (last_event) {
          assert(last_event->on_cpu);
          assert(event.timestamp >= last_event->timestamp);

          auto thread_exited = event.thread_id == -1;
          if (!thread_exited) {
            assert(event.thread_id == last_event->thread_id);
          }
          auto thread_id = last_event->thread_id;
          if (last_event->process_id == process_id) {
            processor_executions.emplace_back(
              thread_executions::thread_execution{
                last_event->timestamp, event.timestamp, thread_id });
          }
        } else {
          // Ignore this off-cpu event. We don't have a corresponding on-cpu
          // event, so we can't determine begin_timestamp.
        }
      }
      last_event = event;
    }
  }

  return executions;
}

auto
thread_schedule_tracer::stop() -> void
{
  for (auto& [processor_number, processor_tracer] :
       this->impl_->processor_tracers) {
    processor_tracer.disable();
  }
}
}
