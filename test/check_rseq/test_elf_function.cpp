#include "../gtest_scoped_trace.h"
#include "assembler.h"
#include "elf_function.h"
#include "is_bytes.h"
#include "machine_code.h"
#include <cstdint>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

using cxxtrace_check_rseq::function_containing_address;
using cxxtrace_check_rseq::machine_address;
using cxxtrace_check_rseq::machine_architecture;
using testing::AnyOf;
using testing::StrEq;

namespace cxxtrace_test {
TEST(test_elf_analyzer, function_containing_address)
{
  auto assembly_code = R"(
    .section .text_cxxtrace_test, "ax", @progbits
    .globl _my_function
    .type _my_function, @function
    _my_function:
      nop // Address: 0x0000007f68000000
      nop // Address: 0x0000007f68000001
      ret // Address: 0x0000007f68000002
      .byte 0x53, 0x53, 0x53, 0x53 // Address: 0x0000007f68000003
      ret // Address: 0x0000007f68000007
    .size _my_function, . - _my_function
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86,
    assembly_code,
    {
      "-Wl,--section-start=.text_cxxtrace_test=0000007f68000000",
    });

  auto my_function_address = machine_address{ 0x0000007f68000000 };
  auto my_function_size = std::uint64_t{ 8 };
  for (auto address = my_function_address;
       address < my_function_address + my_function_size;
       ++address) {
    CXXTRACE_SCOPED_TRACE()
      << "address: " << std::hex << std::showbase << address;
    auto function = function_containing_address(elf_so.elf(), address);
    EXPECT_EQ(function.name, "_my_function");
    EXPECT_EQ(function.base_address, my_function_address);
    EXPECT_EQ(function.instruction_bytes.size(), my_function_size);
    EXPECT_THAT(function.instruction_bytes,
                is_bytes(0x90, // nop
                         0x90, // nop
                         0xc3, // ret
                         // rseq abort signature.
                         0x53,
                         0x53,
                         0x53,
                         0x53,
                         0xc3 // ret
                         ));
  }
}

TEST(test_elf_analyzer, address_with_no_function)
{
  auto assembly_code = R"(
    .section .data_cxxtrace_rseq, "a", @progbits
    .align 32
    .type _my_rseq_descriptor, @object
    _my_rseq_descriptor:
      .long 1 // Address: 0x0010000100002000
      .long 2
      .quad 3
      .quad 4
      .quad 5
    .size _my_rseq_descriptor, . - _my_rseq_descriptor

    .section .text_cxxtrace_test, "ax", @progbits
    .globl _my_function
    .type _my_function, @function
    _my_function:
      nop // Address: 0x0000007f68000000
      nop // Address: 0x0000007f68000001
      ret // Address: 0x0000007f68000002
      .byte 0x53, 0x53, 0x53, 0x53 // Address: 0x0000007f68000003
      ret // Address: 0x0000007f68000007
    .size _my_function, . - _my_function
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86,
    assembly_code,
    {
      "-Wl,--section-start=.data_cxxtrace_rseq=0010000100002000",
      "-Wl,--section-start=.text_cxxtrace_test=0000007f68000000",
    });

  auto assert_no_function_contains_address =
    [&](machine_address address) -> void {
    EXPECT_THROW({ function_containing_address(elf_so.elf(), address); },
                 cxxtrace_check_rseq::no_function_containing_address)
      << "address: " << std::hex << std::showbase << address;
  };

  auto my_function_address = machine_address{ 0x0000007f68000000 };
  auto my_function_size = std::uint64_t{ 8 };
  for (auto address : {
         my_function_address - 1,
         my_function_address + my_function_size,
       }) {
    assert_no_function_contains_address(address);
  }

  auto my_rseq_descriptor_address = machine_address{ 0x0010000100002000 };
  auto my_rseq_descriptor_size = std::uint64_t{ 32 };
  for (auto address = my_rseq_descriptor_address;
       address < my_rseq_descriptor_address + my_rseq_descriptor_size;
       ++address) {
    assert_no_function_contains_address(address);
  }
}

