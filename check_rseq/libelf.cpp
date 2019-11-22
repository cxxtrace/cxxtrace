#include "libelf_support.h"
#include <cstdio>
#include <exception>
#include <libelf/gelf.h>
#include <optional>
#include <stdexcept>

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
  auto* chunk = ::elf_getdata(section, nullptr);
  while (chunk) {
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
    chunk = ::elf_getdata(section, chunk);
  }
  return data;
}
}
