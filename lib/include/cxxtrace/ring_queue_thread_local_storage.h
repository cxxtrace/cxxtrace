#ifndef CXXTRACE_RING_QUEUE_THREAD_LOCAL_STORAGE_H
#define CXXTRACE_RING_QUEUE_THREAD_LOCAL_STORAGE_H

#include <cstddef>
#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <mutex>
#include <vector>

namespace cxxtrace {
class samples_snapshot;

template<std::size_t CapacityPerThread, class Tag, class ClockSample>
class ring_queue_thread_local_storage
{
public:
  static auto reset() noexcept -> void;

  static auto add_sample(czstring category,
                         czstring name,
                         sample_kind,
                         ClockSample time_point) noexcept -> void;
  template<class Clock>
  auto take_all_samples(Clock&) noexcept(false) -> samples_snapshot;
  auto remember_current_thread_name_for_next_snapshot() -> void;

private:
  struct thread_data;
  struct thread_local_sample;

  static auto get_thread_data() -> thread_data&;

  static auto add_to_thread_list(thread_data*) noexcept(false) -> void;
  static auto remove_from_thread_list(thread_data*) noexcept -> void;

  // See NOTE[ring_queue_thread_local_storage lock order].
  inline static std::mutex global_mutex{};
  inline static std::vector<thread_data*> thread_list{};
  inline static std::vector<detail::sample<ClockSample>> disowned_samples{};
  inline static detail::thread_name_set disowned_thread_names{};
};
}

#include <cxxtrace/ring_queue_thread_local_storage_impl.h>

#endif
