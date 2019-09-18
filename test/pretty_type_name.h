#ifndef CXXTRACE_TEST_PRETTY_TYPE_NAME_H
#define CXXTRACE_TEST_PRETTY_TYPE_NAME_H

#include <string>
#include <typeinfo>

namespace cxxtrace_test {
auto
pretty_type_name(const std::type_info&) -> std::string;
}

#endif
