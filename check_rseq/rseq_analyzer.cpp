#include "capstone.h"
#include "elf_function.h"
#include "libelf_support.h"
#include "machine_code.h"
#include "parse_binary.h"
#include "rseq_analyzer.h"
#include <algorithm>
#include <array>
#include <capstone/capstone.h>
#include <capstone/x86.h>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cxxtrace/detail/file_descriptor.h>
#include <cxxtrace/detail/iostream.h>
#include <cxxtrace/string.h>
#include <elf.h>
#include <fcntl.h>
#include <gelf.h>
#include <iomanip>
#include <iterator>
#include <libelf.h>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cxxtrace_check_rseq {
namespace {
constexpr auto rseq_descriptor_section_name = ".data_cxxtrace_rseq";

template<class Problem>
constexpr auto
is_problem_with_critical_section() -> bool
{
  return std::is_base_of_v<rseq_problem::problem_with_critical_section,
                           std::remove_cv_t<std::remove_reference_t<Problem>>>;
}

template<class Problem>
constexpr auto
is_problem_with_critical_section(const Problem&) -> bool
{
  return is_problem_with_critical_section<Problem>();
}

constexpr auto
is_problem_with_critical_section(const any_rseq_problem& problem) -> bool
{
  return std::visit(
    [](const auto& problem) -> bool {
      return is_problem_with_critical_section(problem);
    },
    problem);
}
}

rseq_analysis::critical_section_problems::critical_section_problems(
  rseq_critical_section critical_section)
  : critical_section{ std::move(critical_section) }
{}

auto
rseq_analysis::all_problems() const -> std::vector<any_rseq_problem>
{
  return this->problems_;
}

auto
rseq_analysis::file_problems() const -> std::vector<any_rseq_problem>
{
  auto problems = std::vector<any_rseq_problem>{};
  std::copy_if(this->problems_.begin(),
               this->problems_.end(),
               std::back_inserter(problems),
               [](const any_rseq_problem& problem) {
                 return !is_problem_with_critical_section(problem);
               });
  return problems;
}

auto
rseq_analysis::problems_by_critical_section() const
  -> std::vector<critical_section_problems>
{
  auto groups = std::vector<critical_section_problems>{};
  auto add_problem = [&groups](const auto& problem) -> void {
    auto group_it =
      std::find_if(groups.begin(),
                   groups.end(),
                   [&](const critical_section_problems& group) {
                     return group.critical_section.descriptor_address ==
                            problem.critical_section.descriptor_address;
                   });
    if (group_it == groups.end()) {
      groups.emplace_back(problem.critical_section);
      group_it = std::prev(groups.end());
    }
    group_it->problems.emplace_back(problem);
  };
  for (const auto& problem : this->problems_) {
    std::visit(
      [&](const auto& problem) -> void {
        if constexpr (is_problem_with_critical_section<decltype(problem)>()) {
          add_problem(problem);
        }
      },
      problem);
  }
  return groups;
}

auto
rseq_analysis::add_problem(any_rseq_problem problem) -> void
{
  this->problems_.emplace_back(std::move(problem));
}

namespace {
// Return the bytes of librseq's [1] default signature (RSEQ_SIG) for the given
// architecture.
//
// [1] https://github.com/compudj/librseq
auto
rseq_signature_for_architecture(machine_architecture architecture)
  -> std::array<std::byte, rseq_signature_size>
{
  switch (architecture) {
    case machine_architecture::x86:
      return std::array<std::byte, rseq_signature_size>{ std::byte{ 0x53 },
                                                         std::byte{ 0x30 },
                                                         std::byte{ 0x05 },
                                                         std::byte{ 0x53 } };
  }
  throw std::runtime_error{ "Unknown architecture" };
}

class rseq_analyzer
{
public:
  explicit rseq_analyzer(::Elf* elf,
                         const elf_function& function,
                         const rseq_critical_section& critical_section)
    : elf_{ elf }
    , function{ function }
    , critical_section{ critical_section }
    , capstone{ this->open_capstone() }
  {}

