#include "capstone.h"
#include "libelf_support.h"
#include "parse_binary.h"
#include "rseq_analyzer.h"
#include <capstone/capstone.h>
#include <cstdio>
#include <cxxtrace/detail/file_descriptor.h>
#include <cxxtrace/string.h>
#include <fcntl.h>
#include <iomanip>
#include <ostream>
#include <string>
#include <variant>

namespace cxxtrace_check_rseq {
namespace {
constexpr auto rseq_descriptor_section_name = ".data_cxxtrace_rseq";

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
  explicit rseq_analyzer(const elf_function& function,
                         const rseq_critical_section& critical_section)
    : function{ function }
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
    }

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
  const elf_function& function;
  const rseq_critical_section& critical_section;

  capstone_handle capstone;
};
}

namespace {
auto
analyze_rseq_critical_section(const elf_function& function,
                              const rseq_critical_section& critical_section,
                              rseq_analysis& analysis)
{
  auto analyzer = rseq_analyzer{ function, critical_section };
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
    analyze_rseq_critical_section(*function, critical_section, analysis);
  }
  return analysis;
}

auto
analyze_rseq_critical_section(const elf_function& function,
                              machine_address start_address,
                              machine_address post_commit_address,
                              machine_address abort_address) -> rseq_analysis
{
  auto critical_section = rseq_critical_section{
    .function_address = function.base_address,
    .function = function.name,
    .start_address = start_address,
    .post_commit_address = post_commit_address,
    .abort_address = abort_address,
  };
  auto analysis = rseq_analysis{};
  analyze_rseq_critical_section(function, critical_section, analysis);
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
  enumerate_sections_with_name(
    elf, rseq_descriptor_section_name, [&](::Elf_Scn* section) -> void {
      auto section_begin_address = section_header(section).sh_addr;
      auto bytes = section_data(section);
      auto parser = binary_parser{ bytes.data(), bytes.size() };
      while (parser.remaining_byte_count() > 0) {
        auto offset = parser.cursor() - bytes.data();
        descriptors.emplace_back(
          parse_descriptor(parser, section_begin_address + offset));
      }
    });
  return descriptors;
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
operator<<(std::ostream& stream, const rseq_problem::empty_critical_section& x)
  -> std::ostream&
{
  stream << x.critical_section_function() << '(' << std::hex << std::showbase
         << x.critical_section_address()
         << "): critical section contains no instructions";
  cxxtrace::detail::reset_ostream_formatting(stream);
  return stream;
}

auto
operator<<(std::ostream& stream, const rseq_problem::empty_function& x)
  -> std::ostream&
{
  stream << x.function_name() << '(' << std::hex << std::showbase
         << x.function_address() << "): function is empty";
  cxxtrace::detail::reset_ostream_formatting(stream);
  return stream;
}

auto
operator<<(std::ostream& stream,
           const rseq_problem::incomplete_rseq_descriptor& x) -> std::ostream&
{
  stream << std::hex << std::showbase << x.descriptor_address << std::dec
         << std::noshowbase << ": incomplete rseq_cs descriptor (expected "
         << parsed_rseq_descriptor::size << " bytes)";
  cxxtrace::detail::reset_ostream_formatting(stream);
  return stream;
}

auto
operator<<(std::ostream& stream, const rseq_problem::interrupt& x)
  -> std::ostream&
{
  stream << x.interrupt_instruction_function() << '(' << std::hex
         << std::showbase << x.interrupt_instruction_address
         << "): interrupting instruction: " << x.interrupt_instruction_string;
  cxxtrace::detail::reset_ostream_formatting(stream);
  return stream;
}

auto
operator<<(std::ostream& stream, const rseq_problem::invalid_abort_signature& x)
  -> std::ostream&
{
  stream << x.abort_function_name() << '(' << std::hex << std::showbase
         << x.signature_address() << std::noshowbase
         << "): invalid abort signature: expected";
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
operator<<(std::ostream& stream,
           const rseq_problem::inverted_critical_section& x) -> std::ostream&
{
  stream << std::hex << std::showbase << x.critical_section_function() << '('
         << x.critical_section_post_commit_address()
         << "): post-commit comes before start ("
         << x.critical_section_start_address() << ')';
  cxxtrace::detail::reset_ostream_formatting(stream);
  return stream;
}

auto
operator<<(std::ostream& stream,
           const rseq_problem::jump_into_critical_section& x) -> std::ostream&
{
  stream << x.jump_instruction_function() << '(' << std::hex << std::showbase
         << x.jump_instruction_address
         << "): jump into critical section: " << x.jump_instruction_string;
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
  stream << x.label_function() << '(' << std::hex << std::showbase
         << x.label_address() << "): critical section ";
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
  stream << x.modifying_instruction_function() << '(' << std::hex
         << std::showbase << x.modifying_instruction_address
         << "): stack pointer modified: " << x.modifying_instruction_string;
  cxxtrace::detail::reset_ostream_formatting(stream);
  return stream;
}

auto
PrintTo(const any_rseq_problem& x, std::ostream* output) -> void
{
  std::visit([&](auto&& x) { *output << x; }, x);
}
}
