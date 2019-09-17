#ifndef CXXTRACE_TEST_LINUX_PROC_H
#define CXXTRACE_TEST_LINUX_PROC_H

#include <cxxtrace/string.h>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace cxxtrace_test {
class linux_proc_cpuinfo
{
public:
  using processor_number = int;

  static auto parse() -> linux_proc_cpuinfo;
  static auto parse(cxxtrace::czstring) -> linux_proc_cpuinfo;
  static auto parse(std::istream&) -> linux_proc_cpuinfo;

  auto get_processor_numbers() const -> std::vector<processor_number>;
  auto get_initial_apicid(processor_number) const -> std::optional<int>;

private:
  struct processor_entry
  {
    explicit processor_entry(processor_number number) noexcept
      : number{ number }
    {}

    processor_number number;

    std::optional<int> initial_apicid;
  };

  auto parse_cpuinfo_line(const std::string&) -> void;

  auto get_processor_entry(processor_number) const -> const processor_entry&;

  std::vector<processor_entry> processor_entries{};
};
}

#endif
