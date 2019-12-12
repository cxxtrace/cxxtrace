#include "../posix_spawn.h"
#include "../stringify.h" // IWYU pragma: keep
#include "assembler.h"
#include "elf_function.h"
#include "libelf_support.h"
#include "machine_code.h"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstddef> // IWYU pragma: keep
#include <cstdlib>
#include <cstring>
#include <cxxtrace/detail/file_descriptor.h>
#include <cxxtrace/string.h>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <gelf.h>
#include <gtest/gtest.h>
#include <libelf/libelf.h>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>
// IWYU pragma: no_include "../stringify_impl.h"

using cxxtrace_check_rseq::machine_architecture;

namespace cxxtrace_test {
namespace {
struct temporary_file
{
  static auto make(cxxtrace::czstring mkstemp_template) -> temporary_file;

  cxxtrace::detail::file_descriptor file;
  std::string path;
};

auto
run_child_program(const cxxtrace::czstring* argv) -> void;

auto
escape_for_posix_shell(std::string_view s) -> std::string;

auto
write_full_string(int file_descriptor, cxxtrace::czstring data) -> void;

auto
unique_symbol(::Elf*, cxxtrace::czstring name) -> ::GElf_Sym;
}

assemble_exception::assemble_exception(cxxtrace::czstring message)
  : std::runtime_error{ message }
{}

temporary_elf_file::~temporary_elf_file()
{
  auto test_failed = testing::Test::HasFatalFailure() ||
                     testing::Test::HasNonfatalFailure() ||
                     std::uncaught_exceptions() > 0;
  if (!test_failed && !this->did_delete_files_) {
    this->delete_files();
  }
}

auto
temporary_elf_file::elf() const noexcept -> ::Elf*
{
  return this->elf_.get();
}

auto
temporary_elf_file::elf_path() const noexcept -> cxxtrace::czstring
{
  return this->elf_path_.c_str();
}

auto
temporary_elf_file::symbol(cxxtrace::czstring name) const
  -> cxxtrace_check_rseq::machine_address
{
  return unique_symbol(this->elf(), name).st_value;
}

auto
temporary_elf_file::delete_files() -> void
{
  assert(!this->did_delete_files_);
  assert(!this->elf_path_.empty());
  assert(!this->source_path_.empty());
  assert(this->elf_.valid());
  assert(this->elf_file_.valid());

  this->did_delete_files_ = true;

  this->elf_.end();
  this->elf_file_.close();
  auto deleted_elf = std::filesystem::remove(this->elf_path_);
  auto deleted_source = std::filesystem::remove(this->source_path_);
  if (!deleted_elf) {
    throw std::runtime_error{ stringify(
      "Failed to delete ", this->elf_path_, "; it didn't exist") };
  }
  if (!deleted_source) {
    throw std::runtime_error{ stringify(
      "Failed to delete ", this->source_path_, "; it didn't exist") };
  }
}

auto
assemble_into_elf_shared_object(
  machine_architecture architecture,
  cxxtrace::czstring assembly_code,
  std::initializer_list<cxxtrace::czstring> extra_cc_options)
  -> temporary_elf_file
{
  // TODO(strager): Allow cross-assembling.
  auto ok = false
#if defined(__x86_64__)
            || architecture == machine_architecture::x86
#endif
    ;
  if (!ok) {
    throw std::runtime_error{ "Unsupported architecture" };
  }

  auto source_file = temporary_file::make("cxxtrace_test_elf_source_XXXXXX");
  write_full_string(source_file.file.get(), assembly_code);
  source_file.file.close();

  auto elf_file = temporary_file::make("cxxtrace_test_elf_so_XXXXXX");
  elf_file.file.close();

  auto command = std::vector{
    "cc",
    "-shared",
    "-o",
    elf_file.path.c_str(),
    "-x",
    "assembler-with-cpp",
    source_file.path.c_str(),
  };
  command.insert(
    command.end(), extra_cc_options.begin(), extra_cc_options.end());
  command.emplace_back(nullptr);
  run_child_program(command.data());

  auto elf_file_descriptor = cxxtrace::detail::file_descriptor{ ::open(
    elf_file.path.c_str(), O_CLOEXEC | O_RDONLY) };
  if (!elf_file_descriptor.valid()) {
    throw std::system_error{ errno,
                             std::generic_category(),
                             "Opening ELF file failed" };
  }
  cxxtrace_check_rseq::initialize_libelf();
  auto elf = cxxtrace_check_rseq::elf_handle::begin(elf_file_descriptor.get(),
                                                    ::ELF_C_READ);

  return temporary_elf_file{
    .source_path_ = std::move(source_file.path),
    .elf_path_ = std::move(elf_file.path),
    .elf_file_ = std::move(elf_file_descriptor),
    .elf_ = std::move(elf),
  };
}

auto
assemble_function_into_elf_shared_object(
  cxxtrace_check_rseq::machine_architecture architecture,
  const std::string& assembly_code,
  cxxtrace::czstring function_name) -> temporary_elf_file
{
  auto full_assembly_code = std::string{} + R"(
    .text
    .globl )" + function_name +
                            R"(
    .type )" + function_name +
                            R"(, @function
    )" + function_name + R"(:
  )" + assembly_code + R"(
    .size )" + function_name +
                            R"(, . - )" + function_name + R"(
  )";
  return assemble_into_elf_shared_object(
    architecture, full_assembly_code.c_str(), {});
}

