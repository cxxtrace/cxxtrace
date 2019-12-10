#include "assembler.h"
#include "elf_function.h"
#include "machine_code.h"
#include "rseq_analyzer.h"
#include <array>
#include <cstddef>
#include <cxxtrace/string.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// TODO(strager): Support 32-bit x86.
// TODO(strager): Don't report calls to noreturn functions (such as _Exit).
// TODO(strager): Report registers modified by the critical section and read by
// abort_ip.
// TODO(strager): Report unnecessary nop padding before start and post-commit
// addresses. Such padding could be inserted by the compiler to align the
// addresses. (Jumps to aligned addresses may be an optimization. For rseq, the
// kernel never jumps to the addresses, so the optimization is a pessimisation.

using cxxtrace_check_rseq::machine_architecture;
using cxxtrace_check_rseq::rseq_problem;
using namespace std::literals::string_literals;
using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;
using testing::VariantWith;

#define FIELD_EQ(_class, _member, ...)                                         \
  FIELD(_class, _member, decltype(_class::_member){ __VA_ARGS__ })

#define FIELD(_class, _member, ...)                                            \
  (::testing::Field(#_member, &_class::_member, __VA_ARGS__))

#define PROPERTY_EQ(_class, _member, ...)                                      \
  PROPERTY(_class, _member, TYPE_OF_PROPERTY(_class, _member){ __VA_ARGS__ })

#define TYPE_OF_PROPERTY(_class, _member)                                      \
  std::remove_reference_t<decltype(std::declval<const _class&>()._member())>

#define PROPERTY(_class, _member, ...)                                         \
  (::testing::Property(#_member, &_class::_member, __VA_ARGS__))

namespace cxxtrace_test {
namespace {
constexpr auto operator""_b(unsigned long long x) -> std::byte
{
  return std::byte(x);
}

// Emit the rseq signature which must be place immediately before the abort
// label.
//
// See librseq [1] for an explanation of the undefined instruction and for the
// source of the signature itself.
//
// [1] https://github.com/compudj/librseq
constexpr auto abort_signature_x86 = R"(
  /* BEGIN ABORT SIGNATURE */
  .byte 0x0f, 0xb9, 0x3d // Undefined instruction with 4-byte immediate.
  .long 0x53053053       // The signature.
  /* END ABORT SIGNATURE */
)";

auto
analyze_rseq_critical_section(const temporary_elf_file& elf,
                              cxxtrace::czstring function_name,
                              cxxtrace::czstring start_symbol,
                              cxxtrace::czstring post_commit_symbol,
                              cxxtrace::czstring abort_symbol)
  -> cxxtrace_check_rseq::rseq_analysis
{
  auto function = function_from_symbol(elf.elf(), function_name);
  return analyze_rseq_critical_section(
    function,
    /*start_address=*/elf.symbol(start_symbol),
    /*post_commit_address=*/elf.symbol(post_commit_symbol),
    /*abort_address=*/elf.symbol(abort_symbol));
}

auto
stub_critical_section() -> cxxtrace_check_rseq::rseq_critical_section;

auto
assert_stream_has_default_formatting(std::ostream&) -> void;
}

TEST(test_rseq_analyzer, parse_shared_library_with_no_rseq_descriptors)
{
  auto assembly_code = R"(
    // Empty file.
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86, assembly_code, {});

  auto descriptors = cxxtrace_check_rseq::parse_rseq_descriptors(elf_so.elf());
  EXPECT_THAT(descriptors, IsEmpty());
}

TEST(test_rseq_analyzer, parse_single_rseq_descriptor_from_shared_library)
{
  auto assembly_code = R"(
    .section .data_cxxtrace_rseq, "a", @progbits
    .align 32
    .type _my_rseq_descriptor, @object
    _my_rseq_descriptor:
      .long 0x11223344 // Address: 0x0010000100002000
      .long 0x55667788
      .quad .start
      .quad .post_commit - .start
      .quad .abort
    .size _my_rseq_descriptor, . - _my_rseq_descriptor

    .section .text_cxxtrace_test, "ax", @progbits
    .globl _my_rseq_function
    .type _my_rseq_function, @function
    _my_rseq_function:
      nop // Address: 0x0000007f68000000
    .start:
      nop // Address: 0x0000007f68000001
    .post_commit:
      ret // Address: 0x0000007f68000002
    .abort:
      ret // Address: 0x0000007f68000002
    .size _my_rseq_function, . - _my_rseq_function
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86,
    assembly_code,
    {
      "-Wl,--section-start=.data_cxxtrace_rseq=0010000100002000",
      "-Wl,--section-start=.text_cxxtrace_test=0000007f68000000",
    });

  auto descriptors = cxxtrace_check_rseq::parse_rseq_descriptors(elf_so.elf());
  EXPECT_EQ(descriptors.size(), 1);
  ASSERT_GE(descriptors.size(), 1);
  const auto& descriptor = descriptors.at(0);
  EXPECT_EQ(descriptor.descriptor_address, 0x0010000100002000);
  EXPECT_EQ(descriptor.version, 0x11223344);
  EXPECT_EQ(descriptor.flags, 0x55667788);
  EXPECT_EQ(descriptor.start_ip, 0x0000007f68000001);
  EXPECT_EQ(descriptor.post_commit_offset,
            0x0000007f68000002 - 0x0000007f68000001);
  EXPECT_EQ(descriptor.abort_ip, 0x0000007f68000003);
}

TEST(test_rseq_analyzer, parse_two_rseq_descriptors_from_shared_library)
{
  auto assembly_code = R"(
    .section .data_cxxtrace_rseq, "a", @progbits
    .align 32
    .type _my_rseq_descriptor_1, @object
    _my_rseq_descriptor_1:
      .long 0x11223344 // Address: 0x0000560800002000
      .long 0x55667788
      .quad .start_1
      .quad .post_commit_1 - .start_1
      .quad .abort_1
    .size _my_rseq_descriptor_1, . - _my_rseq_descriptor_1
    .align 32
    .type _my_rseq_descriptor_2, @object
    _my_rseq_descriptor_2:
      .long 0x99aabbcc // Address: 0x0000560800002020
      .long 0xddeeff00
      .quad .start_2
      .quad .post_commit_2 - .start_2
      .quad .abort_2
    .size _my_rseq_descriptor_2, . - _my_rseq_descriptor_2

    .section .text_cxxtrace_test, "ax", @progbits
    .globl _my_rseq_function_1
    .type _my_rseq_function_1, @function
    _my_rseq_function_1:
      nop // Address: 0x0000555555554000
    .start_1:
      nop // Address: 0x0000555555554001
    .post_commit_1:
      ret // Address: 0x0000555555554002
    .abort_1:
      ret // Address: 0x0000555555554003
    .size _my_rseq_function_1, . - _my_rseq_function_1

    .globl _my_rseq_function_2
    .type _my_rseq_function_2, @function
    _my_rseq_function_2:
      nop // Address: 0x0000555555554004
      nop // Address: 0x0000555555554005
      nop // Address: 0x0000555555554006
    .start_2:
      nop // Address: 0x0000555555554007
      nop // Address: 0x0000555555554008
      nop // Address: 0x0000555555554009
    .post_commit_2:
      ret // Address: 0x000055555555400a
      ret // Address: 0x000055555555400b
      ret // Address: 0x000055555555400c
    .abort_2:
      ret // Address: 0x000055555555400d
      ret // Address: 0x000055555555400e
      ret // Address: 0x000055555555400f
    .size _my_rseq_function_2, . - _my_rseq_function_2
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86,
    assembly_code,
    {
      "-Wl,--section-start=.data_cxxtrace_rseq=0000560800002000",
      "-Wl,--section-start=.text_cxxtrace_test=0000555555554000",
    });

  auto descriptors = cxxtrace_check_rseq::parse_rseq_descriptors(elf_so.elf());
  EXPECT_EQ(descriptors.size(), 2);
  ASSERT_GE(descriptors.size(), 2);
  // TODO(strager): Should we care about the order of the descriptors?
  {
    const auto& descriptor = descriptors.at(0);
    EXPECT_EQ(descriptor.descriptor_address, 0x0000560800002000);
    EXPECT_EQ(descriptor.version, 0x11223344);
    EXPECT_EQ(descriptor.flags, 0x55667788);
    EXPECT_EQ(descriptor.start_ip, 0x0000555555554001);
    EXPECT_EQ(descriptor.post_commit_offset,
              0x0000555555554002 - 0x0000555555554001);
    EXPECT_EQ(descriptor.abort_ip, 0x0000555555554003);
  }
  {
    const auto& descriptor = descriptors.at(1);
    EXPECT_EQ(descriptor.descriptor_address, 0x0000560800002020);
    EXPECT_EQ(descriptor.version, 0x99aabbcc);
    EXPECT_EQ(descriptor.flags, 0xddeeff00);
    EXPECT_EQ(descriptor.start_ip, 0x0000555555554007);
    EXPECT_EQ(descriptor.post_commit_offset,
              0x000055555555400a - 0x0000555555554007);
    EXPECT_EQ(descriptor.abort_ip, 0x000055555555400d);
  }
}

TEST(test_rseq_analyzer, parse_incomplete_rseq_descriptor_from_shared_library)
{
  auto assembly_code = R"(
    .section .data_cxxtrace_rseq, "a", @progbits
    .align 32
    .type _my_rseq_descriptor, @object
    _my_rseq_descriptor:
      .long 1 // Address: 0x0000000100002000
      .long 2
      .quad 3
      // Partially missing: post_commit_offset: .quad X
      .long 4
      // Missing: abort_ip: .quad Y
    .size _my_rseq_descriptor, 32
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86,
    assembly_code,
    {
      "-Wl,--section-start=.data_cxxtrace_rseq=0000000100002000",
    });

  auto descriptors = cxxtrace_check_rseq::parse_rseq_descriptors(elf_so.elf());
  EXPECT_EQ(descriptors.size(), 1);
  ASSERT_GE(descriptors.size(), 1);
  const auto& descriptor = descriptors.at(0);
  EXPECT_EQ(descriptor.descriptor_address, 0x0000000100002000);
  EXPECT_EQ(descriptor.version, 1);
  EXPECT_EQ(descriptor.flags, 2);
  EXPECT_EQ(descriptor.start_ip, 3);
  EXPECT_EQ(descriptor.post_commit_offset, std::nullopt);
  EXPECT_EQ(descriptor.abort_ip, std::nullopt);
}

