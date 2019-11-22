#ifndef CXXTRACE_CHECK_RSEQ_MACHINE_CODE_H
#define CXXTRACE_CHECK_RSEQ_MACHINE_CODE_H

#include <cstdint>

namespace cxxtrace_check_rseq {
enum class machine_architecture
{
  x86,
};

using machine_address = std::uint64_t;
}

#endif
