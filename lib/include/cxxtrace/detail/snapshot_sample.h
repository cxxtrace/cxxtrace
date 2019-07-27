#ifndef CXXTRACE_DETAIL_SNAPSHOT_SAMPLE_H
#define CXXTRACE_DETAIL_SNAPSHOT_SAMPLE_H

#include <cxxtrace/clock.h>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>

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

  czstring category;
  czstring name;
  sample_kind kind;
  thread_id thread_id;
  time_point timestamp;
};
}
}

#endif
