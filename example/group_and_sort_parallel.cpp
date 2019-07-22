#include <algorithm>
#include <cxxtrace/chrome_trace_event_format.h>
#include <cxxtrace/clock.h>
#include <cxxtrace/config.h>
#include <cxxtrace/ring_queue_storage.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/span.h>
#include <deque>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>
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
sort_grouped_lines_in_parallel(std::map<group_key, std::vector<std::string>>&)
  -> void;
auto
sort_grouped_lines(std::vector<std::string>* const* begin,
                   std::vector<std::string>* const* end) noexcept -> void;
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
  sort_grouped_lines_in_parallel(grouped_lines);
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
sort_grouped_lines_in_parallel(
  std::map<group_key, std::vector<std::string>>& groups) -> void
{
  auto function_span = CXXTRACE_SPAN_WITH_CONFIG(config, "example", __func__);

  auto number_of_threads = 4;
  auto groups_per_thread =
    (groups.size() + number_of_threads - 1) / number_of_threads;

  auto group_vectors = std::vector<std::vector<std::string>*>{};
  std::transform(groups.begin(),
                 groups.end(),
                 std::back_inserter(group_vectors),
                 [](std::pair<const group_key, std::vector<std::string>> &
                    entry) noexcept->std::vector<std::string> *
                   { return &entry.second; });
  auto rng = std::mt19937{};
  std::shuffle(group_vectors.begin(), group_vectors.end(), rng);

  auto threads = std::vector<std::thread>{};
  threads.reserve(number_of_threads);
  for (auto i = std::size_t{ 0 }; i < group_vectors.size();
       i += groups_per_thread) {
    auto thread_begin_index = i;
    auto thread_end_index =
      std::min(thread_begin_index + groups_per_thread, groups.size());
    threads.emplace_back(sort_grouped_lines,
                         &group_vectors[thread_begin_index],
                         &group_vectors[thread_end_index]);
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

auto
sort_grouped_lines(std::vector<std::string>* const* begin,
                   std::vector<std::string>* const* end) noexcept -> void
{
  auto function_span = CXXTRACE_SPAN_WITH_CONFIG(config, "example", __func__);
  for (auto it = begin; it != end; ++it) {
    auto& lines = **it;
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