  static auto open_capstone() -> capstone_handle
  {
    auto capstone = capstone_handle::open(::CS_ARCH_X86, ::CS_MODE_64);
    capstone.option(::CS_OPT_DETAIL, ::CS_OPT_ON);
    capstone.option(::CS_OPT_SYNTAX, ::CS_OPT_SYNTAX_ATT);
    return capstone;
  }

  auto analyze_function_instructions(rseq_analysis& analysis) -> void
  {
    auto instructions =
      disassemble(this->capstone.get(),
                  /*code=*/this->function.instruction_bytes.data(),
                  /*code_size=*/this->function.instruction_bytes.size(),
                  /*address=*/this->function.base_address,
                  /*count=*/0);
    for (const auto& instruction : instructions) {
      this->analyze_instruction(instruction, analysis);
    }
  }

  auto analyze_instruction(const ::cs_insn& instruction,
                           rseq_analysis& analysis) const -> void
  {
    auto in_group = [&instruction](capstone_group_type group) noexcept->bool
    {
      return instruction_in_group(instruction, group);
    };

    if (this->instruction_within_critical_section(instruction)) {
      auto written_registers =
        registers_written_by_instruction(this->capstone.get(), instruction);
      // TODO(strager): What about esp and sp?
      auto modifies_stack_pointer =
        written_registers.contains(::X86_REG_RSP) || in_group(::X86_GRP_IRET);
      if (modifies_stack_pointer) {
        analysis.add_problem(rseq_problem::stack_pointer_modified{
          {
            .critical_section = this->critical_section,
          },
          .modifying_instruction_address = instruction.address,
          .modifying_instruction_string = instruction_string(instruction),
          .modifying_instruction_called_function =
            this->called_function_name(instruction).value_or(""),
        });
      }

      auto interrupts = in_group(::CS_GRP_INT);
      if (interrupts) {
        analysis.add_problem(rseq_problem::interrupt{
          {
            .critical_section = this->critical_section,
          },
          .interrupt_instruction_address = instruction.address,
          .interrupt_instruction_string = instruction_string(instruction),
        });
      }
    } else {
      auto jumps = in_group(::X86_GRP_JUMP) || in_group(::X86_GRP_CALL);
      if (jumps) {
        auto target = jump_target(instruction);
        if (target.has_value()) {
          if (this->address_within_critical_section(*target) &&
              *target != this->critical_section.start_address) {
            analysis.add_problem(rseq_problem::jump_into_critical_section{
              {
                .critical_section = this->critical_section,
              },
              .jump_instruction_address = instruction.address,
              .jump_instruction_string = instruction_string(instruction),
              .target_instruction_address = *target,
            });
          }
        }
      }
    }
  }

  auto analyze_abort_signature(rseq_analysis& analysis) const -> void
  {
    auto abort_address = this->critical_section.abort_address;
    const auto function_begin_address = this->function.base_address;
    const auto function_end_address =
      this->function.base_address + this->function.instruction_bytes.size();
    auto abort_address_or_abort_signature_within_function =
      function_begin_address <= abort_address &&
      abort_address - rseq_signature_size < function_end_address;
    if (!abort_address_or_abort_signature_within_function) {
      return;
    }

    auto actual_signature =
      std::array<std::optional<std::byte>, rseq_signature_size>{};
    const auto abort_signature_address = abort_address - rseq_signature_size;
    for (auto i = std::size_t{ 0 }; i < actual_signature.size(); ++i) {
      auto address = abort_signature_address + i;
      if (this->address_within_function(address)) {
        auto instruction_bytes_offset = address - function_begin_address;
        actual_signature[i] =
          this->function.instruction_bytes[instruction_bytes_offset];
      } else {
        actual_signature[i] = std::nullopt;
      }
    }

    auto expected_signature =
      rseq_signature_for_architecture(this->function.architecture);
    if (!std::equal(expected_signature.begin(),
                    expected_signature.end(),
                    actual_signature.begin(),
                    actual_signature.end())) {
      analysis.add_problem(rseq_problem::invalid_abort_signature{
        {
          .critical_section = this->critical_section,
        },
        .expected_signature = expected_signature,
        .actual_signature = actual_signature,
      });
    }
  }

