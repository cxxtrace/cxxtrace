#ifndef CXXTRACE_CHECK_RSEQ_PARSE_BINARY_H
#define CXXTRACE_CHECK_RSEQ_PARSE_BINARY_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace cxxtrace_check_rseq {
class binary_parser
{
public:
  explicit binary_parser(const std::byte* data, std::size_t data_size) noexcept;
  explicit binary_parser(const std::byte* data_begin,
                         const std::byte* data_end) noexcept;

  auto cursor() const noexcept -> const std::byte*;

  auto remaining_byte_count() const noexcept -> std::size_t;

  auto parse_4_byte_unsigned_little_endian_integer() noexcept
    -> std::optional<std::uint32_t>;

  auto parse_8_byte_unsigned_little_endian_integer() noexcept
    -> std::optional<std::uint64_t>;

private:
  template<class Integer, int ByteCount>
  auto parse_unsigned_little_endian_integer() noexcept
    -> std::optional<Integer>;

  auto get_and_advance(std::size_t byte_count) noexcept -> const std::byte*;
  auto have_space(std::size_t byte_count) const noexcept -> bool;

  const std::byte* cursor_;
  const std::byte* end_;
};
}

#endif
