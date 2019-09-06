#include "test_processor_id_manual.h"
#include <algorithm>
#include <cstddef>
#include <cxxtrace/detail/thread.h>
#include <cxxtrace/thread.h>
#include <iostream>
#include <ostream>
#include <set>
#include <vector>

using cxxtrace::thread_id;
using cxxtrace::detail::processor_id;

namespace cxxtrace_test {
auto
dump_gnuplot(std::ostream& output,
             const thread_executions& thread_executions,
             const processor_id_samples& processor_id_samples,
             thread_schedule_tracer::timestamp begin_timestamp,
             thread_schedule_tracer::timestamp end_timestamp) -> void
{
  output << R"(# Feed this gnuplot script into gnuplot:
# $ gnuplot -e 'set term png size 1920, 1080 truecolor; set output "plot.png"' plot.gpl

unset colorbox
set xlabel "Time"
set ylabel "Processor"

x_format = "%.3f μs"
x_scale = 1.0 / 1000

processor_height = 4
spacing_between_processors = 1

rainbow_r(x) = clamp(floor(abs(2*x - 0.5) * 255), 0, 255)
rainbow_g(x) = clamp(floor(sin(pi*x) * 255), 0, 255)
rainbow_b(x) = clamp(floor(cos(0.5*pi*x) * 255), 0, 255)
rainbow_rgb(x) = rainbow_r(x)*256*256 + rainbow_g(x)*256 + rainbow_b(x)

min(x, y) = x < y ? x : y
max(x, y) = x > y ? x : y
clamp(x, min_x, max_x) = min(max(x, min_x), max_x)

processor_y(processor_index) = processor_index * (processor_height + spacing_between_processors) + processor_height/2
thread_color(thread_index) = rainbow_rgb(1.0 * thread_index / (thread_count - 1))
thread_x(begin_timestamp, end_timestamp) = (begin_timestamp+end_timestamp)/2 * x_scale
thread_x_delta(begin_timestamp, end_timestamp) = (end_timestamp-begin_timestamp)/2 * x_scale
thread_y(processor_index, thread_index) = processor_y(processor_index) + (0.5 * thread_index / thread_count)
)";

  auto thread_id_set = std::set<thread_id>{};
  auto processor_id_set = std::set<processor_id>{};
  for (const auto& [processor_id, processor_executions] :
       thread_executions.by_processor) {
    processor_id_set.insert(processor_id);
    for (const auto& execution : processor_executions) {
      thread_id_set.insert(execution.thread_id);
    }
  }
  for (const auto& sample : processor_id_samples.samples) {
    thread_id_set.insert(sample.thread_id);
    processor_id_set.insert(sample.processor_id);
  }
  auto thread_ids =
    std::vector<thread_id>{ thread_id_set.begin(), thread_id_set.end() };
  auto processor_ids = std::vector<processor_id>{ processor_id_set.begin(),
                                                  processor_id_set.end() };

  auto index_of = [](const auto& haystack, const auto& needle) -> std::size_t {
    auto it = std::find(haystack.begin(), haystack.end(), needle);
    assert(it != haystack.end());
    return it - haystack.begin();
  };
  auto clamped_timestamp = [&](thread_schedule_tracer::timestamp t) noexcept
                             ->thread_schedule_tracer::timestamp
  {
    if (t < begin_timestamp) {
      return 0;
    }
    if (t > end_timestamp) {
      return end_timestamp - begin_timestamp;
    }
    return t - begin_timestamp;
  };

  output << "processor_count = " << processor_ids.size() << "\n";
  output << "thread_count = " << thread_ids.size() << "\n";

  {
    output << "set ytics (";
    auto need_comma = false;
    for (auto processor_index = std::size_t{ 0 };
         processor_index < processor_ids.size();
         ++processor_index) {
      if (need_comma) {
        output << ", ";
      }
      output << "\"#" << processor_ids[processor_index] << "\" (processor_y("
             << processor_index << "))";
      need_comma = true;
    }
    output << ")\n";
  }

  output << "set xrange [0:" << (end_timestamp - begin_timestamp)
         << " * x_scale]\n";
  output << "set yrange [0:processor_count * (processor_height + "
            "spacing_between_processors)]\n";

  output << R"(
$thread_executions << EOD
# begin_timestamp end_timestamp processor_index thread_index
)";
  for (const auto& [processor_id, processor_executions] :
       thread_executions.by_processor) {
    auto processor_index = index_of(processor_ids, processor_id);
    for (const auto& execution : processor_executions) {
      output << clamped_timestamp(execution.begin_timestamp) << " "
             << clamped_timestamp(execution.end_timestamp) << " "
             << processor_index << " "
             << index_of(thread_ids, execution.thread_id) << "\n";
    }
  }
  output << R"(EOD

$processor_id_samples << EOD
# timestamp_before timestamp_after processor_index thread_index
)";
  for (const auto& sample : processor_id_samples.samples) {
    output << clamped_timestamp(sample.timestamp_before) << " "
           << clamped_timestamp(sample.timestamp_after) << " "
           << index_of(processor_ids, sample.processor_id) << " "
           << index_of(thread_ids, sample.thread_id) << "\n";
  }
  output << R"(EOD

set format x x_format
plot \
  $thread_executions \
    using \
      (thread_x($1, $2)) : \
      (thread_y($3, $4)) : \
      (thread_x_delta($1, $2)) : \
      (processor_height/2) : \
      (thread_color($4)) \
    with boxxyerror \
      fillcolor rgbcolor variable \
      fillstyle transparent solid 0.3 noborder \
    notitle, \
  $processor_id_samples \
    using \
      (thread_x($1, $2)) : \
      (thread_y($3, $4)) : \
      (thread_x_delta($1, $2)) : \
      (thread_color($4)) \
    with xerrorbars \
      linecolor rgbcolor variable \
      pointtype 0 \
    notitle;
)";
}
}
