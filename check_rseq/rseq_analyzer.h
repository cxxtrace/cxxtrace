#ifndef CXXTRACE_CHECK_RSEQ_RSEQ_ANALYZER_H
#define CXXTRACE_CHECK_RSEQ_RSEQ_ANALYZER_H

#include "elf_function.h"
#include "machine_code.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cxxtrace/detail/iostream.h>
#include <cxxtrace/string.h>
#include <libelf/gelf.h>
#include <libelf/libelf.h>
#include <optional>
#include <variant>
#include <vector>

namespace cxxtrace_check_rseq {
class rseq_analysis;

auto
analyze_rseq_critical_sections_in_file(cxxtrace::czstring file_path)
  -> rseq_analysis;

auto
analyze_rseq_critical_section(const elf_function&,
                              machine_address start_address,
                              machine_address post_commit_address,
                              machine_address abort_address) -> rseq_analysis;

constexpr auto rseq_signature_size = 4;

struct rseq_critical_section
{
  machine_address function_address;
  std::string function;

  machine_address start_address;
  machine_address post_commit_address;
  machine_address abort_address;
};

struct rseq_problem
{
  class problem_with_critical_section
  {
  public:
    rseq_critical_section critical_section;
  };

  class empty_critical_section : public problem_with_critical_section
  {
  public:
    auto critical_section_address() const -> machine_address;

    auto critical_section_function() const -> std::string
    {
      return this->critical_section.function;
    }

    friend auto operator<<(std::ostream&, const empty_critical_section&)
      -> std::ostream&;
  };

  class empty_function : public problem_with_critical_section
  {
  public:
    auto function_address() const -> machine_address
    {
      return this->critical_section.function_address;
    }

    auto function_name() const -> std::string
    {
      return this->critical_section.function;
    }

    friend auto operator<<(std::ostream&, const empty_function&)
      -> std::ostream&;
  };

  class incomplete_rseq_descriptor
  {
  public:
    machine_address descriptor_address;

    friend auto operator<<(std::ostream&, const incomplete_rseq_descriptor&)
      -> std::ostream&;
  };

  class interrupt : public problem_with_critical_section
  {
  public:
    machine_address interrupt_instruction_address;
    std::string interrupt_instruction_string;

    auto interrupt_instruction_function() const -> std::string
    {
      return this->critical_section.function;
    }

    friend auto operator<<(std::ostream&, const interrupt&) -> std::ostream&;
  };

  class invalid_abort_signature : public problem_with_critical_section
  {
  public:
    std::array<std::byte, rseq_signature_size> expected_signature;
    std::array<std::optional<std::byte>, rseq_signature_size> actual_signature;

    auto signature_address() const -> machine_address
    {
      return this->abort_address() - rseq_signature_size;
    }

    auto abort_address() const -> machine_address
    {
      return this->critical_section.abort_address;
    }

    auto abort_function_name() const -> std::string
    {
      return this->critical_section.function;
    }

    friend auto operator<<(std::ostream&, const invalid_abort_signature&)
      -> std::ostream&;
  };

  class inverted_critical_section : public problem_with_critical_section
  {
  public:
    auto critical_section_start_address() const -> machine_address
    {
      return this->critical_section.start_address;
    }

    auto critical_section_post_commit_address() const -> machine_address
    {
      return this->critical_section.post_commit_address;
    }

    auto critical_section_function() const -> std::string
    {
      return this->critical_section.function;
    }

    friend auto operator<<(std::ostream&, const inverted_critical_section&)
      -> std::ostream&;
  };

  class jump_into_critical_section : public problem_with_critical_section
  {
  public:
    machine_address jump_instruction_address;
    std::string jump_instruction_string;
    machine_address target_instruction_address;

    auto jump_instruction_function() const -> std::string
    {
      return this->critical_section.function;
    }

    friend auto operator<<(std::ostream&, const jump_into_critical_section&)
      -> std::ostream&;
  };

  class label_outside_function : public problem_with_critical_section
  {
  public:
    enum class kind
    {
      start,
      post_commit,
      abort,
    };

    kind label_kind;

    auto label_address() const -> machine_address;

    auto label_function() const -> std::string
    {
      return this->critical_section.function;
    }

    friend auto operator<<(std::ostream&, const label_outside_function&)
      -> std::ostream&;
  };

  class no_rseq_descriptors
  {
  public:
    std::string section_name;

    friend auto operator<<(std::ostream&, const no_rseq_descriptors&)
      -> std::ostream&;
  };

  class stack_pointer_modified : public problem_with_critical_section
  {
  public:
    machine_address modifying_instruction_address;
    std::string modifying_instruction_string;

    auto modifying_instruction_function() const -> std::string
    {
      return this->critical_section.function;
    }

    friend auto operator<<(std::ostream&, const stack_pointer_modified&)
      -> std::ostream&;
  };
};

using any_rseq_problem = std::variant<rseq_problem::empty_critical_section,
                                      rseq_problem::empty_function,
                                      rseq_problem::incomplete_rseq_descriptor,
                                      rseq_problem::interrupt,
                                      rseq_problem::invalid_abort_signature,
                                      rseq_problem::inverted_critical_section,
                                      rseq_problem::jump_into_critical_section,
                                      rseq_problem::label_outside_function,
                                      rseq_problem::no_rseq_descriptors,
                                      rseq_problem::stack_pointer_modified>;

auto
PrintTo(const any_rseq_problem&, std::ostream*) -> void;

class rseq_analysis
{
public:
  auto problems() const -> std::vector<any_rseq_problem>
  {
    return this->problems_;
  }

  auto add_problem(any_rseq_problem problem) -> void
  {
    this->problems_.emplace_back(std::move(problem));
  }

private:
  std::vector<any_rseq_problem> problems_;
};

struct parsed_rseq_descriptor
{
  static constexpr auto size = 32;

  auto is_complete() const noexcept -> bool;

  machine_address descriptor_address;
  std::optional<std::uint32_t> version;
  std::optional<std::uint32_t> flags;
  std::optional<std::uint64_t> start_ip;
  std::optional<std::uint64_t> post_commit_offset;
  std::optional<std::uint64_t> abort_ip;
};

auto
parse_rseq_descriptors(::Elf*) -> std::vector<parsed_rseq_descriptor>;
}

#endif
