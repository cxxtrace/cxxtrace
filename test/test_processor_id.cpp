#include "test_processor_id.h"
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

using namespace std::literals::chrono_literals;

namespace cxxtrace_test {
TEST_P(test_processor_id,
       current_processor_id_is_no_greater_than_maximum_processor_id)
{
  if (!this->supported()) {
    std::cerr << "warning: this processor ID lookup method is not supported. "
                 "skipping test...\n";
    return;
  }

  // TODO(strager): Investigate CPU hot-plugging.
  auto maximum_processor_id = cxxtrace::detail::get_maximum_processor_id();
  std::fprintf(stderr,
               "maximum_processor_id = %ju\n",
               std::uintmax_t{ maximum_processor_id });

  auto check = [maximum_processor_id, this] {
    for (auto i = 0; i < 100; ++i) {
      EXPECT_LE(this->get_current_processor_id(), maximum_processor_id);
    }
  };
  auto thread_routine = [&check] {
    // TODO(strager): Use a semaphore to make all threads start at the same
    // time.
    check();
    std::this_thread::sleep_for(100ms);
    check();
  };

  // Create more threads than processors in an attempt to get all processors to
  // schedule our threads.
  auto thread_count = std::size_t{ maximum_processor_id } * 2;
  auto threads = std::vector<std::thread>{};
  for (auto i = std::size_t{ 0 }; i < thread_count; ++i) {
    threads.emplace_back(thread_routine);
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

INSTANTIATE_TEST_CASE_P(,
                        test_processor_id,
                        test_processor_id::param_values,
                        test_processor_id::param_name);
}