  auto analyze_address_bounds(rseq_analysis& analysis) const -> void
  {
    if (this->critical_section.start_address ==
        this->critical_section.post_commit_address) {
      analysis.add_problem(rseq_problem::empty_critical_section{
        {
          .critical_section = this->critical_section,
        },
      });
    }
    if (this->critical_section.post_commit_address <
        this->critical_section.start_address) {
      analysis.add_problem(rseq_problem::inverted_critical_section{
        {
          .critical_section = this->critical_section,
        },
      });
    }
    if (!this->address_within_function(this->critical_section.start_address)) {
      analysis.add_problem(rseq_problem::label_outside_function{
        {
          .critical_section = this->critical_section,
        },
        .label_kind = rseq_problem::label_outside_function::kind::start,
      });
    }
    if (!this->address_within_function(
          this->critical_section.post_commit_address)) {
      analysis.add_problem(rseq_problem::label_outside_function{
        {
          .critical_section = this->critical_section,
        },
        .label_kind = rseq_problem::label_outside_function::kind::post_commit,
      });
    }
    if (!this->address_within_function(this->critical_section.abort_address)) {
      analysis.add_problem(rseq_problem::label_outside_function{
        {
          .critical_section = this->critical_section,
        },
        .label_kind = rseq_problem::label_outside_function::kind::abort,
      });
    }
  }

  auto called_function_name(const ::cs_insn& instruction) const
    -> std::optional<std::string>
  {
    if (!this->elf_) {
      return std::nullopt;
    }
    switch (instruction.id) {
      case ::X86_INS_CALL: {
        auto optional_callee_address =
          this->direct_target_of_control_flow_instruction(instruction);
        if (!optional_callee_address.has_value()) {
          return std::nullopt;
        }
        return this->name_of_function(*optional_callee_address);
      }
      default:
        return std::nullopt;
    }
  }

  auto name_of_function(machine_address function_address) const
    -> std::optional<std::string>
  {
    if (auto name = this->name_of_function_by_symbol(function_address)) {
      return name;
    }
    if (auto name = this->name_of_plt_function(function_address)) {
      return *name + "@PLT";
    }
    return std::nullopt;
  }

  auto name_of_function_by_symbol(machine_address function_address) const
    -> std::optional<std::string>
  {
    assert(this->elf_);
    try {
      auto called_function =
        function_containing_address(this->elf_, function_address);
      return std::move(called_function).name;
    } catch (const no_function_containing_address&) {
      return std::nullopt;
    }
  }

  auto name_of_plt_function(machine_address plt_function_address) const
    -> std::optional<std::string>
  {
    // Example ELF:
    //
    // // In .plt section:
    // 0x00000560: ff 25 b2 0a 20 00 jmpq *0x200ab2(%rip) // 0x00201018
    //
    // // In .got.plt section:
    // 0x00201018: 66 05 00 00 00 00 00 00
    //
    // // In .rela.plt section:
    // //          Info         Type               Symbol
    // 0x00201018: 000100000007 R_X86_64_JUMP_SLOT _my_function
    //
    // // In .dynsym section:
    // Num: Value             Size Type   Bind   Vis      Ndx Name
    //   0: 0000000000000000     0 NOTYPE LOCAL  DEFAULT  UND
    //   1: 0000000000000000     0 NOTYPE GLOBAL DEFAULT  UND _my_function
    //
    // If plt_function_address == 0x00000560, then name_of_plt_function should
    // return "_my_function".

    assert(this->elf_);
    auto optional_got_entry =
      this->global_offset_table_entry_from_plt_entry(plt_function_address);
    if (!optional_got_entry.has_value()) {
      return std::nullopt;
    }
    auto relocation = this->find_global_offset_table_entry_relocation(
      this->elf_, *optional_got_entry);
    if (!relocation.has_value()) {
      return std::nullopt;
    }
    if (relocation->relocation_type != R_X86_64_JUMP_SLOT) {
      return std::nullopt;
    }

    auto dynsym_section =
      ::elf_getscn(this->elf_, relocation->dynsym_section_index);
    auto dynsym_section_header = section_header(dynsym_section);
    assert(dynsym_section_header.sh_type == SHT_DYNSYM);

    auto symbol = this->get_symbol(dynsym_section, relocation->symbol_index);
    if (!symbol.has_value()) {
      return std::nullopt;
    }

    const auto* name =
      ::elf_strptr(this->elf_, dynsym_section_header.sh_link, symbol->st_name);
    if (name) {
      return name;
    } else {
      return std::nullopt;
    }
  }

