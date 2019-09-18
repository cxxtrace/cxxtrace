#include "pretty_type_name.h"
#include <cxxtrace/detail/have.h>
#include <cxxtrace/string.h>
#include <string>
#include <typeinfo>

#if CXXTRACE_HAVE_CXA_DEMANGLE
#include <cstdlib>
#include <cxxabi.h>
#include <memory>
#endif

namespace {
#if CXXTRACE_HAVE_CXA_DEMANGLE
struct free_deleter
{
  auto operator()(void* p) const noexcept -> void { std::free(p); }
};
#endif
}

namespace cxxtrace_test {
auto
pretty_type_name(const std::type_info& type) -> std::string
{
  const auto* type_name = cxxtrace::czstring{ type.name() };

#if CXXTRACE_HAVE_CXA_DEMANGLE
  auto status = int{};
  auto buffer = std::unique_ptr<char, free_deleter>{ abi::__cxa_demangle(
    type_name, nullptr, nullptr, &status) };
  if (buffer && status == 0) {
    return std::string{ buffer.get() };
  }
#endif

  return type_name;
}
}
