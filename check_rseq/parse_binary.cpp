#include "parse_binary.h"
#include <cassert>

namespace cxxtrace_check_rseq {
binary_parser::binary_parser(const std::byte* data,
                             std::size_t data_size) noexcept
  : binary_parser{ data, data + data_size }
{}

binary_parser::binary_parser(const std::byte* data_begin,
                             const std::byte* data_end) noexcept
  : cursor_{ data_begin }
  , end_{ data_end }
{}

auto
binary_parser::cursor() const noexcept -> const std::byte*
{
  return this->cursor_;
}

auto
binary_parser::remaining_byte_count() const noexcept -> std::size_t
{
  assert(this->end_ >= this->cursor_);
  return std::size_t(this->end_ - this->cursor_);
}

auto
binary_parser::parse_4_byte_unsigned_little_endian_integer() noexcept
  -> std::optional<std::uint32_t>
{
  return this->parse_unsigned_little_endian_integer<std::uint32_t, 4>();
}

auto
binary_parser::parse_8_byte_unsigned_little_endian_integer() noexcept
  -> std::optional<std::uint64_t>
{
  return this->parse_unsigned_little_endian_integer<std::uint64_t, 8>();
}

template<class Integer, int ByteCount>
auto
binary_parser::parse_unsigned_little_endian_integer() noexcept
  -> std::optional<Integer>
{
  static_assert(std::is_unsigned_v<Integer>);
  if (const auto* bytes = this->get_and_advance(ByteCount)) {
    auto result = Integer{ 0 };
    for (auto index = 0; index < ByteCount; ++index) {
      result |= Integer(bytes[index]) << (index * 8);
    }
    return result;
  } else {
    return std::nullopt;
  }
}

auto
binary_parser::get_and_advance(std::size_t byte_count) noexcept
  -> const std::byte*
{
  if (!this->have_space(byte_count)) {
    this->cursor_ = this->end_;
    return nullptr;
  }
  const auto* old_cursor = this->cursor_;
  this->cursor_ = old_cursor + byte_count;
  return old_cursor;
}

auto
binary_parser::have_space(std::size_t byte_count) const noexcept -> bool
{
  assert(this->end_ >= this->cursor_);
  return this->remaining_byte_count() >= byte_count;
}
}
