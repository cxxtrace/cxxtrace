#ifndef CXXTRACE_DETAIL_SAMPLE_H
#define CXXTRACE_DETAIL_SAMPLE_H

#include <cxxtrace/sample.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>

namespace cxxtrace {
namespace detail {
struct sample_site_local_data
{
  czstring category;
  czstring name;
  sample_kind kind;
};

template<class ClockSample>
struct global_sample
{
  sample_site_local_data site;
  thread_id thread_id;
  ClockSample time_point;
};

template<class ClockSample>
struct thread_local_sample
{
  sample_site_local_data site;
  ClockSample time_point;
};
}
}

#endif
