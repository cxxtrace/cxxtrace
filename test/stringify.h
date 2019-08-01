#ifndef CXXTRACE_STRINGIFY_H
#define CXXTRACE_STRINGIFY_H

#include <string>

namespace cxxtrace_test {
template<class... Args>
auto
stringify(Args&&...) -> std::string;
}

#include "stringify_impl.h"

#endif