  struct global_offset_table_relocation
  {
    ::Elf64_Xword relocation_type;
    ::Elf64_Word dynsym_section_index;
    ::Elf64_Xword symbol_index;
  };

  static auto find_global_offset_table_entry_relocation(
    ::Elf* elf,
    machine_address got_entry_address)
    -> std::optional<global_offset_table_relocation>
  {
    auto result = std::optional<global_offset_table_relocation>{};
    enumerate_rela_entries(
      elf,
      [&](::Elf_Scn*,
          const ::GElf_Shdr& section_header,
          const ::GElf_Rela& entry) -> void {
        if (entry.r_offset == got_entry_address) {
          if (result.has_value()) {
            throw std::runtime_error{ "Found multiple relocations at the same "
                                      "address in the global offset table" };
          }
          result = global_offset_table_relocation{
            .relocation_type = GELF_R_TYPE(entry.r_info),
            .dynsym_section_index = section_header.sh_link,
            .symbol_index = GELF_R_SYM(entry.r_info),
          };
        }
      });
    return result;
  }

  static auto get_symbol(::Elf_Scn* section, std::uint64_t entry_index)
    -> std::optional<::GElf_Sym>
  {
    auto entry = get_section_entry(section, ELF_T_SYM, entry_index);
    if (!entry.has_value()) {
      return std::nullopt;
    }
    ::GElf_Sym symbol /* uninitialized */;
    if (!::gelf_getsym(entry->chunk, entry->index_in_chunk, &symbol)) {
      throw_libelf_error();
    }
    return symbol;
  }

  auto global_offset_table_entry_from_plt_entry(
    machine_address plt_entry_address) const -> std::optional<machine_address>
  {
    // Example ELF:
    //
    // // In .plt section:
    // 0x00000560: ff 25 b2 0a 20 00 jmpq *0x200ab2(%rip) // 0x00201018
    //
    // // In .got.plt section:
    // 0x00201018: 66 05 00 00 00 00 00 00
    //
    // If plt_entry_address == 0x00000560, then
    // global_offset_table_entry_from_plt_entry should return 0x00201018.

    assert(this->elf_);
    auto sections = std::vector<::Elf_Scn*>{};
    std::copy_if(elf_section_range{ this->elf_ }.begin(),
                 elf_section_range{ this->elf_ }.end(),
                 std::back_inserter(sections),
                 [&](::Elf_Scn* section) -> bool {
                   auto header = section_header(section);
                   auto is_possibly_plt_section =
                     (header.sh_flags & SHF_ALLOC) == SHF_ALLOC;
                   return is_possibly_plt_section &&
                          section_contains_address(header, plt_entry_address);
                 });
    if (sections.empty()) {
      return std::nullopt;
    }
    if (sections.size() > 1) {
      throw std::runtime_error{
        "Unexpectedly found multiple sections containing PLT entry"
      };
    }
    const auto& section = sections.front();
    auto section_bytes = section_data(section);

    auto instruction_begin_offset =
      plt_entry_address - section_header(section).sh_addr;
    auto instruction_begin_it =
      section_bytes.begin() + instruction_begin_offset;
    auto instructions =
      disassemble(this->capstone.get(),
                  /*code=*/&*instruction_begin_it,
                  /*code_size=*/section_bytes.end() - instruction_begin_it,
                  /*address=*/plt_entry_address,
                  /*count=*/1);
    if (instructions.empty()) {
      return std::nullopt;
    }
    const auto& instruction = instructions.front();
    switch (instruction.id) {
      case ::X86_INS_JMP:
        return this->indirect_target_of_control_flow_instruction(instruction);
      default:
        return std::nullopt;
    }
  }

