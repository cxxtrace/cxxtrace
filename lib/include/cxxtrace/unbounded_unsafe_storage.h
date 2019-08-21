#ifndef CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_H
#define CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_H

#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>
#include <vector>

namespace cxxtrace {
class samples_snapshot;
enum class sample_kind;

namespace detail {
template<class ClockSample>
struct sample;
}

template<class ClockSample>
class unbounded_unsafe_storage
{
public:
  explicit unbounded_unsafe_storage() noexcept;
  ~unbounded_unsafe_storage() noexcept;

  unbounded_unsafe_storage(const unbounded_unsafe_storage&) = delete;
  unbounded_unsafe_storage& operator=(const unbounded_unsafe_storage&) = delete;
  unbounded_unsafe_storage(unbounded_unsafe_storage&&) = delete;
  unbounded_unsafe_storage& operator=(unbounded_unsafe_storage&&) = delete;

  auto reset() noexcept -> void;

  auto add_sample(detail::sample_site_local_data,
                  ClockSample time_point,
                  thread_id) noexcept(false) -> void;
  auto add_sample(detail::sample_site_local_data,
                  ClockSample time_point) noexcept(false) -> void;
  template<class Clock>
  auto take_all_samples(Clock&) noexcept(false) -> samples_snapshot;
  auto remember_current_thread_name_for_next_snapshot() -> void;

private:
  using sample = detail::global_sample<ClockSample>;

  std::vector<sample> samples;
  detail::thread_name_set remembered_thread_names;
};
}

#include <cxxtrace/unbounded_unsafe_storage_impl.h>

#endif
