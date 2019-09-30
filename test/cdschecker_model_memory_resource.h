#ifndef CXXTRACE_TEST_CDSCHECKER_MODEL_MEMORY_RESOURCE_H
#define CXXTRACE_TEST_CDSCHECKER_MODEL_MEMORY_RESOURCE_H

#include "memory_resource_fwd.h"

namespace cxxtrace_test {
auto
get_cdschecker_model_memory_resource() noexcept
  -> std::experimental::pmr::memory_resource*;
}

#endif
