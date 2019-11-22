#ifndef CXXTRACE_CHECK_RSEQ_CAPSTONE_H
#define CXXTRACE_CHECK_RSEQ_CAPSTONE_H

#include "machine_code.h"
#include "rseq_analyzer.h"
#include <capstone/capstone.h>
#include <cstddef>
#include <memory>
#include <string>

namespace cxxtrace_check_rseq {
class capstone_instructions;
class capstone_register_set;

using capstone_group_type =
  std::remove_reference_t<decltype(::cs_detail::groups[0])>;
using capstone_register_type =
  std::remove_reference_t<decltype(std::declval<::cs_regs>()[0])>;

[[noreturn]] auto throw_capstone_error(::cs_err) -> void;
[[noreturn]] auto throw_capstone_error(::csh) -> void;

auto
disassemble(::csh capstone,
            const std::byte* code,
            std::size_t code_size,
            machine_address address,
            std::size_t count) -> capstone_instructions;

auto
instruction_string(const ::cs_insn&) -> std::string;

auto
instruction_in_group(const ::cs_insn&, capstone_group_type) noexcept -> bool;

auto
jump_target(const ::cs_insn&) -> std::optional<machine_address>;

auto
registers_written_by_instruction(::csh, const ::cs_insn&)
  -> capstone_register_set;

class capstone_handle
{
public:
  static auto open(::cs_arch arch, ::cs_mode mode) -> capstone_handle;

  explicit capstone_handle(::csh handle) noexcept;

  capstone_handle(const capstone_handle&) = delete;
  capstone_handle& operator=(const capstone_handle&) = delete;

  capstone_handle(capstone_handle&& other) noexcept;

  capstone_handle& operator=(capstone_handle&& other) = delete;

  ~capstone_handle();

  auto close() noexcept(false) -> void;

  auto valid() const noexcept -> bool;
  auto get() const noexcept -> ::csh;

  auto option(::cs_opt_type type, std::size_t value) -> void;

private:
  static constexpr auto invalid_handle = ::csh{ 0 };

  ::csh handle_{ invalid_handle };
};

struct cs_free_deleter
{
  std::size_t count;

  auto operator()(::cs_insn*) const noexcept -> void;
};

class capstone_instructions
{
public:
  explicit capstone_instructions(::cs_insn* instructions,
                                 std::size_t instruction_count) noexcept;

  auto size() const noexcept -> std::size_t;
  auto begin() const noexcept -> const ::cs_insn*;
  auto end() const noexcept -> const ::cs_insn*;

private:
  std::unique_ptr<::cs_insn, cs_free_deleter> instructions_;
};

class capstone_register_set
{
public:
  explicit capstone_register_set(const ::cs_regs& registers,
                                 std::uint8_t register_count) noexcept;

  auto begin() const noexcept -> const capstone_register_type*;
  auto end() const noexcept -> const capstone_register_type*;

  auto contains(capstone_register_type) const noexcept -> bool;

private:
  ::cs_regs registers_;
  std::uint8_t register_count_;
};
}

#endif
