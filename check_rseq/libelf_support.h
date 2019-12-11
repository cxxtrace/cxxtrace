#ifndef CXXTRACE_CHECK_RSEQ_LIBELF_SUPPORT_H
#define CXXTRACE_CHECK_RSEQ_LIBELF_SUPPORT_H

#include "machine_code.h"
#include <cstddef>
#include <cxxtrace/string.h>
#include <iterator>
#include <libelf/gelf.h>
#include <libelf/libelf.h>
#include <optional>
#include <vector>

namespace cxxtrace_check_rseq {
class elf_chunk_iterator;
class elf_section_iterator;

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

class elf_chunk_range
{
public:
  explicit elf_chunk_range(::Elf_Scn*) noexcept;

  auto begin() const noexcept -> elf_chunk_iterator;
  auto end() const noexcept -> elf_chunk_iterator;

private:
  ::Elf_Scn* section_{ nullptr };
};

class elf_chunk_iterator
{
public:
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::input_iterator_tag;
  using pointer = ::Elf_Data**;
  using reference = ::Elf_Data*;
  using value_type = ::Elf_Data*;

  /*implicit*/ elf_chunk_iterator() noexcept;
  explicit elf_chunk_iterator(::Elf_Scn*, ::Elf_Data*);

  auto operator*() const noexcept -> ::Elf_Data*;
  auto operator-> () const noexcept -> ::Elf_Data*;

  auto operator==(const elf_chunk_iterator& other) const noexcept -> bool;
  auto operator!=(const elf_chunk_iterator& other) const noexcept -> bool;

  auto operator++() -> elf_chunk_iterator&;
  auto operator++(int) -> elf_chunk_iterator;

private:
  auto is_end_iterator() const noexcept -> bool;

  ::Elf_Scn* section_{ nullptr };
  ::Elf_Data* chunk_{ nullptr };
};

class elf_section_range
{
public:
  explicit elf_section_range(::Elf*) noexcept;

  auto begin() const noexcept -> elf_section_iterator;
  auto end() const noexcept -> elf_section_iterator;

private:
  ::Elf* elf_{ nullptr };
};

class elf_section_iterator
{
public:
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::input_iterator_tag;
  using pointer = ::Elf_Scn**;
  using reference = ::Elf_Scn*;
  using value_type = ::Elf_Scn*;

  /*implicit*/ elf_section_iterator() noexcept;
  explicit elf_section_iterator(::Elf*, ::Elf_Scn*);

  auto operator*() const noexcept -> ::Elf_Scn*;
  auto operator-> () const noexcept -> ::Elf_Scn*;

  auto operator==(const elf_section_iterator& other) const noexcept -> bool;
  auto operator!=(const elf_section_iterator& other) const noexcept -> bool;

  auto operator++() -> elf_section_iterator&;
  auto operator++(int) -> elf_section_iterator;

private:
  auto is_end_iterator() const noexcept -> bool;

  ::Elf* elf_{ nullptr };
  ::Elf_Scn* section_{ nullptr };
};

auto
elf_architecture(::Elf*) -> std::optional<machine_architecture>;

auto
elf_header(::Elf*) -> ::GElf_Ehdr;

auto
section_header(::Elf_Scn*) -> ::GElf_Shdr;

auto
section_name(::Elf*, ::Elf_Scn*) -> cxxtrace::czstring;

auto
section_headers_string_table_section_index(::Elf*) -> std::size_t;

auto
section_by_index(::Elf*, std::size_t index) -> ::Elf_Scn*;

auto
section_data(::Elf_Scn*) -> std::vector<std::byte>;

template<class Func>
auto
enumerate_symbols(::Elf_Scn*,
                  const ::GElf_Shdr& section_header,
                  Func&& callback) -> void;
template<class Func>
auto
enumerate_symbols(::Elf*, Func&& callback) -> void;
}

#include "libelf_support_impl.h" // IWYU pragma: export

#endif
