#include <cxxtrace/chrome_trace_event_format.h>
#include <cxxtrace/clock.h>
#include <cxxtrace/config.h>
#include <cxxtrace/ring_queue_storage.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/span.h>
#include <iostream>
// IWYU pragma: no_include <vector>

int
main()
{
  using clock_type = cxxtrace::apple_absolute_time_clock;
  auto clock = clock_type{};
  auto storage = cxxtrace::ring_queue_storage<1024, clock_type::sample>{};
  auto config = cxxtrace::basic_config{ storage, clock };

  {
    auto span = CXXTRACE_SPAN_WITH_CONFIG(config, "example", "main");
    auto span2 = CXXTRACE_SPAN_WITH_CONFIG(config, "example", "inner");
  }

  auto& output = std::cout;
  auto writer = cxxtrace::chrome_trace_event_writer{ &output };
  writer.write_snapshot(storage.take_all_samples(clock));
  writer.close();

  return 0;
}