TEST(test_elf_analyzer, function_containing_address_with_missing_progbits)
{
  {
    auto assembly_code = R"(
      .section .text_cxxtrace_test, "ax", @progbits
      .globl _my_oversized_function
      .type _my_oversized_function, @function
      _my_oversized_function:
        ret // Address: 0x0000555555554000
        // The next 255 bytes are [probably] unallocated. (Hopefully,
        // 0x0000555555554001 isn't allocated by some other section.) Therefore,
        // the _my_oversized_function function spans some allocated bytes and some
        // unallocated bytes.
      .size _my_oversized_function, 256
    )";
    auto elf_so = assemble_into_elf_shared_object(
      machine_architecture::x86,
      assembly_code,
      {
        "-Wl,--section-start=.text_cxxtrace_test=0000555555554000",
      });
    auto my_oversized_function_address = machine_address{ 0x0000555555554000 };
    auto my_oversized_function_reported_size = std::uint64_t{ 256 };
    auto my_oversized_function_actual_size = std::uint64_t{ 1 };
    for (auto address =
           my_oversized_function_address + my_oversized_function_actual_size;
         address <
         my_oversized_function_address + my_oversized_function_reported_size;
         ++address) {
      EXPECT_THROW({ function_containing_address(elf_so.elf(), address); },
                   cxxtrace_check_rseq::function_contains_unallocated_bytes)
        << "address: " << std::hex << std::showbase << address
        << " (_my_oversized_function)";
    }
  }

  {
    auto assembly_code = R"(
      .section .text_cxxtrace_test, "ax", @progbits
      .globl _my_unallocated_function
      .type _my_unallocated_function, @function
        ret // Address: 0x0000555555558000
        // The next 15 bytes are [probably] unallocated. (Hopefully,
        // 0x0000555555558001 isn't allocated by some other section.) Therefore:
        // * .text_cxxtrace_test's size is 1.
        // * .text_cxxtrace_test starts at address 0x0000555555558010.
        // * _my_unallocated_function's value is 0x0000555555558010.
        // * _my_unallocated_function's section is .text_cxxtrace_test.
      .set _my_unallocated_function, . + 15
      .size _my_unallocated_function, 4
    )";
    auto elf_so = assemble_into_elf_shared_object(
      machine_architecture::x86,
      assembly_code,
      {
        "-Wl,--section-start=.text_cxxtrace_test=0000555555558000",
      });
    auto my_unallocated_function_address =
      machine_address{ 0x0000555555558010 };
    auto my_unallocated_function_reported_size = std::uint64_t{ 4 };
    for (auto address = my_unallocated_function_address;
         address < my_unallocated_function_address +
                     my_unallocated_function_reported_size;
         ++address) {
      EXPECT_THROW({ function_containing_address(elf_so.elf(), address); },
                   cxxtrace_check_rseq::function_contains_unallocated_bytes)
        << "address: " << std::hex << std::showbase << address
        << " (_my_unallocated_function)";
    }
  }

  {
    auto assembly_code = R"(
      .section .text_cxxtrace_test, "ax", @progbits
      .globl _my_headless_function
      .type _my_headless_function, @function
      .set _my_headless_function, . - 16 // 0x000055555555bff0
        ret // Address: 0x000055555555c000
        // The previous 16 bytes are [probably] unallocated. (Hopefully,
        // 0x000055555555bff0 isn't allocated by some other section.) Therefore:
        // * .text_cxxtrace_test's size is 1.
        // * .text_cxxtrace_test starts at address 0x000055555555c000.
        // * _my_headless_function's value is 0x000055555555bff0.
        // * _my_headless_function's section is .text_cxxtrace_test.
        // * _my_headless_function's size is 17.
      .size _my_headless_function, . - _my_headless_function
    )";
    auto elf_so = assemble_into_elf_shared_object(
      machine_architecture::x86,
      assembly_code,
      {
        "-Wl,--section-start=.text_cxxtrace_test=000055555555c000",
      });
    auto my_headless_function_address = machine_address{ 0x000055555555bff0 };
    auto my_headless_function_reported_size = std::uint64_t{ 17 };
    for (auto address = my_headless_function_address;
         address <
         my_headless_function_address + my_headless_function_reported_size;
         ++address) {
      EXPECT_THROW({ function_containing_address(elf_so.elf(), address); },
                   cxxtrace_check_rseq::function_contains_unallocated_bytes)
        << "address: " << std::hex << std::showbase << address
        << " (_my_headless_function)";
    }
  }
}

