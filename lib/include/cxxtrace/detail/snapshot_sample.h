#ifndef CXXTRACE_DETAIL_SNAPSHOT_SAMPLE_H
#define CXXTRACE_DETAIL_SNAPSHOT_SAMPLE_H

#include <algorithm> // IWYU pragma: keep
#include <cxxtrace/clock.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/thread.h>
#include <iterator>
#include <type_traits> // IWYU pragma: keep
#include <vector>

namespace cxxtrace {
namespace detail {
struct snapshot_sample
{
  template<class Clock>
  explicit snapshot_sample(const global_sample<typename Clock::sample>& sample,
                           Clock& clock) noexcept
    : site{ sample.site }
    , thread_id{ sample.thread_id }
    , timestamp{ clock.make_time_point(sample.time_point) }
  {}

  template<class Clock>
  explicit snapshot_sample(
    const thread_local_sample<typename Clock::sample>& sample,
    thread_id thread_id,
    Clock& clock) noexcept
    : site{ sample.site }
    , thread_id{ thread_id }
    , timestamp{ clock.make_time_point(sample.time_point) }
  {}

  template<class Sample, class Clock>
  static auto many_from_samples(const std::vector<Sample>& samples,
                                Clock& clock) noexcept(false)
    -> std::vector<snapshot_sample>
  {
    auto snapshot_samples = std::vector<detail::snapshot_sample>{};
    many_from_samples(samples, clock, snapshot_samples);
    return snapshot_samples;
  }

  template<class Sample, class Clock>
  static auto many_from_samples(
    const std::vector<Sample>& samples,
    Clock& clock,
    std::vector<snapshot_sample>& out) noexcept(false) -> void
  {
    static_assert(
      std::is_convertible_v<Sample, global_sample<typename Clock::sample>>);

    out.reserve(out.size() + samples.size());
    std::transform(
      samples.begin(),
      samples.end(),
      std::back_inserter(out),
      [&](auto&& s) noexcept->detail::snapshot_sample {
        return detail::snapshot_sample{ s, clock };
      });
  }

  sample_site_local_data site;
  cxxtrace::thread_id thread_id;
  time_point timestamp;
};
}
}

#endif