TEST(test_rseq_analyzer_x86, empty_function_is_disallowed)
{
  auto function = cxxtrace_check_rseq::elf_function{
    .name = "empty_function",
    .instruction_bytes = {},
    .base_address = 0x1000,
    .architecture = machine_architecture::x86,
  };
  auto analysis = analyze_rseq_critical_section(function,
                                                /*start_address=*/0x1000,
                                                /*post_commit_address=*/0x1000,
                                                /*abort_address=*/0x1000);
  EXPECT_THAT(
    analysis.all_problems(),
    ElementsAre(VariantWith<rseq_problem::empty_function>(AllOf(
      PROPERTY_EQ(rseq_problem::empty_function, function_address, 0x1000),
      PROPERTY_EQ(
        rseq_problem::empty_function, function_name, "empty_function")))));
}

TEST(test_rseq_analyzer_x86, empty_critical_section_is_disallowed)
{
  auto code = R"(
      push %rax
    start:
    post_commit:
      pop %rax
      ret
  )"s + abort_signature_x86 +
              R"(
    abort:
      pop %rax
      ret
  )";
  auto elf = assemble_function_into_elf_shared_object(
    machine_architecture::x86, code, "unimportant_function");
  auto analysis = analyze_rseq_critical_section(
    elf, "unimportant_function", "start", "post_commit", "abort");
  EXPECT_THAT(analysis.all_problems(),
              ElementsAre(VariantWith<rseq_problem::empty_critical_section>(
                AllOf(PROPERTY_EQ(rseq_problem::empty_critical_section,
                                  critical_section_address,
                                  elf.symbol("start")),
                      PROPERTY_EQ(rseq_problem::empty_critical_section,
                                  critical_section_function,
                                  "unimportant_function")))));
}

TEST(test_rseq_analyzer_x86, label_outside_function_is_disallowed)
{
  auto function = cxxtrace_check_rseq::elf_function{
    .name = "test_function",
    .instruction_bytes = {
      // xor %eax, %eax
      0x31_b, 0xc0_b,
    },
    .base_address = 0x1000,
    .architecture = machine_architecture::x86,
  };
  auto start_address = cxxtrace_check_rseq::machine_address{ 0x2000 };
  auto post_commit_address = cxxtrace_check_rseq::machine_address{ 0x2001 };
  auto abort_address = cxxtrace_check_rseq::machine_address{ 0x2002 };
  auto analysis =
    analyze_rseq_critical_section(function,
                                  /*start_address=*/start_address,
                                  /*post_commit_address=*/post_commit_address,
                                  /*abort_address=*/abort_address);
  EXPECT_THAT(
    analysis.all_problems(),
    UnorderedElementsAre(
      VariantWith<rseq_problem::label_outside_function>(AllOf(
        PROPERTY_EQ(
          rseq_problem::label_outside_function, label_address, start_address),
        PROPERTY_EQ(rseq_problem::label_outside_function,
                    label_function,
                    "test_function"),
        FIELD_EQ(rseq_problem::label_outside_function,
                 label_kind,
                 rseq_problem::label_outside_function::kind::start))),
      VariantWith<rseq_problem::label_outside_function>(AllOf(
        PROPERTY_EQ(rseq_problem::label_outside_function,
                    label_address,
                    post_commit_address),
        PROPERTY_EQ(rseq_problem::label_outside_function,
                    label_function,
                    "test_function"),
        FIELD_EQ(rseq_problem::label_outside_function,
                 label_kind,
                 rseq_problem::label_outside_function::kind::post_commit))),
      VariantWith<rseq_problem::label_outside_function>(AllOf(
        PROPERTY_EQ(
          rseq_problem::label_outside_function, label_address, abort_address),
        PROPERTY_EQ(rseq_problem::label_outside_function,
                    label_function,
                    "test_function"),
        FIELD_EQ(rseq_problem::label_outside_function,
                 label_kind,
                 rseq_problem::label_outside_function::kind::abort)))));
}

TEST(test_rseq_analyzer_x86, end_of_critical_section_must_follow_beginning)
{
  auto code = R"(
    post_commit:
      nop
    start:
      nop
  )"s + abort_signature_x86 +
              R"(
    abort:
      nop
  )";
  auto elf = assemble_function_into_elf_shared_object(
    machine_architecture::x86, code, "backward_function");
  auto analysis = analyze_rseq_critical_section(
    elf, "backward_function", "start", "post_commit", "abort");
  ASSERT_LT(elf.symbol("post_commit"), elf.symbol("start"));
  EXPECT_THAT(
    analysis.all_problems(),
    UnorderedElementsAre(VariantWith<rseq_problem::inverted_critical_section>(
      AllOf(PROPERTY_EQ(rseq_problem::inverted_critical_section,
                        critical_section_start_address,
                        elf.symbol("start")),
            PROPERTY_EQ(rseq_problem::inverted_critical_section,
                        critical_section_post_commit_address,
                        elf.symbol("post_commit")),
            PROPERTY_EQ(rseq_problem::inverted_critical_section,
                        critical_section_function,
                        "backward_function")))));
}

