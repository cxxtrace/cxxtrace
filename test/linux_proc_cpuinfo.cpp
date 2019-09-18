#include "linux_proc_cpuinfo.h"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cxxtrace/string.h>
#include <fstream>
#include <istream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cxxtrace_test {
namespace {
template<class T>
auto
parse_integer_value(std::string_view value_string) -> std::optional<T>;
}

auto
linux_proc_cpuinfo::parse() -> linux_proc_cpuinfo
{
  auto file = std::ifstream{};
  file.exceptions(std::ifstream::badbit | std::ifstream::failbit);
  file.open("/proc/cpuinfo");
  file.exceptions(std::ifstream::badbit);
  return parse(file);
}

auto
linux_proc_cpuinfo::parse(cxxtrace::czstring input) -> linux_proc_cpuinfo
{
  auto input_stream = std::istringstream{ input };
  return linux_proc_cpuinfo::parse(input_stream);
}

auto
linux_proc_cpuinfo::parse(std::istream& input) -> linux_proc_cpuinfo
{
  auto cpuinfo = linux_proc_cpuinfo{};
  auto line = std::string{};
  while (std::getline(input, line, '\n')) {
    cpuinfo.parse_cpuinfo_line(line);
  }
  return cpuinfo;
}

auto
linux_proc_cpuinfo::parse_cpuinfo_line(const std::string& line) -> void
{
  auto have_key = [&line](cxxtrace::czstring key) -> bool {
    // TODO(strager): Use std::string::starts_with when C++20 is available.
    return line.find(key) == 0;
  };
  auto get_value_string = [&line]() -> std::string_view {
    auto colon_index = line.find(':');
    if (colon_index == line.npos) {
      // Value is absent.
      return {};
    }
    auto value_index = line.find_first_not_of(" \t", colon_index + 1);
    if (value_index == line.npos) {
      // Value is present but empty.
      return std::string_view{ line.data() + line.size(), 0 };
    } else {
      // Value is present and non-empty.
      return std::string_view{ line }.substr(value_index);
    }
  };

  if (have_key("processor")) {
    auto number = parse_integer_value<processor_number>(get_value_string());
    if (!number.has_value()) {
      std::fprintf(stderr,
                   "warning: ignoring invalid /proc/cpuinfo line: %s\n",
                   line.c_str());
      return;
    }
    // TODO(strager): How should we handle duplicate entries like "processor :
    // 0\nprocessor: 0\n"?
    this->processor_entries.emplace_back(*number);
    return;
  }
  if (this->processor_entries.empty()) {
    std::fprintf(stderr,
                 "warning: ignoring unexpected /proc/cpuinfo line before first "
                 "processor: %s\n",
                 line.c_str());
    return;
  }
  auto& entry = this->processor_entries.back();

  if (have_key("initial apicid")) {
    if (entry.initial_apicid.has_value()) {
      std::fprintf(stderr,
                   "warning: ignoring spurious /proc/cpuinfo line: %s\n",
                   line.c_str());
      return;
    }
    entry.initial_apicid =
      parse_integer_value<decltype(entry.initial_apicid)::value_type>(
        get_value_string());
    if (!entry.initial_apicid.has_value()) {
      std::fprintf(stderr,
                   "warning: ignoring invalid /proc/cpuinfo line: %s\n",
                   line.c_str());
      return;
    }
    return;
  }
}

auto
linux_proc_cpuinfo::get_processor_numbers() const
  -> std::vector<processor_number>
{
  auto numbers = std::vector<processor_number>{};
  for (const auto& entry : this->processor_entries) {
    numbers.emplace_back(entry.number);
  }
  return numbers;
}

auto
linux_proc_cpuinfo::get_initial_apicid(processor_number number) const
  -> std::optional<int>
{
  return this->get_processor_entry(number).initial_apicid;
}

auto
linux_proc_cpuinfo::get_processor_entry(processor_number number) const
  -> const processor_entry&
{
  auto entry_it = std::find_if(
    this->processor_entries.begin(),
    this->processor_entries.end(),
    [&](const processor_entry& entry) { return entry.number == number; });
  if (entry_it == this->processor_entries.end()) {
    throw std::out_of_range{ "Invalid processor number" };
  }
  return *entry_it;
}

namespace {
template<class T>
auto
parse_integer_value(std::string_view value_string) -> std::optional<T>
{
  if (!value_string.data()) {
    return std::nullopt;
  }

  T value /* uninitialized */;
  auto result = std::from_chars(
    value_string.data(), value_string.data() + value_string.size(), value);
  if (result.ptr == value_string.data()) {
    return std::nullopt;
  }
  // TODO(strager): Should we fail if there's junk after the value?
  return value;
}
}
}
