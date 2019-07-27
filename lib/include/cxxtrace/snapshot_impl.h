#ifndef CXXTRACE_SNAPSHOT_IMPL_H
#define CXXTRACE_SNAPSHOT_IMPL_H

#if !defined(CXXTRACE_SNAPSHOT_H)
#error                                                                         \
  "Include <cxxtrace/snapshot.h> instead of including <cxxtrace/snapshot_impl.h> directly."
#endif

#include <cxxtrace/detail/snapshot_sample.h>

namespace cxxtrace {
// TODO(strager): Inline this function.
template<class Storage, class Clock>
auto
take_all_samples(Storage& storage, Clock& clock) noexcept(false)
  -> samples_snapshot
{
  return samples_snapshot{ storage.take_all_samples(clock) };
}
}

#endif