TEST(test_rseq_analyzer_x86, modifying_rsp_in_critical_section_is_disallowed)
{
  {
    auto code = R"(
      start:
      push_instruction:
        push %rax
      pop_instruction:
        pop %rsi
      post_commit:
    )"s + abort_signature_x86 +
                R"(
      abort:
        ret
    )";
    auto elf = assemble_function_into_elf_shared_object(
      machine_architecture::x86, code, "test_function");
    auto analysis = analyze_rseq_critical_section(
      elf, "test_function", "start", "post_commit", "abort");
    EXPECT_THAT(
      analysis.all_problems(),
      ElementsAre(VariantWith<rseq_problem::stack_pointer_modified>(
                    AllOf(FIELD_EQ(rseq_problem::stack_pointer_modified,
                                   modifying_instruction_address,
                                   elf.symbol("push_instruction")),
                          FIELD_EQ(rseq_problem::stack_pointer_modified,
                                   modifying_instruction_string,
                                   "pushq %rax"),
                          PROPERTY_EQ(rseq_problem::stack_pointer_modified,
                                      modifying_instruction_function,
                                      "test_function"))),
                  VariantWith<rseq_problem::stack_pointer_modified>(
                    AllOf(FIELD_EQ(rseq_problem::stack_pointer_modified,
                                   modifying_instruction_address,
                                   elf.symbol("pop_instruction")),
                          FIELD_EQ(rseq_problem::stack_pointer_modified,
                                   modifying_instruction_string,
                                   "popq %rsi"),
                          PROPERTY_EQ(rseq_problem::stack_pointer_modified,
                                      modifying_instruction_function,
                                      "test_function")))));
  }

  for (const auto* instruction : { "retq",
                                   "subq $0x16, %rsp",
                                   "callq *%rsi",
                                   "callq *0(%rbx)",
                                   "iretq" }) {
    auto code = R"(
      start:
      bad_instruction: )"s +
                instruction + R"(
      post_commit:
    )"s + abort_signature_x86 +
                R"(
      abort:
        ret
    )"s;
    SCOPED_TRACE("code: " + code);
    auto elf = assemble_function_into_elf_shared_object(
      machine_architecture::x86, code, "fancy_function");
    auto analysis = analyze_rseq_critical_section(
      elf, "fancy_function", "start", "post_commit", "abort");
    EXPECT_THAT(analysis.all_problems(),
                ElementsAre(VariantWith<rseq_problem::stack_pointer_modified>(
                  AllOf(FIELD_EQ(rseq_problem::stack_pointer_modified,
                                 modifying_instruction_address,
                                 elf.symbol("bad_instruction")),
                        FIELD_EQ(rseq_problem::stack_pointer_modified,
                                 modifying_instruction_string,
                                 instruction),
                        PROPERTY_EQ(rseq_problem::stack_pointer_modified,
                                    modifying_instruction_function,
                                    "fancy_function")))));
  }
}

TEST(test_rseq_analyzer_x86,
     calling_local_function_in_critical_section_is_disallowed)
{
  auto assembly_code = R"(
      .section .data_cxxtrace_rseq, "a", @progbits
        .long 0
        .long 0
        .quad _start
        .quad _post_commit - _start
        .quad _abort

      .text
      .globl _my_function
      .type _my_function, @function
      _my_function:

      _start:
        nop
      _bad_instruction:
        callq _other_function
      _post_commit:
        ret

    )"s + abort_signature_x86 +
                       R"(
      _abort:
        ret

      .size _my_function, . - _my_function

      .type _other_function, @function
      _other_function:
        ret
      .size _other_function, . - _other_function
    )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86, assembly_code.c_str(), {});

  auto analysis = cxxtrace_check_rseq::analyze_rseq_critical_sections_in_file(
    elf_so.elf_path());
  EXPECT_THAT(analysis.all_problems(),
              ElementsAre(VariantWith<rseq_problem::stack_pointer_modified>(
                AllOf(FIELD_EQ(rseq_problem::stack_pointer_modified,
                               modifying_instruction_address,
                               elf_so.symbol("_bad_instruction")),
                      FIELD_EQ(rseq_problem::stack_pointer_modified,
                               modifying_instruction_called_function,
                               "_other_function")))));
}

TEST(test_rseq_analyzer_x86,
     calling_external_function_in_critical_section_is_disallowed)
{
  auto assembly_code = R"(
      .section .data_cxxtrace_rseq, "a", @progbits
        .long 0
        .long 0
        .quad _start
        .quad _post_commit - _start
        .quad _abort

      .text

        // Create a dummy PLT entry to prevent a naive implementation from
        // passing this test.
        callq _undefined_function_1@PLT

      .globl _my_function
      .type _my_function, @function
      _my_function:

      _start:
        nop
      _bad_instruction:
        callq _undefined_function_2@PLT
      _post_commit:
        ret

    )"s + abort_signature_x86 +
                       R"(
      _abort:
        ret

        callq _undefined_function_3@PLT

      .size _my_function, . - _my_function
    )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86, assembly_code.c_str(), {});

  auto analysis = cxxtrace_check_rseq::analyze_rseq_critical_sections_in_file(
    elf_so.elf_path());
  EXPECT_THAT(analysis.all_problems(),
              ElementsAre(VariantWith<rseq_problem::stack_pointer_modified>(
                AllOf(FIELD_EQ(rseq_problem::stack_pointer_modified,
                               modifying_instruction_address,
                               elf_so.symbol("_bad_instruction")),
                      FIELD_EQ(rseq_problem::stack_pointer_modified,
                               modifying_instruction_called_function,
                               "_undefined_function_2@PLT")))));
}

TEST(test_rseq_analyzer_x86, interrupt_in_critical_section_is_disallowed)
{
  for (const auto* instruction : { "syscall", "int3", "int $5" }) {
    auto code = R"(
      start:
      bad_instruction: )"s +
                instruction + R"(
      post_commit:
    )"s + abort_signature_x86 +
                R"(
      abort:
        ret
    )";
    SCOPED_TRACE("code: " + code);
    auto elf = assemble_function_into_elf_shared_object(
      machine_architecture::x86, code, "test_function");
    auto analysis = analyze_rseq_critical_section(
      elf, "test_function", "start", "post_commit", "abort");
    EXPECT_THAT(
      analysis.all_problems(),
      ElementsAre(VariantWith<rseq_problem::interrupt>(AllOf(
        FIELD_EQ(rseq_problem::interrupt,
                 interrupt_instruction_address,
                 elf.symbol("bad_instruction")),
        FIELD_EQ(
          rseq_problem::interrupt, interrupt_instruction_string, instruction),
        PROPERTY_EQ(rseq_problem::interrupt,
                    interrupt_instruction_function,
                    "test_function")))));
  }
}

TEST(test_rseq_analyzer_x86, modifying_rsp_outside_critical_section_is_allowed)
{
  auto code = R"(
      push %rbp
    start:
      xor %eax, %eax
    post_commit:
      pop %rbp
      ret
  )"s + abort_signature_x86 +
              R"(
    abort:
      pop %rbp
      ret
  )";
  auto elf = assemble_function_into_elf_shared_object(
    machine_architecture::x86, code, "test_function");
  auto analysis = analyze_rseq_critical_section(
    elf, "test_function", "start", "post_commit", "abort");
  EXPECT_THAT(analysis.all_problems(), IsEmpty());
}

TEST(test_rseq_analyzer_x86,
     data_instructions_inside_critical_section_are_allowed)
{
  auto code = R"(
    start:
      movq $1, (%rax)
      incq 4(%rax)
    post_commit:
  )"s + abort_signature_x86 +
              R"(
    abort:
      ret
  )";
  auto elf = assemble_function_into_elf_shared_object(
    machine_architecture::x86, code, "test_function");
  auto analysis = analyze_rseq_critical_section(
    elf, "test_function", "start", "post_commit", "abort");
  EXPECT_THAT(analysis.all_problems(), IsEmpty());
}

TEST(test_rseq_analyzer_x86, jumping_into_critical_section_is_disallowed)
{
  enum jump_location
  {
    before_start,
    post_commit,
    post_abort,
  };

  for (const auto* jump_instruction :
       { "jmp jump_target", "jz jump_target", "call jump_target" }) {
    auto bad_instruction_line = "bad_instruction: "s + jump_instruction + "\n";
    for (auto jump_location : { before_start, post_commit, post_abort }) {
      auto code = R"(
          incq %rax
      )" + (jump_location == before_start ? bad_instruction_line : ""s) +
                  R"(
          incq %rax
        start:
          incq %rax
        jump_target:
          incq %rax
          incq %rax
        post_commit:
          incq %rax
      )" + (jump_location == post_commit ? bad_instruction_line : ""s) +
                  R"(
          incq %rax
      )"s + abort_signature_x86 +
                  R"(
        abort:
          incq %rax
      )" + (jump_location == post_abort ? bad_instruction_line : ""s) +
                  R"(
          incq %rax
      )";
      SCOPED_TRACE("code: " + code);
      auto elf = assemble_function_into_elf_shared_object(
        machine_architecture::x86, code, "test_function");
      auto analysis = analyze_rseq_critical_section(
        elf, "test_function", "start", "post_commit", "abort");
      EXPECT_THAT(
        analysis.all_problems(),
        ElementsAre(VariantWith<rseq_problem::jump_into_critical_section>(
          AllOf(FIELD_EQ(rseq_problem::jump_into_critical_section,
                         jump_instruction_address,
                         elf.symbol("bad_instruction")),
                FIELD_EQ(rseq_problem::jump_into_critical_section,
                         target_instruction_address,
                         elf.symbol("jump_target"))))));
    }
  }
}

