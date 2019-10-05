#ifndef CXXTRACE_TEST_CDSCHECKER_THREAD_H
#define CXXTRACE_TEST_CDSCHECKER_THREAD_H

namespace cxxtrace_test {
using cdschecker_thread_id = int;

auto
cdschecker_current_thread_id() noexcept -> cdschecker_thread_id;
}

#endif