TEST(test_elf_analyzer,
     function_containing_address_with_multiple_identical_function_symbols)
{
  auto assembly_code = R"(
    .section .text_cxxtrace_test, "ax", @progbits
    .globl _my_fully_overlapping_function_1
    .type _my_fully_overlapping_function_1, @function
    _my_fully_overlapping_function_1:
    .globl _my_fully_overlapping_function_2
    .type _my_fully_overlapping_function_2, @function
    _my_fully_overlapping_function_2:
      ret // Address: 0x0000555555554000
    .size _my_fully_overlapping_function_1, . - _my_fully_overlapping_function_1
    .size _my_fully_overlapping_function_2, . - _my_fully_overlapping_function_2
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86,
    assembly_code,
    {
      "-Wl,--section-start=.text_cxxtrace_test=0000555555554000",
    });

  auto my_fully_overlapping_function_address =
    machine_address{ 0x0000555555554000 };
  auto my_fully_overlapping_function_size = std::uint64_t{ 1 };

  CXXTRACE_SCOPED_TRACE() << "address: " << std::hex << std::showbase
                          << my_fully_overlapping_function_address;
  auto function = function_containing_address(
    elf_so.elf(), my_fully_overlapping_function_address);
  EXPECT_THAT(function.name,
              AnyOf(StrEq("_my_fully_overlapping_function_1"),
                    StrEq("_my_fully_overlapping_function_2")));
  EXPECT_EQ(function.base_address, my_fully_overlapping_function_address);
  EXPECT_EQ(function.instruction_bytes.size(),
            my_fully_overlapping_function_size);
  EXPECT_THAT(function.instruction_bytes,
              is_bytes(0xc3 // ret
                       ));
}

TEST(test_elf_analyzer,
     function_containing_address_with_overlapping_prefix_function_symbols)
{
  auto assembly_code = R"(
    .section .text_cxxtrace_test, "ax", @progbits
    .globl _my_prefix_overlapping_function_1
    .type _my_prefix_overlapping_function_1, @function
    _my_prefix_overlapping_function_1:
    .globl _my_prefix_overlapping_function_2
    .type _my_prefix_overlapping_function_2, @function
    _my_prefix_overlapping_function_2:
      pop %rax // Address: 0x0000555555554000
    .size _my_prefix_overlapping_function_1, . - _my_prefix_overlapping_function_1
      ret // Address: 0x0000555555554001
    .size _my_prefix_overlapping_function_2, . - _my_prefix_overlapping_function_2
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86,
    assembly_code,
    {
      "-Wl,--section-start=.text_cxxtrace_test=0000555555554000",
    });

  auto my_prefix_overlapping_function_address =
    machine_address{ 0x0000555555554000 };
  auto my_prefix_overlapping_function_overlapping_size = std::uint64_t{ 1 };
  {
    auto address = my_prefix_overlapping_function_address;
    EXPECT_THROW({ function_containing_address(elf_so.elf(), address); },
                 cxxtrace_check_rseq::multiple_functions_contain_address)
      << "address: " << std::hex << std::showbase << address
      << " (overlapping prefix)";
  }
  {
    auto address = my_prefix_overlapping_function_address +
                   my_prefix_overlapping_function_overlapping_size;
    CXXTRACE_SCOPED_TRACE() << "address: " << std::hex << std::showbase
                            << address << " (non-overlapping suffix)";
    auto function = function_containing_address(elf_so.elf(), address);
    EXPECT_EQ(function.name, "_my_prefix_overlapping_function_2");
    EXPECT_EQ(function.base_address, my_prefix_overlapping_function_address);
    EXPECT_THAT(function.instruction_bytes,
                is_bytes(0x58, // pop %rax
                         0xc3  // ret
                         ));
  }
}