  static auto direct_target_of_control_flow_instruction(
    const ::cs_insn& instruction) -> std::optional<machine_address>
  {
    if (!(instruction.id == ::X86_INS_JMP ||
          instruction.id == ::X86_INS_CALL)) {
      return std::nullopt;
    }
    const auto& instruction_details = instruction.detail->x86;
    if (instruction_details.op_count != 1) {
      return std::nullopt;
    }
    const auto& operand = instruction_details.operands[0];
    switch (operand.type) {
      case ::X86_OP_IMM:
        return operand.imm;
      default:
        return std::nullopt;
    }
  }

  static auto indirect_target_of_control_flow_instruction(
    const ::cs_insn& instruction) -> std::optional<machine_address>
  {
    if (!(instruction.id == ::X86_INS_JMP ||
          instruction.id == ::X86_INS_CALL)) {
      return std::nullopt;
    }
    const auto& instruction_details = instruction.detail->x86;
    if (instruction_details.op_count != 1) {
      return std::nullopt;
    }
    const auto& operand = instruction_details.operands[0];
    switch (operand.type) {
      case ::X86_OP_MEM:
        if (operand.mem.base == ::X86_REG_RIP && operand.mem.scale == 1 &&
            operand.mem.segment == ::X86_REG_INVALID &&
            operand.mem.index == ::X86_REG_INVALID) {
          return instruction.address + instruction.size + operand.mem.disp;
        }
        return std::nullopt;
      default:
        return std::nullopt;
    }
  }

  auto address_within_function(machine_address address) const noexcept -> bool
  {
    auto function_begin_address = this->function.base_address;
    auto function_end_address =
      this->function.base_address + this->function.instruction_bytes.size();
    return function_begin_address <= address && address < function_end_address;
  }

  auto address_within_critical_section(machine_address address) const noexcept
    -> bool
  {
    return this->critical_section.start_address <= address &&
           address < this->critical_section.post_commit_address;
  }

  auto instruction_within_critical_section(const ::cs_insn& instruction) const
    noexcept -> bool
  {
    // TODO(strager): How should we treat instructions which are partially
    // within the critical section? (This could happen if Capstone's instruction
    // decoding went haywire, or if any of start_address and post_commit_address
    // is wrong.)
    auto instruction_begin = instruction.address;
    auto instruction_end = instruction.address + instruction.size;
    assert(instruction_end > instruction_begin);
    for (auto address = instruction_begin; address < instruction_end;
         ++address) {
      if (this->address_within_critical_section(address)) {
        return true;
      }
    }
    return false;
  }

private:
  ::Elf* elf_{ nullptr };
  const elf_function& function;
  const rseq_critical_section& critical_section;

  capstone_handle capstone;
};
}

namespace {
auto
analyze_rseq_critical_section(::Elf* elf,
                              const elf_function& function,
                              const rseq_critical_section& critical_section,
                              rseq_analysis& analysis)
{
  auto analyzer = rseq_analyzer{ elf, function, critical_section };
  if (function.instruction_bytes.empty()) {
    analysis.add_problem(rseq_problem::empty_function{
      {
        .critical_section = critical_section,
      },
    });
  } else {
    analyzer.analyze_function_instructions(analysis);
    analyzer.analyze_abort_signature(analysis);
    analyzer.analyze_address_bounds(analysis);
  }
}
}

