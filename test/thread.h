#ifndef CXXTRACE_TEST_THREAD_H
#define CXXTRACE_TEST_THREAD_H

#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <thread>

namespace cxxtrace_test {
auto
await_thread_exit(std::thread&, cxxtrace::thread_id) -> void;
auto set_current_thread_name(cxxtrace::czstring) -> void;
}

#endif
