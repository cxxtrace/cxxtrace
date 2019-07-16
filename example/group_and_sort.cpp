#include <algorithm>
#include <cxxtrace/chrome_trace_event_format.h>
#include <cxxtrace/clock.h>
#include <cxxtrace/config.h>
#include <cxxtrace/ring_queue_storage.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/span.h>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace example {
using group_key = std::string::size_type;

auto
main() -> void;
auto
read_lines(std::istream&) -> std::vector<std::string>;
auto group_lines(std::vector<std::string>)
  -> std::map<group_key, std::vector<std::string>>;
auto
sort_grouped_lines(std::map<group_key, std::vector<std::string>>&) noexcept
  -> void;
auto
dump_trace() -> void;

using clock_type = cxxtrace::apple_absolute_time_clock;
clock_type clock = clock_type{};
cxxtrace::ring_queue_storage<1024, clock_type::sample> storage =
  cxxtrace::ring_queue_storage<1024, clock_type::sample>{};
auto config = cxxtrace::basic_config{ storage, clock };

auto
main() -> void
{
  auto lines = read_lines(std::cin);
  auto grouped_lines = group_lines(std::move(lines));
  sort_grouped_lines(grouped_lines);
  dump_trace();
}

auto
read_lines(std::istream& input) -> std::vector<std::string>
{
  auto span = CXXTRACE_SPAN_WITH_CONFIG(config, "example", __func__);
  auto lines = std::vector<std::string>{};
  while (!input.eof()) {
    std::string line;
    std::getline(input, line);
    if (!input) {
      if (input.eof() && line.empty()) {
        break;
      }
      std::cerr << "error: reading input failed\n";
      break;
    }
    lines.emplace_back(std::move(line));
  }
  return lines;
}

auto
group_lines(std::vector<std::string> lines)
  -> std::map<group_key, std::vector<std::string>>
{
  auto span = CXXTRACE_SPAN_WITH_CONFIG(config, "example", __func__);
  auto groups = std::map<group_key, std::vector<std::string>>{};
  for (auto& line : lines) {
    auto key = group_key{ line.size() / 8 };
    groups[key].emplace_back(std::move(line));
  }
  return groups;
}

auto
sort_grouped_lines(
  std::map<group_key, std::vector<std::string>>& groups) noexcept -> void
{
  auto function_span = CXXTRACE_SPAN_WITH_CONFIG(config, "example", __func__);
  for (auto& [key, lines] : groups) {
    auto group_span =
      CXXTRACE_SPAN_WITH_CONFIG(config, "example", "sort group");
    std::sort(lines.begin(),
              lines.end(),
              [](const std::string& x, const std::string& y) -> bool {
                auto x_sorted = x;
                std::sort(x_sorted.begin(), x_sorted.end());
                auto y_sorted = y;
                std::sort(y_sorted.begin(), y_sorted.end());
                return x < y;
              });
  }
}

auto
dump_trace() -> void
{
  auto& output = std::cout;
  auto writer = cxxtrace::chrome_trace_event_writer{ &output };
  writer.write_snapshot(cxxtrace::take_all_samples(storage, clock));
  writer.close();
}
}

int
main()
{
  example::main();
  return 0;
}