auto
analyze_rseq_critical_sections_in_file(cxxtrace::czstring file_path)
  -> rseq_analysis
{
  initialize_libelf();
  auto elf_file = cxxtrace::detail::file_descriptor{ ::open(
    file_path, O_CLOEXEC | O_RDONLY) };
  if (!elf_file.valid()) {
    throw std::system_error{ errno, std::generic_category(), file_path };
  }
  auto elf = elf_handle::begin(elf_file.get(), ::ELF_C_READ);

  auto analysis = rseq_analysis{};
  auto descriptors = parse_rseq_descriptors(elf.get());
  if (descriptors.empty()) {
    analysis.add_problem(rseq_problem::no_rseq_descriptors{
      .section_name = rseq_descriptor_section_name });
  }
  for (const auto& rseq_descriptor : descriptors) {
    if (!rseq_descriptor.is_complete()) {
      analysis.add_problem(rseq_problem::incomplete_rseq_descriptor{
        .descriptor_address = rseq_descriptor.descriptor_address,
      });
      continue;
    }
    auto critical_section = rseq_critical_section{
      .descriptor_address = rseq_descriptor.descriptor_address,
      .function_address = 0,
      .function = "",
      .start_address = *rseq_descriptor.start_ip,
      .post_commit_address =
        *rseq_descriptor.start_ip + *rseq_descriptor.post_commit_offset,
      .abort_address = *rseq_descriptor.abort_ip,
    };
    // TODO(strager): Verify the version and flags fields.
    auto function = std::optional<elf_function>{};
    try {
      function =
        function_containing_address(elf.get(), critical_section.start_address);
    } catch (const no_function_containing_address&) {
      analysis.add_problem(rseq_problem::label_outside_function{
        {
          .critical_section = critical_section,
        },
        .label_kind = rseq_problem::label_outside_function::kind::start,
      });
    }
    if (!function.has_value()) {
      continue;
    }
    critical_section.function_address = function->base_address;
    critical_section.function = function->name;
    analyze_rseq_critical_section(
      elf.get(), *function, critical_section, analysis);
  }
  return analysis;
}

auto
analyze_rseq_critical_section(::Elf* elf,
                              const elf_function& function,
                              machine_address start_address,
                              machine_address post_commit_address,
                              machine_address abort_address) -> rseq_analysis
{
  auto critical_section = rseq_critical_section{
    .descriptor_address = 0,
    .function_address = function.base_address,
    .function = function.name,
    .start_address = start_address,
    .post_commit_address = post_commit_address,
    .abort_address = abort_address,
  };
  auto analysis = rseq_analysis{};
  analyze_rseq_critical_section(elf, function, critical_section, analysis);
  return analysis;
}

auto
parsed_rseq_descriptor::is_complete() const noexcept -> bool
{
  return this->version.has_value() && this->flags.has_value() &&
         this->start_ip.has_value() && this->post_commit_offset.has_value() &&
         this->abort_ip.has_value();
}

auto
parse_rseq_descriptors(::Elf* elf) -> std::vector<parsed_rseq_descriptor>
{
  auto parse_descriptor =
    [](binary_parser& parser,
       machine_address descriptor_address) -> parsed_rseq_descriptor {
    return parsed_rseq_descriptor{
      .descriptor_address = descriptor_address,
      .version = parser.parse_4_byte_unsigned_little_endian_integer(),
      .flags = parser.parse_4_byte_unsigned_little_endian_integer(),
      .start_ip = parser.parse_8_byte_unsigned_little_endian_integer(),
      .post_commit_offset =
        parser.parse_8_byte_unsigned_little_endian_integer(),
      .abort_ip = parser.parse_8_byte_unsigned_little_endian_integer(),
    };
  };

  auto descriptors = std::vector<parsed_rseq_descriptor>{};
  for (auto* section : elf_section_range{ elf }) {
    if (std::strcmp(section_name(elf, section), rseq_descriptor_section_name) ==
        0) {
      auto section_begin_address = section_header(section).sh_addr;
      auto bytes = section_data(section);
      auto parser = binary_parser{ bytes.data(), bytes.size() };
      while (parser.remaining_byte_count() > 0) {
        auto offset = parser.cursor() - bytes.data();
        descriptors.emplace_back(
          parse_descriptor(parser, section_begin_address + offset));
      }
    }
  }
  return descriptors;
}

auto
rseq_critical_section::size_in_bytes() const noexcept -> std::optional<int>
{
  if (this->post_commit_address < this->start_address) {
    return std::nullopt;
  }
  auto size = this->post_commit_address - this->start_address;
  constexpr auto maximum_size = 4 * 1024; // Arbitrary.
  if (size > maximum_size) {
    return std::nullopt;
  }
  return size;
}

auto
rseq_problem::empty_critical_section::critical_section_address() const
  -> machine_address
{
  assert(this->critical_section.start_address ==
         this->critical_section.post_commit_address);
  return this->critical_section.start_address;
}