TEST(test_rseq_analyzer_x86, jumping_into_start_of_critical_section_is_allowed)
{
  enum jump_location
  {
    before_start,
    exactly_post_commit,
    post_commit,
    post_abort,
  };

  for (const auto* jump_instruction : { "jmp start", "jz start" }) {
    auto jump_instruction_line = jump_instruction + "\n"s;
    for (auto jump_location :
         { before_start, exactly_post_commit, post_commit, post_abort }) {
      auto code =
        R"(
          incq %rax
      )" +
        (jump_location == before_start ? jump_instruction_line : ""s) + R"(
          incq %rax
        start:
          incq %rax
          incq %rax
          incq %rax
      )" +
        (jump_location == exactly_post_commit ? jump_instruction_line : ""s) +
        R"(
        post_commit:
          incq %rax
      )" +
        (jump_location == post_commit ? jump_instruction_line : ""s) + R"(
          incq %rax
      )"s +
        abort_signature_x86 + R"(
        abort:
          incq %rax
      )" +
        (jump_location == post_abort ? jump_instruction_line : ""s) + R"(
          incq %rax
      )";
      SCOPED_TRACE("code: " + code);
      auto elf = assemble_function_into_elf_shared_object(
        machine_architecture::x86, code, "test_function");
      auto analysis = analyze_rseq_critical_section(
        elf, "test_function", "start", "post_commit", "abort");
      EXPECT_THAT(analysis.all_problems(), IsEmpty());
    }
  }
}

TEST(test_rseq_analyzer_x86, jumping_around_critical_section_is_allowed)
{
  enum location
  {
    before_start,
    exactly_post_commit,
    post_commit,
    post_abort,
  };

  for (const auto* jump_instruction :
       { "jmp jump_target", "jz jump_target", "call jump_target" }) {
    auto jump_instruction_line = jump_instruction + "\n"s;
    auto jump_target_label = "jump_target:\n";
    for (auto jump_location : { before_start, post_commit, post_abort }) {
      for (auto target_location :
           { before_start, exactly_post_commit, post_commit, post_abort }) {
        auto code =
          R"(
            incq %rax
        )" +
          (jump_location == before_start ? jump_instruction_line : ""s) + R"(
        )" +
          (target_location == before_start ? jump_target_label : ""s) + R"(
            incq %rax
          start:
            incq %rax
            incq %rax
            incq %rax
        )" +
          (target_location == exactly_post_commit ? jump_target_label : ""s) +
          R"(
          post_commit:
            incq %rax
        )" +
          (jump_location == post_commit ? jump_instruction_line : ""s) + R"(
        )" +
          (target_location == post_commit ? jump_target_label : ""s) + R"(
            incq %rax
        )"s +
          abort_signature_x86 + R"(
          abort:
            incq %rax
        )" +
          (jump_location == post_abort ? jump_instruction_line : ""s) + R"(
        )" +
          (target_location == post_abort ? jump_target_label : ""s) + R"(
            incq %rax
        )";
        SCOPED_TRACE("code: " + code);
        auto elf = assemble_function_into_elf_shared_object(
          machine_architecture::x86, code, "test_function");
        auto analysis = analyze_rseq_critical_section(
          elf, "test_function", "start", "post_commit", "abort");
        EXPECT_THAT(analysis.all_problems(), IsEmpty());
      }
    }
  }
}

TEST(test_rseq_analyzer_x86, jumping_out_of_critical_section_is_allowed)
{
  enum location
  {
    before_start,
    exactly_post_commit,
    post_commit,
    post_abort,
  };

  for (const auto* jump_instruction : { "jmp jump_target", "jz jump_target" }) {
    auto jump_target_label = "jump_target:\n";
    for (auto target_location :
         { before_start, exactly_post_commit, post_commit, post_abort }) {
      auto code = R"(
          incq %rax
      )" + (target_location == before_start ? jump_target_label : ""s) +
                  R"(
          incq %rax
        start:
          incq %rax
      )" + jump_instruction +
                  R"(
          incq %rax
          incq %rax
      )" + (target_location == exactly_post_commit ? jump_target_label : ""s) +
                  R"(
        post_commit:
          incq %rax
      )" + (target_location == post_commit ? jump_target_label : ""s) +
                  R"(
          incq %rax
      )"s + abort_signature_x86 +
                  R"(
        abort:
          incq %rax
      )" + (target_location == post_abort ? jump_target_label : ""s) +
                  R"(
          incq %rax
      )";
      SCOPED_TRACE("code: " + code);
      auto elf = assemble_function_into_elf_shared_object(
        machine_architecture::x86, code, "test_function");
      auto analysis = analyze_rseq_critical_section(
        elf, "test_function", "start", "post_commit", "abort");
      EXPECT_THAT(analysis.all_problems(), IsEmpty());
    }
  }
}

TEST(test_rseq_analyzer_x86, looping_to_start_of_critical_section_is_allowed)
{
  for (const auto* jump_instruction : { "jmp start", "jz start" }) {
    auto code = R"(
        incq %rax
      start:
        incq %rax
    )"s + jump_instruction +
                R"(
        incq %rax
      post_commit:
        incq %rax
    )"s + abort_signature_x86 +
                R"(
      abort:
        incq %rax
    )";
    SCOPED_TRACE("code: " + code);
    auto elf = assemble_function_into_elf_shared_object(
      machine_architecture::x86, code, "test_function");
    auto analysis = analyze_rseq_critical_section(
      elf, "test_function", "start", "post_commit", "abort");
    EXPECT_THAT(analysis.all_problems(), IsEmpty());
  }
}

TEST(test_rseq_analyzer_x86, abort_label_must_be_signed)
{
  auto code = R"(
    start:
      incq %rax
    post_commit:
      incq %rax
      ret

    abort_signature:
      .byte 0x12, 0x34, 0x56, 0x78
    abort:
      incq %rax
  )";
  auto elf = assemble_function_into_elf_shared_object(
    machine_architecture::x86, code, "broken_function");
  auto analysis = analyze_rseq_critical_section(
    elf, "broken_function", "start", "post_commit", "abort");
  EXPECT_THAT(analysis.all_problems(),
              ElementsAre(VariantWith<rseq_problem::invalid_abort_signature>(
                AllOf(PROPERTY_EQ(rseq_problem::invalid_abort_signature,
                                  signature_address,
                                  elf.symbol("abort_signature")),
                      PROPERTY_EQ(rseq_problem::invalid_abort_signature,
                                  abort_address,
                                  elf.symbol("abort")),
                      FIELD_EQ(rseq_problem::invalid_abort_signature,
                               expected_signature,
                               { 0x53_b, 0x30_b, 0x05_b, 0x53_b }),
                      FIELD_EQ(rseq_problem::invalid_abort_signature,
                               actual_signature,
                               { 0x12_b, 0x34_b, 0x56_b, 0x78_b }),
                      PROPERTY_EQ(rseq_problem::invalid_abort_signature,
                                  abort_function_name,
                                  "broken_function")))));
}

