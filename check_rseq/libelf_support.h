#ifndef CXXTRACE_CHECK_RSEQ_LIBELF_SUPPORT_H
#define CXXTRACE_CHECK_RSEQ_LIBELF_SUPPORT_H

#include "machine_code.h"
#include <cxxtrace/string.h>
#include <libelf/gelf.h>
#include <libelf/libelf.h>
#include <optional>
#include <vector>

namespace cxxtrace_check_rseq {
auto
initialize_libelf() -> void;

[[noreturn]] auto
throw_libelf_error() -> void;

class elf_handle
{
public:
  static auto begin(int file_descriptor, ::Elf_Cmd) -> elf_handle;

  explicit elf_handle(::Elf* elf) noexcept;

  elf_handle(const elf_handle&) = delete;
  elf_handle& operator=(const elf_handle&) = delete;

  elf_handle(elf_handle&&) noexcept;
  elf_handle& operator=(elf_handle&&) = delete;

  ~elf_handle();

  auto end() noexcept(false) -> void;
  auto reset() noexcept(false) -> void;

  auto get() const noexcept -> ::Elf*;
  auto valid() const noexcept -> bool;

private:
  ::Elf* elf_;
};

auto
elf_architecture(::Elf*) -> std::optional<machine_architecture>;

auto
elf_header(::Elf*) -> ::GElf_Ehdr;

auto
section_header(::Elf_Scn*) -> ::GElf_Shdr;

auto
section_headers_string_table_section_index(::Elf*) -> std::size_t;

auto
section_by_index(::Elf*, std::size_t index) -> ::Elf_Scn*;

auto
section_data(::Elf_Scn*) -> std::vector<std::byte>;

template<class Func>
auto
enumerate_sections(::Elf*, Func&& callback) -> void;

template<class Func>
auto
enumerate_sections_with_name(::Elf*,
                             cxxtrace::czstring section_name,
                             Func&& callback) -> void;

template<class Func>
auto
enumerate_sections_with_type(::Elf*, ::Elf64_Word section_type, Func&& callback)
  -> void;

template<class Func>
auto
enumerate_symbols(::Elf_Scn*,
                  const ::GElf_Shdr& section_header,
                  Func&& callback) -> void;
template<class Func>
auto
enumerate_symbols(::Elf*, Func&& callback) -> void;
}

#include "libelf_support_impl.h"

#endif
