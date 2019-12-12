#include "assembler.h"
#include "elf_function.h"
#include "is_bytes.h"
#include "machine_code.h"
#include <cxxtrace/string.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

using cxxtrace_check_rseq::machine_architecture;
using testing::ElementsAreArray;
using testing::IsEmpty;

namespace cxxtrace_test {
namespace {
auto
assemble_function(cxxtrace::czstring name,
                  machine_architecture architecture,
                  cxxtrace::czstring assembly_code)
  -> cxxtrace_check_rseq::elf_function
{
  auto elf =
    assemble_function_into_elf_shared_object(architecture, assembly_code, name);
  return function_from_symbol(elf.elf(), name);
}
}

TEST(test_assemble_x86, assemble_nothing)
{
  auto function =
    assemble_function("test_function", machine_architecture::x86, "");
  EXPECT_THAT(function.instruction_bytes, IsEmpty());
}

TEST(test_assemble_x86, single_simple_instruction)
{
  auto function =
    assemble_function("test_function", machine_architecture::x86, "ret");
  EXPECT_THAT(function.instruction_bytes, is_bytes(0xc3));

  function = assemble_function(
    "test_function", machine_architecture::x86, "xor %eax, %eax");
  EXPECT_THAT(function.instruction_bytes, is_bytes(0x31, 0xc0));

  function = assemble_function(
    "test_function", machine_architecture::x86, "xor %rax, %rax");
  EXPECT_THAT(function.instruction_bytes, is_bytes(0x48, 0x31, 0xc0));
}

TEST(test_assemble_x86, multiple_simple_instructions)
{
  auto function = assemble_function(
    "test_function", machine_architecture::x86, "xor %eax, %eax\nret");
  EXPECT_THAT(function.instruction_bytes, is_bytes(0x31, 0xc0, 0xc3));
}

TEST(test_assemble_x86, query_symbols)
{
  auto code = R"(
    /* +0 */first:  push %rax      /* 1 byte */
    /* +1 */second: add $3, %rax   /* 4 bytes */
    /* +5 */third:  xor %eax, %eax /* 2 bytes */
    /* +7 */fourth:
  )";
  auto elf = assemble_function_into_elf_shared_object(
    machine_architecture::x86, code, "test_function");
  auto function = function_from_symbol(elf.elf(), "test_function");
  auto base = function.base_address;
  EXPECT_EQ(elf.symbol("first") - base, 0);
  EXPECT_EQ(elf.symbol("second") - base, 1);
  EXPECT_EQ(elf.symbol("third") - base, 5);
  EXPECT_EQ(elf.symbol("fourth") - base, 7);
}

TEST(test_assemble_x86, jump_to_label)
{
  auto function =
    assemble_function("test_function", machine_architecture::x86, R"(
    /* +0 */      jmp end  /* 2 bytes */
    /* +2 */loop: dec %eax /* 2 bytes */
    /* +4 */      jnz loop /* 2 bytes */
    /* +6 */end:
  )");
  EXPECT_THAT(function.instruction_bytes,
              is_bytes(
                // jmp end
                0xeb,
                0x04,
                // dec %eax
                0xff,
                0xc8,
                // jnz loop
                0x75,
                0xfc));
}

TEST(test_assemble_x86, assembling_undefined_external_label_succeeds)
{
  EXPECT_NO_THROW({
    assemble_function("test_function", machine_architecture::x86, R"(
          jmp symbol_does_not_exist@PLT
        )");
  });
}
}