TEST(test_rseq_analyzer_x86, out_of_bounds_abort_signature_is_disallowed)
{
  {
    auto code = R"(
        /* No signature: .long 0x53535353 */
      abort:
        incq %rax

      start:
        incq %rax
      post_commit:
        incq %rax
        ret
    )";
    auto elf = assemble_function_into_elf_shared_object(
      machine_architecture::x86, code, "misassembled_function");
    auto analysis = analyze_rseq_critical_section(
      elf, "misassembled_function", "start", "post_commit", "abort");
    EXPECT_THAT(
      analysis.all_problems(),
      ElementsAre(VariantWith<rseq_problem::invalid_abort_signature>(AllOf(
        PROPERTY_EQ(rseq_problem::invalid_abort_signature,
                    signature_address,
                    elf.symbol("abort") - 4),
        PROPERTY_EQ(rseq_problem::invalid_abort_signature,
                    abort_address,
                    elf.symbol("abort")),
        FIELD_EQ(rseq_problem::invalid_abort_signature,
                 expected_signature,
                 { 0x53_b, 0x30_b, 0x05_b, 0x53_b }),
        FIELD_EQ(rseq_problem::invalid_abort_signature,
                 actual_signature,
                 { std::nullopt, std::nullopt, std::nullopt, std::nullopt }),
        PROPERTY_EQ(rseq_problem::invalid_abort_signature,
                    abort_function_name,
                    "misassembled_function")))));
  }

  {
    auto code = R"(
      start:
        incq %rax
      post_commit:
        incq %rax
        ret

      abort_signature:
        /* No signature: .long 0x53535353 */
    )";
    auto elf = assemble_function_into_elf_shared_object(
      machine_architecture::x86, code, "test_function");
    auto abort_address = elf.symbol("abort_signature") + 4;
    auto analysis = analyze_rseq_critical_section(
      function_from_symbol(elf.elf(), "test_function"),
      elf.symbol("start"),
      elf.symbol("post_commit"),
      abort_address);
    EXPECT_THAT(
      analysis.all_problems(),
      ElementsAre(VariantWith<rseq_problem::label_outside_function>(AllOf(
        PROPERTY_EQ(
          rseq_problem::label_outside_function, label_address, abort_address),
        PROPERTY_EQ(rseq_problem::label_outside_function,
                    label_function,
                    "test_function"),
        FIELD_EQ(rseq_problem::label_outside_function,
                 label_kind,
                 rseq_problem::label_outside_function::kind::abort)))));
  }
}

TEST(test_rseq_analyzer_x86,
     partially_out_of_bounds_abort_signature_is_disallowed)
{
  {
    auto code = R"(
        /* Partial signature. */
        .byte 0x53, 0x53
      abort:
        incq %rax

      start:
        incq %rax
      post_commit:
        incq %rax
        ret
    )";
    auto elf = assemble_function_into_elf_shared_object(
      machine_architecture::x86, code, "test_function");
    auto analysis = analyze_rseq_critical_section(
      elf, "test_function", "start", "post_commit", "abort");
    EXPECT_THAT(
      analysis.all_problems(),
      ElementsAre(VariantWith<rseq_problem::invalid_abort_signature>(
        AllOf(PROPERTY_EQ(rseq_problem::invalid_abort_signature,
                          signature_address,
                          elf.symbol("abort") - 4),
              PROPERTY_EQ(rseq_problem::invalid_abort_signature,
                          abort_address,
                          elf.symbol("abort")),
              FIELD_EQ(rseq_problem::invalid_abort_signature,
                       expected_signature,
                       { 0x53_b, 0x30_b, 0x05_b, 0x53_b }),
              FIELD_EQ(rseq_problem::invalid_abort_signature,
                       actual_signature,
                       { std::nullopt, std::nullopt, 0x53_b, 0x53_b }),
              PROPERTY_EQ(rseq_problem::invalid_abort_signature,
                          abort_function_name,
                          "test_function")))));
  }

  {
    auto code = R"(
      start:
        incq %rax
      post_commit:
        incq %rax
        ret

      abort_signature:
        /* Partial signature. */
        .byte 0x53, 0x53
    )";
    auto elf = assemble_function_into_elf_shared_object(
      machine_architecture::x86, code, "test_function");
    auto abort_address = elf.symbol("abort_signature") + 4;
    auto analysis = analyze_rseq_critical_section(
      function_from_symbol(elf.elf(), "test_function"),
      elf.symbol("start"),
      elf.symbol("post_commit"),
      abort_address);
    EXPECT_THAT(analysis.all_problems(),
                UnorderedElementsAre(
                  VariantWith<rseq_problem::invalid_abort_signature>(AllOf(
                    PROPERTY_EQ(rseq_problem::invalid_abort_signature,
                                signature_address,
                                elf.symbol("abort_signature")),
                    PROPERTY_EQ(rseq_problem::invalid_abort_signature,
                                abort_address,
                                abort_address),
                    FIELD_EQ(rseq_problem::invalid_abort_signature,
                             expected_signature,
                             { 0x53_b, 0x30_b, 0x05_b, 0x53_b }),
                    FIELD_EQ(rseq_problem::invalid_abort_signature,
                             actual_signature,
                             { 0x53_b, 0x53_b, std::nullopt, std::nullopt }),
                    PROPERTY_EQ(rseq_problem::invalid_abort_signature,
                                abort_function_name,
                                "test_function"))),
                  VariantWith<rseq_problem::label_outside_function>(FIELD_EQ(
                    rseq_problem::label_outside_function,
                    label_kind,
                    rseq_problem::label_outside_function::kind::abort))));
  }
}

TEST(test_check_rseq_formatting, empty_critical_section_problem)
{
  auto string = std::ostringstream{};
  auto critical_section = stub_critical_section();
  critical_section.function = "too_tight";
  critical_section.start_address = 0x0123456789a;
  critical_section.post_commit_address = 0x0123456789a;
  string << rseq_problem::empty_critical_section{
    {
      .critical_section = critical_section,
    },
  };
  EXPECT_EQ(string.str(), "critical section contains no instructions");
  assert_stream_has_default_formatting(string);
}

TEST(test_check_rseq_formatting, empty_function_problem)
{
  auto string = std::ostringstream{};
  auto critical_section = stub_critical_section();
  critical_section.function_address = 0x0123456789a;
  critical_section.function = "null_and_void";
  string << rseq_problem::empty_function{
    {
      .critical_section = critical_section,
    },
  };
  EXPECT_EQ(string.str(), "function is empty");
  assert_stream_has_default_formatting(string);
}

TEST(test_check_rseq_formatting, interrupt_problem)
{
  auto string = std::ostringstream{};
  auto critical_section = stub_critical_section();
  critical_section.function = "annoying_chatter_box";
  string << rseq_problem::interrupt{
    {
      .critical_section = critical_section,
    },
    .interrupt_instruction_address = 0x0123456789a,
    .interrupt_instruction_string = "syscall",
  };
  EXPECT_EQ(string.str(), "interrupting instruction at 0x123456789a: syscall");
  assert_stream_has_default_formatting(string);
}

TEST(test_check_rseq_formatting, incomplete_rseq_descriptor_problem)
{
  auto string = std::ostringstream{};
  string << rseq_problem::incomplete_rseq_descriptor{
    .descriptor_address = 0x0123456789a,
  };
  EXPECT_EQ(
    string.str(),
    "incomplete rseq_cs descriptor at 0x123456789a (expected 32 bytes)");
  assert_stream_has_default_formatting(string);
}

TEST(test_check_rseq_formatting, invalid_abort_signature_problem)
{
  {
    auto string = std::ostringstream{};
    auto critical_section = stub_critical_section();
    critical_section.function = "sloppy_back_dater";
    critical_section.abort_address = 0x0123456789a + 4;
    string << rseq_problem::invalid_abort_signature{
      {
        .critical_section = critical_section,
      },
      .expected_signature = { 0x0f_b, 0x1f_b, 0x2f_b, 0x3f_b },
      .actual_signature = { 0x3f_b, 0x2f_b, 0x1f_b, 0x0f_b },
    };
    EXPECT_EQ(string.str(),
              "invalid abort signature at 0x123456789a: "
              "expected 0f 1f 2f 3f but got 3f 2f 1f 0f");
    assert_stream_has_default_formatting(string);
  }

  {
    auto string = std::ostringstream{};
    auto critical_section = stub_critical_section();
    critical_section.function = "banana";
    critical_section.abort_address = 0x08 + 4;
    string << rseq_problem::invalid_abort_signature{
      {
        .critical_section = critical_section,
      },
      .expected_signature = { 0xff_b, 0xfe_b, 0xfd_b, 0xfc_b },
      .actual_signature = { std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt },
    };
    EXPECT_EQ(string.str(),
              "invalid abort signature at 0x8: expected ff fe fd fc but "
              "got ?? ?? ?? ??");
    assert_stream_has_default_formatting(string);
  }
}

