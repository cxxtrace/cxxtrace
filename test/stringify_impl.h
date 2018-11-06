#ifndef CXXTRACE_STRINGIFY_IMPL_H
#define CXXTRACE_STRINGIFY_IMPL_H

#include <sstream>
#include <string>
#include <utility>

#if !defined(CXXTRACE_STRINGIFY_H)
#error                                                                         \
  "Include \"stringify.h\" instead of including \"stringify_impl.h\" directly."
#endif

namespace cxxtrace {
template<class... Args>
auto
stringify(Args&&... args) -> std::string
{
  std::ostringstream stream;
  (stream << ... << std::forward<Args>(args));
  return stream.str();
}
}

#endif