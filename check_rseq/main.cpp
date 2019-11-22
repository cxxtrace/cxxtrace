#include "elf_function.h"
#include "rseq_analyzer.h"
#include <cstdio>
#include <cxxtrace/string.h>
#include <iostream>
#include <variant>

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
    for (const auto& problem : analysis.problems()) {
      std::cerr << file_path << ": error: ";
      std::visit([](auto&& problem) { std::cerr << problem; }, problem);
      std::cerr << '\n';
      ok = false;
    }
  }
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