auto
function_from_symbol(::Elf* elf, cxxtrace::czstring name)
  -> cxxtrace_check_rseq::elf_function
{
  return cxxtrace_check_rseq::function_from_symbol(
    elf, unique_symbol(elf, name), name);
}

namespace {
auto
temporary_file::make(cxxtrace::czstring mkstemp_template) -> temporary_file
{
  auto path =
    (std::filesystem::temp_directory_path() / mkstemp_template).string();
  auto file = cxxtrace::detail::file_descriptor{ ::mkstemp(path.data()) };
  if (!file.valid()) {
    throw std::system_error{ errno, std::generic_category(), "mkstemp failed" };
  }
  return temporary_file{ .file = std::move(file), .path = std::move(path) };
}

auto
run_child_program(const cxxtrace::czstring* argv) -> void
{
  auto print_command_being_executed = [](const auto& argv) -> void {
    auto& stream = std::clog;
    stream << '$';
    for (const auto* arg = argv; *arg; ++arg) {
      stream << ' ' << escape_for_posix_shell(*arg);
    }
    stream << '\n';
  };
  print_command_being_executed(argv);
  auto assembler_process_id =
    posix_spawnp(argv[0],
                 /*file_actions=*/nullptr,
                 /*attributes=*/nullptr,
                 /*argv=*/argv,
                 /*envp=*/get_current_process_environment());
  auto status = wait_for_child_process_to_exit(assembler_process_id);
  auto exit_code = get_exit_code_from_waitpid_status(status);
  if (exit_code != 0) {
    throw assemble_exception{ "Program failed with non-zero exit code" };
  }
}

auto
escape_for_posix_shell(std::string_view s) -> std::string
{
  auto characters_allowed_without_escaping = std::string_view{
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ/.-,=0123456789_"
  };
  if (s.find_first_not_of(characters_allowed_without_escaping) == s.npos) {
    return std::string{ s };
  }

  auto result = std::string{};
  result += '\'';
  for (auto c : s) {
    if (c == '\'') {
      // Close the active single-quoted string.
      result += '\'';
      // Insert an escaped single quote (\').
      result += '\\';
      result += '\'';
      // Open a single-quoted string.
      result += '\'';
    } else {
      result += c;
    }
  }
  result += '\'';
  return result;
}

auto
write_full_string(int file_descriptor, cxxtrace::czstring data) -> void
{
  auto bytes_to_write = std::strlen(data);
  auto bytes_written = ::write(file_descriptor, data, bytes_to_write);
  if (bytes_written < 0) {
    throw std::system_error{ errno,
                             std::generic_category(),
                             "Writing to file failed" };
  }
  if (static_cast<std::size_t>(bytes_written) != bytes_to_write) {
    throw std::runtime_error{ "Writing to file failed; not all bytes written" };
  }
}

auto
unique_symbol(::Elf* elf, cxxtrace::czstring name) -> ::GElf_Sym
{
  auto candidate_symbol = std::optional<::GElf_Sym>{};
  cxxtrace_check_rseq::enumerate_symbols(
    elf,
    [&](const ::GElf_Shdr& section_header, const ::GElf_Sym& symbol) -> void {
      const auto* current_symbol_name =
        ::elf_strptr(elf, section_header.sh_link, symbol.st_name);
      assert(current_symbol_name);
      if (current_symbol_name) {
        if (std::strcmp(current_symbol_name, name) == 0) {
          if (candidate_symbol.has_value()) {
            throw std::runtime_error{ stringify(
              "Symbol ", name, " unexpectedly defined multiple times") };
          }
          candidate_symbol = symbol;
        }
      }
    });
  if (!candidate_symbol.has_value()) {
    throw std::runtime_error{ stringify("Symbol ", name, " was not defined") };
  }
  return *candidate_symbol;
}
}
}
