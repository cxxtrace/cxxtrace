#include "libelf_support.h"
#include "machine_code.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cxxtrace/string.h>
#include <elf.h>
#include <exception>
#include <libelf.h>
#include <libelf/gelf.h>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace cxxtrace_check_rseq {
namespace {
class libelf_error : public std::runtime_error
{
public:
  explicit libelf_error(int error)
    : std::runtime_error{ ::elf_errmsg(error) }
  {}
};
}

auto
initialize_libelf() -> void
{
  auto rc = ::elf_version(EV_CURRENT);
  if (rc == EV_NONE) {
    throw_libelf_error();
  }
}

[[noreturn]] auto
throw_libelf_error() -> void
{
  auto error = ::elf_errno();
  throw libelf_error{ error };
}

auto
elf_handle::begin(int file_descriptor, ::Elf_Cmd command) -> elf_handle
{
  auto* elf = ::elf_begin(file_descriptor, command, nullptr);
  if (!elf) {
    throw_libelf_error();
  }
  return elf_handle{ elf };
}

elf_handle::elf_handle(::Elf* elf) noexcept
  : elf_{ elf }
{}

elf_handle::elf_handle(elf_handle&& other) noexcept
  : elf_handle{ std::exchange(other.elf_, nullptr) }
{}

elf_handle::~elf_handle()
{
  this->reset();
}

auto
elf_handle::end() noexcept(false) -> void
{
  assert(this->valid());
  if (::elf_end(this->elf_) < 0) {
    throw_libelf_error();
  }
  this->elf_ = nullptr;
}

auto
elf_handle::reset() noexcept(false) -> void
{
  if (this->valid()) {
    this->end();
  }
}

auto
elf_handle::get() const noexcept -> ::Elf*
{
  assert(this->valid());
  return this->elf_;
}

auto
elf_handle::valid() const noexcept -> bool
{
  return this->elf_;
}

elf_chunk_range::elf_chunk_range(::Elf_Scn* section) noexcept
  : section_{ section }
{}

auto
elf_chunk_range::begin() const noexcept -> elf_chunk_iterator
{
  auto* chunk = ::elf_getdata(this->section_, nullptr);
  return elf_chunk_iterator{ this->section_, chunk };
}

auto
elf_chunk_range::end() const noexcept -> elf_chunk_iterator
{
  return elf_chunk_iterator{};
}

elf_chunk_iterator::elf_chunk_iterator() noexcept = default;

elf_chunk_iterator::elf_chunk_iterator(::Elf_Scn* section, ::Elf_Data* chunk)
  : section_{ section }
  , chunk_{ chunk }
{}

auto elf_chunk_iterator::operator*() const noexcept -> ::Elf_Data*
{
  assert(this->chunk_);
  return this->chunk_;
}

auto elf_chunk_iterator::operator-> () const noexcept -> ::Elf_Data*
{
  assert(false && "Unimplemented");
  std::terminate();
}

auto
elf_chunk_iterator::operator==(const elf_chunk_iterator& other) const noexcept
  -> bool
{
  if (this->is_end_iterator()) {
    return other.is_end_iterator();
  }
  if (other.is_end_iterator()) {
    return this->is_end_iterator();
  }
  return std::tie(this->section_, this->chunk_) ==
         std::tie(other.section_, other.chunk_);
}

auto
elf_chunk_iterator::operator!=(const elf_chunk_iterator& other) const noexcept
  -> bool
{
  return !(*this == other);
}

auto
elf_chunk_iterator::operator++() -> elf_chunk_iterator&
{
  assert(!this->is_end_iterator());
  this->chunk_ = ::elf_getdata(this->section_, this->chunk_);
  return *this;
}

auto
elf_chunk_iterator::operator++(int) -> elf_chunk_iterator
{
  assert(false && "Unimplemented");
  std::terminate();
}

auto
elf_chunk_iterator::is_end_iterator() const noexcept -> bool
{
  return !this->section_ || !this->chunk_;
}

elf_section_range::elf_section_range(::Elf* elf) noexcept
  : elf_{ elf }
{}

auto
elf_section_range::begin() const noexcept -> elf_section_iterator
{
  auto* section = ::elf_nextscn(this->elf_, nullptr);
  return elf_section_iterator{ this->elf_, section };
}

auto
elf_section_range::end() const noexcept -> elf_section_iterator
{
  return elf_section_iterator{};
}

elf_section_iterator::elf_section_iterator() noexcept = default;

elf_section_iterator::elf_section_iterator(::Elf* elf, ::Elf_Scn* section)
  : elf_{ elf }
  , section_{ section }
{}

auto elf_section_iterator::operator*() const noexcept -> ::Elf_Scn*
{
  assert(this->section_);
  return this->section_;
}

auto elf_section_iterator::operator-> () const noexcept -> ::Elf_Scn*
{
  assert(false && "Unimplemented");
  std::terminate();
}

