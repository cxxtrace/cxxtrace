#include "cxxtrace_concurrency_test_base.h"
#include "pretty_type_name.h"
#include <string>

namespace cxxtrace_test {
namespace detail {
auto
concurrency_test::name() const -> std::string
{
  return pretty_type_name(typeid(*this));
}
}
}
