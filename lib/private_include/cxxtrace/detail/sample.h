#ifndef CXXTRACE_DETAIL_SAMPLE_H
#define CXXTRACE_DETAIL_SAMPLE_H

#include <cxxtrace/string.h>
#include <vector>

namespace cxxtrace {
namespace detail {
enum class sample_kind
{
  enter_span,
  exit_span,
};

struct sample
{
  czstring category;
  czstring name;
  sample_kind kind;
};

extern std::vector<sample> g_samples;
}
}

#endif
