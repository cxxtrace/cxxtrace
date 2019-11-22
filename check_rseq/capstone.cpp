#include "capstone.h"
#include "machine_code.h"
#include <algorithm>
#include <capstone/capstone.h>
#include <cassert>
#include <cstdio>
#include <exception>
#include <sstream>

namespace cxxtrace_check_rseq {
namespace {
class capstone_error : public std::runtime_error
{
public:
  explicit capstone_error(::cs_err error)
    : std::runtime_error{ ::cs_strerror(error) }
  {}
};
}

[[noreturn]] auto
throw_capstone_error(::cs_err error) -> void
{
  throw capstone_error{ error };
}

[[noreturn]] auto
throw_capstone_error(::csh capstone) -> void
{
  throw_capstone_error(::cs_errno(capstone));
}

auto
disassemble(::csh capstone,
            const std::byte* code,
            std::size_t code_size,
            machine_address address,
            std::size_t count) -> capstone_instructions
{
  ::cs_insn* instructions /* uninitialized */;
  auto instruction_count =
    ::cs_disasm(capstone,
                /*code=*/reinterpret_cast<const std::uint8_t*>(code),
                /*code_size=*/code_size,
                /*address=*/address,
                /*count=*/count,
                &instructions);
  if (instruction_count == 0) {
    auto error = ::cs_errno(capstone);
    if (error != ::CS_ERR_OK) {
      throw_capstone_error(error);
    }
  }
  return capstone_instructions{ instructions, instruction_count };
}

auto
instruction_string(const ::cs_insn& instruction) -> std::string
{
  auto result = std::ostringstream{};
  result << instruction.mnemonic;
  if (instruction.op_str[0] != '\0') {
    result << ' ' << instruction.op_str;
  }
  return std::move(result).str();
}

auto
instruction_in_group(const ::cs_insn& instruction,
                     capstone_group_type group) noexcept -> bool
{
  return std::any_of(instruction.detail->groups,
                     instruction.detail->groups +
                       instruction.detail->groups_count,
                     [group](auto g) { return g == group; });
}

auto
jump_target(const ::cs_insn& instruction) -> std::optional<machine_address>
{
  assert(instruction_in_group(instruction, ::X86_GRP_CALL) ||
         instruction_in_group(instruction, ::X86_GRP_JUMP));
  if (instruction.detail->x86.op_count != 1) {
    throw std::runtime_error{
      "Call or jump instruction has unexpected number of operands"
    };
  }
  const auto& operand = instruction.detail->x86.operands[0];
  switch (operand.type) {
    case ::X86_OP_IMM:
      // Example: callq _some_symbol
      // Example: jnz .some_label
      return std::uint64_t(operand.imm);
    case ::X86_OP_MEM:
      // Example: callq *0(%rbx)
      return std::nullopt;
    case ::X86_OP_REG:
      // Example: callq *%rsi
      return std::nullopt;
    case ::X86_OP_INVALID:
      assert(false);
      return std::nullopt;
  }
  assert(false);
  return std::nullopt;
}

auto
registers_written_by_instruction(::csh capstone, const ::cs_insn& instruction)
  -> capstone_register_set
{
  ::cs_regs read_registers /* uninitialized */;
  std::uint8_t read_register_count /* uninitialized */;
  ::cs_regs written_registers /* uninitialized */;
  std::uint8_t written_register_count /* uninitialized */;
  auto rc = ::cs_regs_access(capstone,
                             &instruction,
                             read_registers,
                             &read_register_count,
                             written_registers,
                             &written_register_count);
  if (rc != CS_ERR_OK) {
    throw_capstone_error(rc);
  }
  return capstone_register_set{ written_registers, written_register_count };
}

auto
capstone_handle::open(::cs_arch arch, ::cs_mode mode) -> capstone_handle
{
  ::csh handle /* uninitialized */;
  auto rc = ::cs_open(arch, mode, &handle);
  if (rc != ::CS_ERR_OK) {
    throw_capstone_error(rc);
  }
  return capstone_handle{ handle };
}

capstone_handle::capstone_handle(::csh handle) noexcept
  : handle_{ handle }
{
  assert(this->valid());
}

capstone_handle::capstone_handle(capstone_handle&& other) noexcept
  : handle_{ std::exchange(other.handle_, this->invalid_handle) }
{}

capstone_handle::~capstone_handle()
{
  if (this->valid()) {
    this->close();
  }
}

auto
capstone_handle::close() noexcept(false) -> void
{
  assert(this->valid());
  auto rc = ::cs_close(&this->handle_);
  if (rc != ::CS_ERR_OK) {
    throw_capstone_error(rc);
  }
}

auto
capstone_handle::valid() const noexcept -> bool
{
  return this->handle_ != this->invalid_handle;
}

auto
capstone_handle::get() const noexcept -> ::csh
{
  assert(this->valid());
  return this->handle_;
}

auto
capstone_handle::option(::cs_opt_type type, std::size_t value) -> void
{
  auto rc = ::cs_option(this->handle_, type, value);
  if (rc != ::CS_ERR_OK) {
    throw_capstone_error(rc);
  }
}

auto
cs_free_deleter::operator()(::cs_insn* p) const noexcept -> void
{
  ::cs_free(p, this->count);
}

capstone_instructions::capstone_instructions(
  ::cs_insn* instructions,
  std::size_t instruction_count) noexcept
  : instructions_{ instructions, cs_free_deleter{ instruction_count } }
{}

auto
capstone_instructions::size() const noexcept -> std::size_t
{
  return this->instructions_.get_deleter().count;
}

auto
capstone_instructions::begin() const noexcept -> const ::cs_insn*
{
  return this->instructions_.get();
}

auto
capstone_instructions::end() const noexcept -> const ::cs_insn*
{
  return this->begin() + this->size();
}

capstone_register_set::capstone_register_set(
  const ::cs_regs& registers,
  std::uint8_t register_count) noexcept
  : register_count_{ register_count }
{
  assert(register_count <= std::size(this->registers_));
  std::copy(&registers[0], &registers[register_count], &this->registers_[0]);
}

auto
capstone_register_set::begin() const noexcept -> const capstone_register_type*
{
  return this->registers_;
}

auto
capstone_register_set::end() const noexcept -> const capstone_register_type*
{
  return this->begin() + this->register_count_;
}

auto
capstone_register_set::contains(capstone_register_type reg) const noexcept
  -> bool
{
  return std::any_of(this->begin(), this->end(), [&](capstone_register_type r) {
    return r == reg;
  });
}
}
