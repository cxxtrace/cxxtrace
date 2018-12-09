#ifndef CXXTRACE_RING_QUEUE_THREAD_LOCAL_STORAGE_H
#define CXXTRACE_RING_QUEUE_THREAD_LOCAL_STORAGE_H

#include <cstddef>
#include <cxxtrace/detail/ring_queue.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <mutex>
#include <vector>

namespace cxxtrace {
template<std::size_t CapacityPerThread, class Tag, class ClockSample>
class ring_queue_thread_local_storage
{
public:
  static auto reset() noexcept -> void;

  static auto add_sample(czstring category,
                         czstring name,
                         detail::sample_kind,
                         ClockSample time_point) noexcept -> void;
  static auto take_all_samples() noexcept(false)
    -> std::vector<detail::sample<ClockSample>>;

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
};
}

#include <cxxtrace/ring_queue_thread_local_storage_impl.h>

#endif