TEST(test_check_rseq_formatting, inverted_critical_section_problem)
{
  auto string = std::ostringstream{};
  auto critical_section = stub_critical_section();
  critical_section.function = "travel_time";
  critical_section.start_address = 0x01000008;
  critical_section.post_commit_address = 0x01000002;
  string << rseq_problem::inverted_critical_section{
    {
      .critical_section = critical_section,
    },
  };
  EXPECT_EQ(string.str(), "post-commit comes before start");
  assert_stream_has_default_formatting(string);
}

TEST(test_check_rseq_formatting, jump_into_critical_section_problem)
{
  auto string = std::ostringstream{};
  auto critical_section = stub_critical_section();
  critical_section.function = "antsy_itch";
  string << rseq_problem::jump_into_critical_section{
    {
      .critical_section = critical_section,
    },
    .jump_instruction_address = 0x10000005,
    .jump_instruction_string = "jmp $1000001f",
    .target_instruction_address = 0x1000001f,
  };
  EXPECT_EQ(string.str(),
            "jump into critical section at 0x10000005: jmp $1000001f");
  assert_stream_has_default_formatting(string);
}

TEST(test_check_rseq_formatting, label_outside_function_problem)
{
  {
    auto string = std::ostringstream{};
    auto critical_section = stub_critical_section();
    critical_section.function = "bad_start";
    critical_section.start_address = 0x10000000;
    critical_section.post_commit_address = 0x1000000f;
    critical_section.abort_address = 0x100000ff;
    string << rseq_problem::label_outside_function{
      {
        .critical_section = critical_section,
      },
      .label_kind = rseq_problem::label_outside_function::kind::start,
    };
    EXPECT_EQ(string.str(), "critical section start is outside function");
    assert_stream_has_default_formatting(string);
  }

  {
    auto string = std::ostringstream{};
    auto critical_section = stub_critical_section();
    critical_section.function = "sad_post_commit";
    critical_section.start_address = 0x1ffffffff;
    critical_section.post_commit_address = 0x20000000;
    critical_section.abort_address = 0x200000ff;
    string << rseq_problem::label_outside_function{
      {
        .critical_section = critical_section,
      },
      .label_kind = rseq_problem::label_outside_function::kind::post_commit,
    };
    EXPECT_EQ(string.str(),
              "critical section post-commit is "
              "outside function");
    assert_stream_has_default_formatting(string);
  }

  {
    auto string = std::ostringstream{};
    auto critical_section = stub_critical_section();
    critical_section.function = "nasty_abort";
    critical_section.start_address = 0x3000000f;
    critical_section.post_commit_address = 0x300000ff;
    critical_section.abort_address = 0x30000000;
    string << rseq_problem::label_outside_function{
      {
        .critical_section = critical_section,
      },
      .label_kind = rseq_problem::label_outside_function::kind::abort,
    };
    EXPECT_EQ(string.str(), "critical section abort is outside function");
    assert_stream_has_default_formatting(string);
  }
}

TEST(test_check_rseq_formatting, no_rseq_descriptors_problem)
{
  auto string = std::ostringstream{};
  string << rseq_problem::no_rseq_descriptors{
    .section_name = ".rseq",
  };
  EXPECT_EQ(string.str(),
            ".rseq section is missing or contains no descriptors");
  assert_stream_has_default_formatting(string);
}

TEST(test_check_rseq_formatting, stack_pointer_modified_problem)
{
  {
    auto string = std::ostringstream{};
    auto critical_section = stub_critical_section();
    critical_section.function = "stack_smasher";
    string << rseq_problem::stack_pointer_modified{
      {
        .critical_section = critical_section,
      },
      .modifying_instruction_address = 0x00007ffffffffea8,
      .modifying_instruction_string = "retq",
      .modifying_instruction_called_function = "",
    };
    EXPECT_EQ(string.str(), "stack pointer modified at 0x7ffffffffea8: retq");
    assert_stream_has_default_formatting(string);
  }

  {
    auto string = std::ostringstream{};
    auto critical_section = stub_critical_section();
    critical_section.function = "sinking_ship";
    string << rseq_problem::stack_pointer_modified{
      {
        .critical_section = critical_section,
      },
      .modifying_instruction_address = 0x0000000800001003,
      .modifying_instruction_string = "callq 0x123456",
      .modifying_instruction_called_function = "_abort",
    };
    EXPECT_EQ(
      string.str(),
      "stack pointer modified at 0x800001003: callq 0x123456 // _abort");
    assert_stream_has_default_formatting(string);
  }
}

TEST(test_check_rseq, empty_critical_section_has_zero_bytes)
{
  auto cs = stub_critical_section();
  cs.start_address = 0x00050000;
  cs.post_commit_address = 0x00050000;
  EXPECT_EQ(cs.size_in_bytes(), 0);
}

TEST(test_check_rseq, normal_critical_section_has_bytes)
{
  auto cs = stub_critical_section();
  cs.start_address = 0x00050000;
  cs.post_commit_address = 0x00050031;
  EXPECT_EQ(cs.size_in_bytes(), 0x31);
}

TEST(test_check_rseq, backward_critical_section_has_no_bytes)
{
  auto cs = stub_critical_section();
  cs.start_address = 0x00050000;
  cs.post_commit_address = 0x0004fff8;
  EXPECT_EQ(cs.size_in_bytes(), std::nullopt);
}

TEST(test_check_rseq, huge_critical_section_has_no_bytes)
{
  // A 10 KiB critical section is probably a mistake.
  auto huge_size = 10 * 1024;
  auto cs = stub_critical_section();
  cs.start_address = 0x00050000;
  cs.post_commit_address = 0x00050000 + huge_size;
  EXPECT_EQ(cs.size_in_bytes(), std::nullopt);
}

TEST(test_check_rseq_file, library_with_no_rseq_descriptors_has_problems)
{
  auto assembly_code = R"(
    // Missing: .section .data_cxxtrace_rseq, "a", @progbits

    .text
    .align 16
    .globl _my_rseq_function
    .type _my_rseq_function, @function
    _my_rseq_function:
      nop
    .start:
      nop
    .post_commit:
      ret
    .abort:
      ret
    .size _my_rseq_function, . - _my_rseq_function
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86, assembly_code, {});

  auto analysis = cxxtrace_check_rseq::analyze_rseq_critical_sections_in_file(
    elf_so.elf_path());
  EXPECT_THAT(analysis.all_problems(),
              ElementsAre(VariantWith<rseq_problem::no_rseq_descriptors>(
                FIELD_EQ(rseq_problem::no_rseq_descriptors,
                         section_name,
                         ".data_cxxtrace_rseq", ))));
}

TEST(test_check_rseq_file, good_libraries_have_no_problems)
{
  auto assembly_code = R"(
    .section .data_cxxtrace_rseq, "a", @progbits
    .align 32
    .type _my_rseq_descriptor, @object
    _my_rseq_descriptor:
      .long 0x11223344 // Address: 0x0000000100002000
      .long 0x55667788
      .quad .start
      .quad .post_commit - .start
      .quad .abort
    .size _my_rseq_descriptor, . - _my_rseq_descriptor

    .text
    .align 16
    .globl _my_rseq_function
    .type _my_rseq_function, @function
    _my_rseq_function:
      nop // Address: +0x0
    .start:
      nop // Address: +0x1
    .post_commit:
      ret // Address: +0x2

      // rseq abort signature.
      .byte 0x53, 0x30, 0x05, 0x53 // Address: +0x3
    .abort:
      ret // Address: +0x7
    .size _my_rseq_function, . - _my_rseq_function
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86, assembly_code, {});

  auto analysis = cxxtrace_check_rseq::analyze_rseq_critical_sections_in_file(
    elf_so.elf_path());
  EXPECT_THAT(analysis.all_problems(), IsEmpty());
}

