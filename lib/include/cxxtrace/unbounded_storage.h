#ifndef CXXTRACE_UNBOUNDED_STORAGE_H
#define CXXTRACE_UNBOUNDED_STORAGE_H

#include <cxxtrace/thread.h>
#include <cxxtrace/unbounded_unsafe_storage.h>
#include <mutex>

namespace cxxtrace {
class samples_snapshot;
enum class sample_kind;

namespace detail {
template<class ClockSample>
struct sample;
}

template<class ClockSample>
class unbounded_storage
{
public:
  explicit unbounded_storage() noexcept;
  ~unbounded_storage() noexcept;

  unbounded_storage(const unbounded_storage&) = delete;
  unbounded_storage& operator=(const unbounded_storage&) = delete;
  unbounded_storage(unbounded_storage&&) = delete;
  unbounded_storage& operator=(unbounded_storage&&) = delete;

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
  std::mutex mutex{};
  unbounded_unsafe_storage<ClockSample> storage{};
};
}

#include <cxxtrace/unbounded_storage_impl.h> // IWYU pragma: export

#endif
