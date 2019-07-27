#ifndef CXXTRACE_DETAIL_SNAPSHOT_SAMPLE_H
#define CXXTRACE_DETAIL_SNAPSHOT_SAMPLE_H

#include <algorithm>
#include <cxxtrace/clock.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <iterator>
#include <type_traits>
#include <vector>

namespace cxxtrace {
namespace detail {
struct snapshot_sample
{
  template<class Clock>
  explicit snapshot_sample(const sample<typename Clock::sample>& sample,
                           Clock& clock) noexcept
    : category{ sample.category }
    , name{ sample.name }
    , kind{ sample.kind }
    , thread_id{ sample.thread_id }
    , timestamp{ clock.make_time_point(sample.time_point) }
  {}

  template<class ForwardIt, class Clock>
  static auto many_from_samples(ForwardIt begin,
                                ForwardIt end,
                                Clock& clock) noexcept(false)
    -> std::vector<snapshot_sample>
  {
    static_assert(std::is_convertible_v<typename ForwardIt::reference,
                                        sample<typename Clock::sample>>);

    auto snapshot_samples = std::vector<detail::snapshot_sample>{};
    snapshot_samples.reserve(std::distance(begin, end));
    std::transform(begin,
                   end,
                   std::back_inserter(snapshot_samples),
                   [&](auto&& s) noexcept->detail::snapshot_sample {
                     return detail::snapshot_sample{ s, clock };
                   });
    return snapshot_samples;
  }

  czstring category;
  czstring name;
  sample_kind kind;
  thread_id thread_id;
  time_point timestamp;
};
}
}

#endif
