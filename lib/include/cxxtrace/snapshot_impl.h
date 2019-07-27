#ifndef CXXTRACE_SNAPSHOT_IMPL_H
#define CXXTRACE_SNAPSHOT_IMPL_H

#if !defined(CXXTRACE_SNAPSHOT_H)
#error                                                                         \
  "Include <cxxtrace/snapshot.h> instead of including <cxxtrace/snapshot_impl.h> directly."
#endif

#include <algorithm>
#include <cassert>
#include <cxxtrace/detail/sample.h>
#include <cxxtrace/detail/snapshot_sample.h>
#include <cxxtrace/uninitialized.h>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace cxxtrace {
template<class Storage, class Clock>
auto
take_all_samples(Storage& storage, Clock& clock) noexcept(false)
  -> samples_snapshot
{
  auto raw_samples = storage.take_all_samples();
  auto samples = std::vector<detail::snapshot_sample>{};
  samples.reserve(raw_samples.size());
  std::transform(raw_samples.begin(),
                 raw_samples.end(),
                 std::back_inserter(samples),
                 [&](const detail::sample<typename Clock::sample>&
                       raw_sample) noexcept->detail::snapshot_sample {
                   return detail::snapshot_sample{ raw_sample, clock };
                 });
  return samples_snapshot{ std::move(samples) };
}
}

#endif
