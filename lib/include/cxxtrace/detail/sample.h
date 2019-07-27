#ifndef CXXTRACE_DETAIL_SAMPLE_H
#define CXXTRACE_DETAIL_SAMPLE_H

#include <cxxtrace/sample.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>

namespace cxxtrace {
namespace detail {
template<class ClockSample>
struct sample
{
  czstring category;
  czstring name;
  sample_kind kind;
  thread_id thread_id;
  ClockSample time_point;
};
}
}

#endif