auto
elf_section_iterator::operator==(const elf_section_iterator& other) const
  noexcept -> bool
{
  if (this->is_end_iterator()) {
    return other.is_end_iterator();
  }
  if (other.is_end_iterator()) {
    return this->is_end_iterator();
  }
  return std::tie(this->elf_, this->section_) ==
         std::tie(other.elf_, other.section_);
}

auto
elf_section_iterator::operator!=(const elf_section_iterator& other) const
  noexcept -> bool
{
  return !(*this == other);
}

auto
elf_section_iterator::operator++() -> elf_section_iterator&
{
  assert(!this->is_end_iterator());
  this->section_ = ::elf_nextscn(this->elf_, this->section_);
  return *this;
}

auto
elf_section_iterator::operator++(int) -> elf_section_iterator
{
  assert(false && "Unimplemented");
  std::terminate();
}

auto
elf_section_iterator::is_end_iterator() const noexcept -> bool
{
  return !this->elf_ || !this->section_;
}

auto
elf_architecture(::Elf* elf) -> std::optional<machine_architecture>
{
  auto header = elf_header(elf);
  switch (header.e_machine) {
    case EM_X86_64:
      return machine_architecture::x86;
    default:
      return std::nullopt;
  }
}

auto
elf_header(::Elf* elf) -> ::GElf_Ehdr
{
  ::GElf_Ehdr header_storage /* uninitialized */;
  auto* header = ::gelf_getehdr(elf, &header_storage);
  if (!header) {
    throw_libelf_error();
  }
  assert(header == &header_storage);
  return *header;
}

auto
section_header(::Elf_Scn* section) -> ::GElf_Shdr
{
  ::GElf_Shdr header_storage /* uninitialized */;
  auto* header = ::gelf_getshdr(section, &header_storage);
  if (!header) {
    throw_libelf_error();
  }
  assert(header == &header_storage);
  return *header;
}

auto
section_name(::Elf* elf, ::Elf_Scn* section) -> cxxtrace::czstring
{
  auto string_table_section_index =
    section_headers_string_table_section_index(elf);
  auto header = section_header(section);
  auto* section_name =
    ::elf_strptr(elf, string_table_section_index, header.sh_name);
  if (!section_name) {
    throw_libelf_error();
  }
  return section_name;
}

auto
section_headers_string_table_section_index(::Elf* elf) -> std::size_t
{
  std::size_t index /* uninitialized */;
  if (::elf_getshdrstrndx(elf, &index) < 0) {
    throw_libelf_error();
  }
  return index;
}

auto
section_by_index(::Elf* elf, std::size_t index) -> ::Elf_Scn*
{
  auto* section = ::elf_getscn(elf, index);
  if (!section) {
    throw_libelf_error();
  }
  return section;
}

auto
section_data(::Elf_Scn* section) -> std::vector<std::byte>
{
  auto header = section_header(section);
  auto data = std::vector<std::byte>{};
  if (header.sh_size == 0) {
    return data;
  }
  for (auto* chunk : elf_chunk_range{ section }) {
    if (chunk->d_type != ELF_T_BYTE) {
      throw std::runtime_error{ "Expected section to contain bytes" };
    }
    assert(chunk->d_size > 0);
    auto end = chunk->d_off + chunk->d_size;
    if (data.size() < end) {
      data.resize(end);
    }
    auto* chunk_bytes = static_cast<const std::byte*>(chunk->d_buf);
    if (chunk_bytes) {
      std::copy(&chunk_bytes[0],
                &chunk_bytes[chunk->d_size],
                data.begin() + chunk->d_off);
    } else {
      assert(header.sh_type == SHT_NOBITS);
      std::fill_n(data.begin() + chunk->d_off, chunk->d_size, std::byte{ 0 });
    }
  }
  return data;
}

auto
section_contains_address(const ::GElf_Shdr& section_header,
                         machine_address address) -> bool
{
  return section_header.sh_addr <= address &&
         address < section_header.sh_addr + section_header.sh_size;
}

auto
get_section_entry(::Elf_Scn* section,
                  ::Elf_Type expected_type,
                  std::uint64_t entry_index) -> std::optional<elf_section_entry>
{
  for (auto* chunk : elf_chunk_range{ section }) {
    if (chunk->d_type != expected_type) {
      throw std::runtime_error{
        "Expected section to contain entries of a different type"
      };
    }
    auto entry_size = section_header(section).sh_entsize;
    auto chunk_entries_begin = chunk->d_off / entry_size;
    auto chunk_entry_count = chunk->d_size / entry_size;
    auto chunk_entries_end = chunk_entries_begin + chunk_entry_count;
    if (chunk_entries_begin <= entry_index && entry_index < chunk_entries_end) {
      return elf_section_entry{ chunk, entry_index - chunk_entries_begin };
    }
  }
  return std::nullopt;
}
}