auto
operator<<(std::ostream& stream, const rseq_problem::empty_critical_section&)
  -> std::ostream&
{
  stream << "critical section contains no instructions";
  return stream;
}

auto
operator<<(std::ostream& stream, const rseq_problem::empty_function&)
  -> std::ostream&
{
  stream << "function is empty";
  return stream;
}

auto
operator<<(std::ostream& stream,
           const rseq_problem::incomplete_rseq_descriptor& x) -> std::ostream&
{
  stream << "incomplete rseq_cs descriptor at " << std::hex << std::showbase
         << x.descriptor_address << std::dec << std::noshowbase << " (expected "
         << parsed_rseq_descriptor::size << " bytes)";
  cxxtrace::detail::reset_ostream_formatting(stream);
  return stream;
}

auto
operator<<(std::ostream& stream, const rseq_problem::interrupt& x)
  -> std::ostream&
{
  stream << "interrupting instruction at " << std::hex << std::showbase
         << x.interrupt_instruction_address << ": "
         << x.interrupt_instruction_string;
  cxxtrace::detail::reset_ostream_formatting(stream);
  return stream;
}

auto
operator<<(std::ostream& stream, const rseq_problem::invalid_abort_signature& x)
  -> std::ostream&
{
  stream << "invalid abort signature at " << std::hex << std::showbase
         << x.signature_address() << std::noshowbase << ": expected";
  for (auto byte : x.expected_signature) {
    stream << ' ' << std::setw(2) << std::setfill('0') << unsigned(byte);
  }
  stream << " but got";
  for (auto optional_byte : x.actual_signature) {
    stream << ' ';
    if (optional_byte.has_value()) {
      stream << std::setw(2) << unsigned(*optional_byte);
    } else {
      stream << "??";
    }
  }
  cxxtrace::detail::reset_ostream_formatting(stream);
  return stream;
}

auto
operator<<(std::ostream& stream, const rseq_problem::inverted_critical_section&)
  -> std::ostream&
{
  stream << "post-commit comes before start";
  return stream;
}

auto
operator<<(std::ostream& stream,
           const rseq_problem::jump_into_critical_section& x) -> std::ostream&
{
  stream << "jump into critical section at " << std::hex << std::showbase
         << x.jump_instruction_address << ": " << x.jump_instruction_string;
  cxxtrace::detail::reset_ostream_formatting(stream);
  return stream;
}

auto
rseq_problem::label_outside_function::label_address() const -> machine_address
{
  switch (this->label_kind) {
    case kind::start:
      return this->critical_section.start_address;
    case kind::post_commit:
      return this->critical_section.post_commit_address;
    case kind::abort:
      return this->critical_section.abort_address;
  }
  assert(false && "Unexpected label kind");
}

auto
operator<<(std::ostream& stream, const rseq_problem::label_outside_function& x)
  -> std::ostream&
{
  stream << "critical section ";
  switch (x.label_kind) {
    case rseq_problem::label_outside_function::kind::start:
      stream << "start";
      break;
    case rseq_problem::label_outside_function::kind::post_commit:
      stream << "post-commit";
      break;
    case rseq_problem::label_outside_function::kind::abort:
      stream << "abort";
      break;
  }
  stream << " is outside function";
  cxxtrace::detail::reset_ostream_formatting(stream);
  return stream;
}

auto
operator<<(std::ostream& stream, const rseq_problem::no_rseq_descriptors& x)
  -> std::ostream&
{
  stream << x.section_name << " section is missing or contains no descriptors";
  return stream;
}

auto
operator<<(std::ostream& stream, const rseq_problem::stack_pointer_modified& x)
  -> std::ostream&
{
  stream << "stack pointer modified at " << std::hex << std::showbase
         << x.modifying_instruction_address << ": "
         << x.modifying_instruction_string;
  if (!x.modifying_instruction_called_function.empty()) {
    stream << " // " << x.modifying_instruction_called_function;
  }
  cxxtrace::detail::reset_ostream_formatting(stream);
  return stream;
}

auto
PrintTo(const any_rseq_problem& x, std::ostream* output) -> void
{
  std::visit([&](auto&& x) { *output << x; }, x);
}
}
