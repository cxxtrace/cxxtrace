#include "elf_function.h"
#include "libelf_support.h"
#include "machine_code.h"
#include <algorithm>
#include <cassert>
#include <elf.h>
#include <gelf.h>
#include <libelf.h>
#include <optional>
#include <string>
#include <utility>

namespace cxxtrace_check_rseq {
function_contains_unallocated_bytes::function_contains_unallocated_bytes(
  cxxtrace::czstring function_name)
  : std::runtime_error{ std::string{ "Function contains unallocated bytes: " } +
                        function_name }
{}

no_function_containing_address::no_function_containing_address(
  machine_address address)
  /// TODO(strager): Show address in hexidecimal with an '0x' prefix.
  : std::runtime_error{ "No function exists at address " +
                        std::to_string(address) }
{}

multiple_functions_contain_address::multiple_functions_contain_address(
  machine_address address)
  /// TODO(strager): Show address in hexidecimal with an '0x' prefix.
  : std::runtime_error{ "Multiple functions exists at address " +
                        std::to_string(address) }
{}

auto
function_from_symbol(::Elf* elf,
                     const ::GElf_Sym& symbol,
                     cxxtrace::czstring name) -> elf_function
{
  auto optional_architecture = elf_architecture(elf);
  if (!optional_architecture.has_value()) {
    throw std::runtime_error{ "ELF file has unknown architecture" };
  }

  auto* section = section_by_index(elf, symbol.st_shndx);
  auto section_header = cxxtrace_check_rseq::section_header(section);
  auto section_begin_address = section_header.sh_addr;
  auto section_end_address = section_begin_address + section_header.sh_size;
  if (symbol.st_value < section_begin_address) {
    throw function_contains_unallocated_bytes{ name };
  }

  auto begin_offset = symbol.st_value - section_begin_address;
  auto end_offset = begin_offset + symbol.st_size;
  if (end_offset > section_end_address) {
    throw function_contains_unallocated_bytes{ name };
  }

  auto section_bytes = section_data(section);
  if (end_offset > section_bytes.size()) {
    throw function_contains_unallocated_bytes{ name };
  }
  auto bytes = std::vector<std::byte>{};
  bytes.resize(symbol.st_size);
  std::copy(section_bytes.data() + begin_offset,
            section_bytes.data() + end_offset,
            bytes.data());

  return elf_function{ .name = name,
                       .instruction_bytes = std::move(bytes),
                       .base_address = symbol.st_value,
                       .architecture = *optional_architecture };
}

auto
function_containing_address(::Elf* elf, machine_address address) -> elf_function
{
  struct symbol_info : public ::GElf_Sym
  {
    decltype(::GElf_Shdr::sh_link) name_string_table_section_index;
  };
  auto function_symbol = std::optional<symbol_info>{};
  enumerate_symbols(elf,
                    [&](const ::GElf_Shdr& symbol_table_header,
                        const ::GElf_Sym& symbol) -> void {
                      if (GELF_ST_TYPE(symbol.st_info) != STT_FUNC) {
                        return;
                      }
                      auto address_is_within_symbol =
                        symbol.st_value <= address &&
                        address < symbol.st_value + symbol.st_size;
                      if (!address_is_within_symbol) {
                        return;
                      }
                      if (function_symbol.has_value()) {
                        if (function_symbol->st_value != symbol.st_value ||
                            function_symbol->st_size != symbol.st_size) {
                          throw multiple_functions_contain_address{ address };
                        }
                      }
                      function_symbol =
                        symbol_info{ symbol,
                                     .name_string_table_section_index =
                                       symbol_table_header.sh_link };
                    });
  if (!function_symbol.has_value()) {
    throw no_function_containing_address{ address };
  }
  const auto* function_name =
    ::elf_strptr(elf,
                 function_symbol->name_string_table_section_index,
                 function_symbol->st_name);
  assert(function_name);
  if (!function_name) {
    function_name = "???";
  }
  return function_from_symbol(elf, *function_symbol, function_name);
}
}