TEST(test_check_rseq_file, single_function_with_multiple_problems)
{
  auto assembly_code = R"(
    .section .data_cxxtrace_rseq, "a", @progbits
    .align 32
    .type _my_rseq_descriptor, @object
    _my_rseq_descriptor:
      .long 0
      .long 0
      .quad .start
      .quad .post_commit - .start
      .quad .abort
    .size _my_rseq_descriptor, . - _my_rseq_descriptor

    .section .text
    .align 16
    .globl _my_rseq_function
    .type _my_rseq_function, @function
    _my_rseq_function:
      push %rbp               // Address: +0x0
    .start:
      push %rax               // Address: +0x1 (problem)
      nop                     // Address: +0x2
    .inside_critical_section:
      pop %rax                // Address: +0x3 (problem)
    .post_commit:
      pop %rbp                // Address: +0x4
      ret                     // Address: +0x5

      // rseq abort signature.
      .byte 0x53, 0x53, 0x53, 0xff // Address: +0x6 (problem)
    .abort:
      jmp .inside_critical_section // Address: +0xa (problem)
      ret                          // Address: +0xc
    .size _my_rseq_function, . - _my_rseq_function
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86, assembly_code, {});

  auto analysis = cxxtrace_check_rseq::analyze_rseq_critical_sections_in_file(
    elf_so.elf_path());
  EXPECT_THAT(
    analysis.all_problems(),
    UnorderedElementsAre(
      // _my_rseq_function: stack pointer modified: pushq %rax
      VariantWith<rseq_problem::stack_pointer_modified>(testing::_),
      // _my_rseq_function: stack pointer modified: popq %rax
      VariantWith<rseq_problem::stack_pointer_modified>(testing::_),
      // _my_rseq_function: invalid abort signature: expected 53 53 53 53 but
      // got 53 53 53 ff
      VariantWith<rseq_problem::invalid_abort_signature>(testing::_)));
}

TEST(test_check_rseq_file, multiple_functions_with_problems)
{
  auto assembly_code = R"(
    .section .data_cxxtrace_rseq, "a", @progbits
    .align 32
    .type _my_rseq_descriptor_1, @object
    _my_rseq_descriptor_1:
      .long 0
      .long 0
      .quad .start_1
      .quad .post_commit_1 - .start_1
      .quad .abort_1
    .size _my_rseq_descriptor_1, . - _my_rseq_descriptor_1

    .align 32
    .type _my_rseq_descriptor_2, @object
    _my_rseq_descriptor_2:
      .long 0
      .long 0
      .quad .start_2
      .quad .post_commit_2 - .start_2
      .quad .abort_2
    .size _my_rseq_descriptor_2, . - _my_rseq_descriptor_2

    .align 32
    .type _my_rseq_descriptor_3, @object
    _my_rseq_descriptor_3:
      .long 0
      .long 0
      /* Truncated; missing start_ip, post_commit_offset, and abort_ip. */
    .size _my_rseq_descriptor_3, . - _my_rseq_descriptor_3

    .section .text
    .align 16
    .globl _my_rseq_function_1
    .type _my_rseq_function_1, @function
    _my_rseq_function_1:
    .start_1: // (problem)
    .post_commit_1:
      ret
  )"s + abort_signature_x86 +
                       R"(
    .abort_1:
      ret
    .size _my_rseq_function_1, . - _my_rseq_function_1

    .align 16
    .globl _my_rseq_function_2
    .type _my_rseq_function_2, @function
    _my_rseq_function_2:
    .start_2:
      syscall // (problem)
    .post_commit_2:
      ret
  )"s + abort_signature_x86 +
                       R"(
    .abort_2:
      ret
    .size _my_rseq_function_2, . - _my_rseq_function_2
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86, assembly_code.c_str(), {});

  auto analysis = cxxtrace_check_rseq::analyze_rseq_critical_sections_in_file(
    elf_so.elf_path());
  EXPECT_THAT(
    analysis.all_problems(),
    UnorderedElementsAre(
      // _my_rseq_function_1: critical section contains no instructions
      VariantWith<rseq_problem::empty_critical_section>(
        FIELD(rseq_problem::empty_critical_section,
              critical_section,
              FIELD_EQ(cxxtrace_check_rseq::rseq_critical_section,
                       descriptor_address,
                       elf_so.symbol("_my_rseq_descriptor_1")))),
      // _my_rseq_function_2: interrupting instruction: syscall
      VariantWith<rseq_problem::interrupt>(
        FIELD(rseq_problem::interrupt,
              critical_section,
              FIELD_EQ(cxxtrace_check_rseq::rseq_critical_section,
                       descriptor_address,
                       elf_so.symbol("_my_rseq_descriptor_2")))),
      // incomplete rseq_cs descriptor (expected 32 bytes)
      VariantWith<rseq_problem::incomplete_rseq_descriptor>(
        FIELD_EQ(rseq_problem::incomplete_rseq_descriptor,
                 descriptor_address,
                 elf_so.symbol("_my_rseq_descriptor_3")))));
}

TEST(test_check_rseq_file,
     rseq_descriptors_with_unallocated_ip_pointers_are_disallowed)
{
  // start, post_commit, and abort addresses are unallocated:
  {
    auto assembly_code = R"(
      .section .data_cxxtrace_rseq, "a", @progbits
      .align 32
      .type _my_rseq_descriptor, @object
      _my_rseq_descriptor:
        .long 0
        .long 0
        .quad my_rseq_function_start
        .quad my_rseq_function_post_commit - my_rseq_function_start
        .quad my_rseq_function_abort
      .size _my_rseq_descriptor, . - _my_rseq_descriptor

      .section .text_cxxtrace_test, "ax", @progbits
        .byte 0x00 // Make our dummy section exist in the .so.
      // HACK(strager): Hopefully nothing collides with these symbols.
      .set my_rseq_function_start, . + 0       // Address: 0x00007f0000000001
      .set my_rseq_function_post_commit, . + 2 // Address: 0x00007f0000000003
      .set my_rseq_function_abort, . + 8       // Address: 0x00007f0000000009
    )";
    auto elf_so = assemble_into_elf_shared_object(
      machine_architecture::x86,
      assembly_code,
      {
        "-Wl,--section-start=.data_cxxtrace_rseq=0000000008000000",
        "-Wl,--section-start=.text_cxxtrace_test=00007f0000000000",
      });

    auto analysis = cxxtrace_check_rseq::analyze_rseq_critical_sections_in_file(
      elf_so.elf_path());
    EXPECT_THAT(
      analysis.all_problems(),
      UnorderedElementsAre(
        VariantWith<rseq_problem::label_outside_function>(AllOf(
          FIELD(rseq_problem::label_outside_function,
                critical_section,
                FIELD_EQ(cxxtrace_check_rseq::rseq_critical_section,
                         descriptor_address,
                         0x0000000008000000)),
          PROPERTY_EQ(rseq_problem::label_outside_function,
                      label_address,
                      elf_so.symbol("my_rseq_function_start")),
          PROPERTY_EQ(rseq_problem::label_outside_function,
                      label_address,
                      elf_so.symbol("my_rseq_function_start")),
          PROPERTY_EQ(rseq_problem::label_outside_function, label_function, ""),
          FIELD_EQ(rseq_problem::label_outside_function,
                   label_kind,
                   rseq_problem::label_outside_function::kind::start)))));
    // TODO(strager): Have the analysis infer the function name from the rseq
    // descriptor's DWARF information.
  }

  // abort address is unallocated:
  {
    auto assembly_code = R"(
      .section .data_cxxtrace_rseq, "a", @progbits
      .align 32
      .type _my_rseq_descriptor, @object
      _my_rseq_descriptor:
        .long 0
        .long 0
        .quad .start
        .quad .post_commit - .start
        .quad my_bogus_abort
      .size _my_rseq_descriptor, . - _my_rseq_descriptor

      .section .text_cxxtrace_test, "ax", @progbits
      .globl _my_rseq_function
      .type _my_rseq_function, @function
      _my_rseq_function:
      .start:
        nop       // Address: 0x00007f0000000000
      .post_commit:
        ret       // Address: 0x00007f0000000001
      .size _my_rseq_function, . - _my_rseq_function

      .set my_bogus_abort, . + 8 // Address: 0x00007f000000000a
    )";
    auto elf_so = assemble_into_elf_shared_object(
      machine_architecture::x86,
      assembly_code,
      {
        "-Wl,--section-start=.text_cxxtrace_test=00007f0000000000",
      });

    auto analysis = cxxtrace_check_rseq::analyze_rseq_critical_sections_in_file(
      elf_so.elf_path());
    EXPECT_THAT(
      analysis.all_problems(),
      UnorderedElementsAre(VariantWith<rseq_problem::label_outside_function>(
        AllOf(PROPERTY_EQ(rseq_problem::label_outside_function,
                          label_address,
                          elf_so.symbol("my_bogus_abort")),
              PROPERTY_EQ(rseq_problem::label_outside_function,
                          label_function,
                          "_my_rseq_function"),
              FIELD_EQ(rseq_problem::label_outside_function,
                       label_kind,
                       rseq_problem::label_outside_function::kind::abort)))));
  }
}

