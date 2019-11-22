#ifndef CXXTRACE_TEST_ASSEMBLER_H
#define CXXTRACE_TEST_ASSEMBLER_H

#include "elf_function.h"
#include "libelf_support.h"
#include "machine_code.h"
#include <cxxtrace/detail/file_descriptor.h>
#include <cxxtrace/string.h>
#include <initializer_list>
#include <libelf/libelf.h>
#include <stdexcept>
#include <string>

namespace cxxtrace_test {
class assemble_exception : public std::runtime_error
{
public:
  explicit assemble_exception(cxxtrace::czstring message);
};

class temporary_elf_file
{
public:
  ~temporary_elf_file();

  auto elf() const noexcept -> ::Elf*;
  auto elf_path() const noexcept -> cxxtrace::czstring;

  auto symbol(cxxtrace::czstring name) const
    -> cxxtrace_check_rseq::machine_address;

  auto delete_files() -> void;

  std::string source_path_;
  std::string elf_path_;
  cxxtrace::detail::file_descriptor elf_file_;
  cxxtrace_check_rseq::elf_handle elf_;
  bool did_delete_files_{ false };
};

auto
assemble_into_elf_shared_object(
  cxxtrace_check_rseq::machine_architecture,
  cxxtrace::czstring assembly_code,
  std::initializer_list<cxxtrace::czstring> extra_cc_options)
  -> temporary_elf_file;
auto
assemble_function_into_elf_shared_object(
  cxxtrace_check_rseq::machine_architecture,
  const std::string& assembly_code,
  cxxtrace::czstring function_name) -> temporary_elf_file;

auto
function_from_symbol(::Elf*, cxxtrace::czstring name)
  -> cxxtrace_check_rseq::elf_function;
}

#endif
