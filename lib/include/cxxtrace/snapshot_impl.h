#ifndef CXXTRACE_SNAPSHOT_IMPL_H
#define CXXTRACE_SNAPSHOT_IMPL_H

#if !defined(CXXTRACE_SNAPSHOT_H)
#error                                                                         \
  "Include <cxxtrace/snapshot.h> instead of including <cxxtrace/snapshot_impl.h> directly."
#endif

#include <cxxtrace/detail/snapshot_sample.h>

namespace cxxtrace {
template<class Config>
auto
remember_current_thread_name_for_next_snapshot(Config& config) -> void
{
  config.storage().remember_current_thread_name_for_next_snapshot();
}
}

#endif
