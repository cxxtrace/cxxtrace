#ifndef CXXTRACE_CHECK_RSEQ_LIBELF_SUPPORT_IMPL_H
#define CXXTRACE_CHECK_RSEQ_LIBELF_SUPPORT_IMPL_H

// IWYU pragma: private, include "libelf_support.h"
#if !defined(CXXTRACE_CHECK_RSEQ_LIBELF_SUPPORT_H)
#error                                                                         \
  "Include \"libelf_support.h\" instead of including \"libelf_support_impl.h\" directly."
#endif

#include <cassert>
#include <elf.h>
#include <gelf.h>
#include <libelf.h>
#include <stdexcept>

namespace cxxtrace_check_rseq {
template<class Func>
auto
enumerate_symbols(::Elf_Scn* section,
                  const ::GElf_Shdr& section_header,
                  Func&& callback) -> void
{
  assert(section_header.sh_type == SHT_SYMTAB);
  for (auto* chunk : elf_chunk_range{ section }) {
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
  }
}

template<class Func>
auto
enumerate_symbols(::Elf* elf, Func&& callback) -> void
{
  for (auto* section : elf_section_range{ elf }) {
    auto header = section_header(section);
    if (header.sh_type == SHT_SYMTAB) {
      enumerate_symbols(section, header, [&](const ::GElf_Sym& symbol) -> void {
        callback(header, symbol);
      });
    }
  }
}
}

#endif