TEST(test_elf_analyzer,
     function_containing_address_with_overlapping_suffix_function_symbols)
{
  auto assembly_code = R"(
    .section .text_cxxtrace_test, "ax", @progbits
    .globl _my_suffix_overlapping_function_1
    .type _my_suffix_overlapping_function_1, @function
    _my_suffix_overlapping_function_1:
    .globl _my_suffix_overlapping_function_2
    .type _my_suffix_overlapping_function_2, @function
      push %rax // Address: 0x0000555555554000
    _my_suffix_overlapping_function_2:
      ret // Address: 0x0000555555554001
    .size _my_suffix_overlapping_function_2, . - _my_suffix_overlapping_function_2
    .size _my_suffix_overlapping_function_1, . - _my_suffix_overlapping_function_1
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86,
    assembly_code,
    {
      "-Wl,--section-start=.text_cxxtrace_test=0000555555554000",
    });

  auto my_suffix_overlapping_function_address =
    machine_address{ 0x0000555555554000 };
  auto my_suffix_overlapping_function_overlapping_size = std::uint64_t{ 1 };
  {
    auto address = my_suffix_overlapping_function_address;
    CXXTRACE_SCOPED_TRACE() << "address: " << std::hex << std::showbase
                            << address << " (non-overlapping prefix)";
    auto function = function_containing_address(elf_so.elf(), address);
    EXPECT_EQ(function.name, "_my_suffix_overlapping_function_1");
    EXPECT_EQ(function.base_address, my_suffix_overlapping_function_address);
    EXPECT_THAT(function.instruction_bytes,
                is_bytes(0x50, // push %rax
                         0xc3  // ret
                         ));
  }
  {
    auto address = my_suffix_overlapping_function_address +
                   my_suffix_overlapping_function_overlapping_size;
    EXPECT_THROW({ function_containing_address(elf_so.elf(), address); },
                 cxxtrace_check_rseq::multiple_functions_contain_address)
      << "address: " << std::hex << std::showbase << address
      << " (overlapping suffix)";
  }
}

TEST(test_elf_analyzer,
     function_containing_address_with_partially_overlapping_function_symbols)
{
  auto assembly_code = R"(
    .section .text_cxxtrace_test, "ax", @progbits
    .globl _my_partially_overlapping_function_1
    .type _my_partially_overlapping_function_1, @function
    _my_partially_overlapping_function_1:
    .globl _my_partially_overlapping_function_2
    .type _my_partially_overlapping_function_2, @function
      push %rax // Address: 0x0000555555554000
    _my_partially_overlapping_function_2:
      pop %rax // Address: 0x0000555555554001
    .size _my_partially_overlapping_function_1, . - _my_partially_overlapping_function_1
      ret // Address: 0x0000555555554002
    .size _my_partially_overlapping_function_2, . - _my_partially_overlapping_function_2
  )";
  auto elf_so = assemble_into_elf_shared_object(
    machine_architecture::x86,
    assembly_code,
    {
      "-Wl,--section-start=.text_cxxtrace_test=0000555555554000",
    });

  auto my_partially_overlapping_function_address =
    machine_address{ 0x0000555555554000 };
  auto my_partially_overlapping_function_unique_prefix_size =
    std::uint64_t{ 1 };
  auto my_partially_overlapping_function_unique_suffix_size =
    std::uint64_t{ 1 };
  {
    auto address = my_partially_overlapping_function_address;
    CXXTRACE_SCOPED_TRACE() << "address: " << std::hex << std::showbase
                            << address << " (non-overlapping prefix)";
    auto function = function_containing_address(elf_so.elf(), address);
    EXPECT_EQ(function.name, "_my_partially_overlapping_function_1");
    EXPECT_EQ(function.base_address, my_partially_overlapping_function_address);
    EXPECT_THAT(function.instruction_bytes,
                is_bytes(0x50, // push %rax
                         0x58  // pop %rax
                         ));
  }
  {
    auto address = my_partially_overlapping_function_address +
                   my_partially_overlapping_function_unique_prefix_size;
    EXPECT_THROW({ function_containing_address(elf_so.elf(), address); },
                 cxxtrace_check_rseq::multiple_functions_contain_address)
      << "address: " << std::hex << std::showbase << address
      << " (overlapping middle)";
  }
  {
    auto address = my_partially_overlapping_function_address +
                   my_partially_overlapping_function_unique_prefix_size +
                   my_partially_overlapping_function_unique_suffix_size;
    CXXTRACE_SCOPED_TRACE() << "address: " << std::hex << std::showbase
                            << address << " (non-overlapping suffix)";
    auto function = function_containing_address(elf_so.elf(), address);
    EXPECT_EQ(function.name, "_my_partially_overlapping_function_2");
    EXPECT_EQ(function.base_address,
              my_partially_overlapping_function_address +
                my_partially_overlapping_function_unique_prefix_size);
    EXPECT_THAT(function.instruction_bytes,
                is_bytes(0x58, // pop %rax
                         0xc3  // ret
                         ));
  }
}
}
