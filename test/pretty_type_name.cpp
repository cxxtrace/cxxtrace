#include "pretty_type_name.h"
#include <cstddef>
#include <cxxtrace/detail/have.h>
#include <cxxtrace/string.h>
#include <string>
#include <string_view>
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

auto
strip_all(std::string_view string_to_strip, std::string& out)
{
  auto i = std::size_t{ 0 };
  for (;;) {
    if (i >= out.size()) {
      return;
    }
    auto strip_index = out.find(string_to_strip, i);
    if (strip_index == out.npos) {
      return;
    }
    out.erase(strip_index, string_to_strip.size());
    i = strip_index;
  }
}
}

namespace cxxtrace_test {
auto
pretty_type_name(const std::type_info& type) -> std::string
{
  const auto* type_name = cxxtrace::czstring{ type.name() };
  auto result = std::string{};

#if CXXTRACE_HAVE_CXA_DEMANGLE
  auto status = int{};
  auto buffer = std::unique_ptr<char, free_deleter>{ abi::__cxa_demangle(
    type_name, nullptr, nullptr, &status) };
  if (buffer && status == 0) {
    result = std::string{ buffer.get() };
  } else {
    result = type_name;
  }
#else
  result = type_name;
#endif

  strip_all("cxxtrace::detail::", result);
  strip_all("cxxtrace_test::detail::", result);

  strip_all("cxxtrace::", result);
  strip_all("cxxtrace_test::", result);

  return result;
}
}
