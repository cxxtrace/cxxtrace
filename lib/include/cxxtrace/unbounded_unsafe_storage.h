#ifndef CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_H
#define CXXTRACE_UNBOUNDED_UNSAFE_STORAGE_H

#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <vector>

namespace cxxtrace {
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

  auto add_sample(czstring category,
                  czstring name,
                  sample_kind,
                  ClockSample time_point,
                  thread_id) noexcept(false) -> void;
  auto add_sample(czstring category,
                  czstring name,
                  sample_kind,
                  ClockSample time_point) noexcept(false) -> void;
  template<class Clock>
  auto take_all_samples(Clock&) noexcept(false)
    -> std::vector<detail::snapshot_sample>;

private:
  std::vector<detail::sample<ClockSample>> samples;
};
}

#include <cxxtrace/unbounded_unsafe_storage_impl.h>

#endif
