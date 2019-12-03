#include "rseq_analyzer.h"
#include <cxxtrace/detail/iostream.h>
#include <iostream>
#include <optional>
#include <stdlib.h>
#include <string>
#include <variant>
#include <vector>

namespace {
auto
print_usage() -> void
{
  std::cerr << "usage: check_rseq ELFFILE\n";
}
}

auto
main(int argc, char** argv) -> int
{
  if (argc <= 1) {
    std::cerr << "error: expected at least one file\n";
    print_usage();
    return 1;
  }
  auto ok = true;
  for (auto i = 1; i < argc; ++i) {
    auto file_path = argv[i];
    auto analysis =
      cxxtrace_check_rseq::analyze_rseq_critical_sections_in_file(file_path);
    for (const auto& problem : analysis.file_problems()) {
      std::cerr << file_path << ": error: ";
      std::visit([](auto&& problem) { std::cerr << problem; }, problem);
      std::cerr << '\n';
      ok = false;
    }
    for (const auto& problem_group : analysis.problems_by_critical_section()) {
      std::cerr << file_path << ": in critical section in function "
                << problem_group.critical_section.function << ":\n";
      std::cerr << "  note: function starts at address:         " << std::hex
                << std::showbase
                << problem_group.critical_section.function_address << "\n";
      std::cerr << "  note: critical section starts at address: " << std::hex
                << std::showbase << problem_group.critical_section.start_address
                << "\n";
      std::cerr << "  note: critical section ends at address:   " << std::hex
                << std::showbase
                << problem_group.critical_section.post_commit_address;
      auto size_in_bytes = problem_group.critical_section.size_in_bytes();
      if (size_in_bytes.has_value()) {
        std::cerr << " (+" << std::dec << *size_in_bytes << " bytes)";
      }
      std::cerr << '\n';
      std::cerr << "  note: abort handler starts at address:    " << std::hex
                << std::showbase << problem_group.critical_section.abort_address
                << "\n";
      cxxtrace::detail::reset_ostream_formatting(std::cerr);
      for (const auto& problem : problem_group.problems) {
        std::cerr << "  error: ";
        std::visit([](auto&& problem) { std::cerr << problem; }, problem);
        std::cerr << '\n';
        ok = false;
      }
    }
  }
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
