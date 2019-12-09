#ifndef CXXTRACE_CHECK_RSEQ_ELF_FUNCTION_H
#define CXXTRACE_CHECK_RSEQ_ELF_FUNCTION_H

#include "machine_code.h"
#include <cstddef>
#include <cxxtrace/string.h>
#include <libelf/gelf.h>
#include <libelf/libelf.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace cxxtrace_check_rseq {
struct elf_function
{
  std::string name;
  std::vector<std::byte> instruction_bytes;
  machine_address base_address;
  machine_architecture architecture;
};

auto
function_from_symbol(::Elf*, const ::GElf_Sym&, cxxtrace::czstring name)
  -> elf_function;

// This function can throw an exception, including any of the following:
// * function_contains_unallocated_bytes
// * multiple_functions_contain_address
// * no_function_containing_address
auto
function_containing_address(::Elf*, machine_address) -> elf_function;

class function_contains_unallocated_bytes : public std::runtime_error
{
public:
  explicit function_contains_unallocated_bytes(
    cxxtrace::czstring function_name);
};

class multiple_functions_contain_address : public std::runtime_error
{
public:
  explicit multiple_functions_contain_address(machine_address address);
};

class no_function_containing_address : public std::runtime_error
{
public:
  explicit no_function_containing_address(machine_address address);
};
}

#endif
