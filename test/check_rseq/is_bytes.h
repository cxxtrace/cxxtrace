#ifndef CXXTRACE_TEST_IS_BYTES_H
#define CXXTRACE_TEST_IS_BYTES_H

#include <cstddef>
#include <gmock/gmock.h>
#include <initializer_list>

namespace cxxtrace_test {
template<class... Args>
auto
is_bytes(Args... bytes)
{
  return testing::ElementsAreArray(
    std::initializer_list<std::byte>{ std::byte(bytes)... });
}
}

#endif
