#ifndef CXXTRACE_CHECK_RSEQ_LIBELF_SUPPORT_IMPL_H
#define CXXTRACE_CHECK_RSEQ_LIBELF_SUPPORT_IMPL_H

#if !defined(CXXTRACE_CHECK_RSEQ_LIBELF_SUPPORT_H)
#error                                                                         \
  "Include \"libelf_support.h\" instead of including \"libelf_support_impl.h\" directly."
#endif

#include <cassert>
#include <cstring>
#include <stdexcept>

namespace cxxtrace_check_rseq {
template<class Func>
auto
enumerate_sections(::Elf* elf, Func&& callback) -> void
{
  auto* section = ::elf_nextscn(elf, nullptr);
  while (section) {
    callback(section);
    section = ::elf_nextscn(elf, section);
  }
}

template<class Func>
auto
enumerate_sections_with_name(::Elf* elf,
                             cxxtrace::czstring section_name,
                             Func&& callback) -> void
{
  auto string_table_section_index =
    section_headers_string_table_section_index(elf);
  enumerate_sections(elf, [&](::Elf_Scn* section) -> void {
    auto header = section_header(section);
    auto* current_section_name =
      ::elf_strptr(elf, string_table_section_index, header.sh_name);
    if (!current_section_name) {
      throw_libelf_error();
    }
    if (std::strcmp(current_section_name, section_name) == 0) {
      callback(section);
    }
  });
}

template<class Func>
auto
enumerate_sections_with_type(::Elf* elf,
                             ::Elf64_Word section_type,
                             Func&& callback) -> void
{
  auto* section = ::elf_nextscn(elf, nullptr);
  while (section) {
    auto header = section_header(section);
    if (header.sh_type == section_type) {
      callback(section, header);
    }
    section = ::elf_nextscn(elf, section);
  }
}

template<class Func>
auto
enumerate_symbols(::Elf_Scn* section,
                  const ::GElf_Shdr& section_header,
                  Func&& callback) -> void
{
  assert(section_header.sh_type == SHT_SYMTAB);
  auto* chunk = ::elf_getdata(section, nullptr);
  while (chunk) {
    if (chunk->d_type != ELF_T_SYM) {
      throw std::runtime_error{ "Expected section to contain symbols" };
    }
    auto symbol_count = chunk->d_size / section_header.sh_entsize;
    using size_type = decltype(symbol_count);
    for (auto i = size_type{ 0 }; i < symbol_count; ++i) {
      ::GElf_Sym symbol /* uninitialized */;
      if (!::gelf_getsym(chunk, i, &symbol)) {
        throw_libelf_error();
      }
      callback(symbol);
    }
    chunk = ::elf_getdata(section, chunk);
  }
}

template<class Func>
auto
enumerate_symbols(::Elf* elf, Func&& callback) -> void
{
  enumerate_sections_with_type(
    elf,
    SHT_SYMTAB,
    [&](::Elf_Scn* section, const ::GElf_Shdr& section_header) -> void {
      enumerate_symbols(
        section, section_header, [&](const ::GElf_Sym& symbol) -> void {
          callback(section_header, symbol);
        });
    });
}
}

#endif