TEST(test_check_rseq_file, incomplete_rseq_descriptors_are_disallowed)
{
  for (auto fields : { 0, 1, 2, 3, 4 }) {
    auto assembly_code = R"(
      .section .data_cxxtrace_rseq, "a", @progbits
      .align 32
      .type _my_rseq_descriptor, @object
      _my_rseq_descriptor:
    )"s + (fields >= 1 ? ".long 0" : ".byte 0 // Truncated version.") +
                         R"(
    )"s + (fields >= 2 ? ".long 0" : ".byte 0 // Truncated flags.") +
                         R"(
    )"s + (fields >= 3 ? ".quad .start" : ".byte 0 // Truncated start.") +
                         R"(
    )"s +
                         (fields >= 4 ? ".quad .post_commit - .start"
                                      : ".byte 0 // Truncated post_commit.") +
                         R"(
        .byte 0 // Truncated abort.
      .size _my_rseq_descriptor, . - _my_rseq_descriptor

      .section .text
      .align 16
      .globl _my_rseq_function
      .type _my_rseq_function, @function
      _my_rseq_function:
        nop
      .start:
        nop
      .post_commit:
        ret
    )" + abort_signature_x86 +
                         R"(
      .abort:
        ret
      .size _my_rseq_function, . - _my_rseq_function
    )";
    auto elf_so = assemble_into_elf_shared_object(
      machine_architecture::x86, assembly_code.c_str(), {});

    auto analysis = cxxtrace_check_rseq::analyze_rseq_critical_sections_in_file(
      elf_so.elf_path());
    EXPECT_THAT(analysis.all_problems(),
                UnorderedElementsAre(
                  VariantWith<rseq_problem::incomplete_rseq_descriptor>(
                    FIELD_EQ(rseq_problem::incomplete_rseq_descriptor,
                             descriptor_address,
                             elf_so.symbol("_my_rseq_descriptor")))));
  }
}

TEST(test_rseq_analysis, file_problems_excludes_critical_section_problems)
{
  auto analysis = cxxtrace_check_rseq::rseq_analysis{};
  analysis.add_problem(rseq_problem::no_rseq_descriptors{});
  analysis.add_problem(rseq_problem::label_outside_function{});
  EXPECT_THAT(analysis.file_problems(),
              UnorderedElementsAre(
                VariantWith<rseq_problem::no_rseq_descriptors>(testing::_)));
}

TEST(test_rseq_analysis, critical_section_problems_exclude_file_problems)
{
  auto analysis = cxxtrace_check_rseq::rseq_analysis{};
  analysis.add_problem(rseq_problem::no_rseq_descriptors{});
  analysis.add_problem(rseq_problem::label_outside_function{});
  EXPECT_THAT(analysis.problems_by_critical_section(),
              ElementsAre(FIELD(
                cxxtrace_check_rseq::rseq_analysis::critical_section_problems,
                problems,
                ElementsAre(VariantWith<rseq_problem::label_outside_function>(
                  testing::_)))));
}

TEST(test_rseq_analysis, problems_by_critical_section_are_grouped_by_descriptor)
{
  auto critical_section_a = stub_critical_section();
  critical_section_a.descriptor_address = 0x00010000;
  auto critical_section_b = stub_critical_section();
  critical_section_b.descriptor_address = 0x00010020;
  ASSERT_NE(critical_section_a, critical_section_b);

  auto analysis = cxxtrace_check_rseq::rseq_analysis{};
  analysis.add_problem(rseq_problem::label_outside_function{
    { .critical_section = critical_section_a },
    .label_kind = rseq_problem::label_outside_function::kind::start,
  });
  analysis.add_problem(rseq_problem::label_outside_function{
    { .critical_section = critical_section_b },
    .label_kind = rseq_problem::label_outside_function::kind::start,
  });
  analysis.add_problem(rseq_problem::label_outside_function{
    { .critical_section = critical_section_a },
    .label_kind = rseq_problem::label_outside_function::kind::start,
  });
  analysis.add_problem(rseq_problem::jump_into_critical_section{
    { .critical_section = critical_section_a },
    .jump_instruction_address = 0,
    .jump_instruction_string = "",
    .target_instruction_address = 0,
  });
  analysis.add_problem(rseq_problem::invalid_abort_signature{
    { .critical_section = critical_section_b },
    .expected_signature = { 0_b, 0_b, 0_b, 0_b },
    .actual_signature = { std::nullopt,
                          std::nullopt,
                          std::nullopt,
                          std::nullopt },
  });
  EXPECT_THAT(
    analysis.problems_by_critical_section(),
    ElementsAre(
      AllOf(
        FIELD_EQ(cxxtrace_check_rseq::rseq_analysis::critical_section_problems,
                 critical_section,
                 critical_section_a),
        FIELD(cxxtrace_check_rseq::rseq_analysis::critical_section_problems,
              problems,
              ElementsAre(
                VariantWith<rseq_problem::label_outside_function>(testing::_),
                VariantWith<rseq_problem::label_outside_function>(testing::_),
                VariantWith<rseq_problem::jump_into_critical_section>(
                  testing::_)))),
      AllOf(
        FIELD_EQ(cxxtrace_check_rseq::rseq_analysis::critical_section_problems,
                 critical_section,
                 critical_section_b),
        FIELD(
          cxxtrace_check_rseq::rseq_analysis::critical_section_problems,
          problems,
          ElementsAre(
            VariantWith<rseq_problem::label_outside_function>(testing::_),
            VariantWith<rseq_problem::invalid_abort_signature>(testing::_))))));
}

namespace {
auto
stub_critical_section() -> cxxtrace_check_rseq::rseq_critical_section
{
  return cxxtrace_check_rseq::rseq_critical_section{
    .descriptor_address = 0xf0000000,
    .function_address = 0x08000000,
    .function = "stub_function",
    .start_address = 0x08000010,
    .post_commit_address = 0x08000020,
    .abort_address = 0x08000030,
  };
}

auto
assert_stream_has_default_formatting(std::ostream& stream) -> void
{
  EXPECT_EQ(stream.fill(), stream.widen(' '));
  EXPECT_EQ(stream.flags(), std::ios_base::dec | std::ios_base::skipws);
  EXPECT_EQ(stream.precision(), 6);
  EXPECT_EQ(stream.width(), 0);

  EXPECT_EQ(stream.flags() & std::ios_base::adjustfield, 0);
  EXPECT_EQ(stream.flags() & std::ios_base::basefield, std::ios_base::dec);
  EXPECT_FALSE(stream.flags() & std::ios_base::boolalpha);
  EXPECT_EQ(stream.flags() & std::ios_base::floatfield, 0);
  EXPECT_FALSE(stream.flags() & std::ios_base::showbase);
  EXPECT_FALSE(stream.flags() & std::ios_base::showpoint);
  EXPECT_FALSE(stream.flags() & std::ios_base::showpos);
  EXPECT_TRUE(stream.flags() & std::ios_base::skipws);
  EXPECT_FALSE(stream.flags() & std::ios_base::unitbuf);
  EXPECT_FALSE(stream.flags() & std::ios_base::uppercase);
}
}
}
