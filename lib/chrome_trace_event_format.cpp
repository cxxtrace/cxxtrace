#include <array>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cxxtrace/chrome_trace_event_format.h>
#include <cxxtrace/clock.h>
#include <cxxtrace/sample.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <limits>
#include <ostream>
#include <string_view>
#include <system_error>
#include <type_traits>

#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION <= 8000
// Work around std::to_chars adding leading zeros.
// Bug report: https://bugs.llvm.org/show_bug.cgi?id=42166
#define CXXTRACE_WORK_AROUND_LIBCXX_42166 1
#endif

namespace cxxtrace {
namespace {
// Reset the formatting options to their defaults.
//
// See std::basic_ios<>::init for details.
auto
reset_ostream_formatting(std::ostream& stream)
{
  stream.fill(stream.widen(' '));
  stream.flags(std::ios_base::dec | std::ios_base::skipws);
  stream.precision(6);
  stream.width(0);
}
}

chrome_trace_event_writer::chrome_trace_event_writer(
  std::ostream* output) noexcept
  : output{ output }
{}

auto
chrome_trace_event_writer::write_snapshot(const samples_snapshot& snapshot)
  -> void
{
  reset_ostream_formatting(*this->output);

  *this->output << "{\"traceEvents\": [";
  auto should_output_comma = false;
  for (auto tid : snapshot.thread_ids()) {
    if (should_output_comma) {
      *this->output << ',';
    }
    should_output_comma = true;
    auto thread_name = czstring{ snapshot.thread_name(tid) };
    if (thread_name) {
      // TODO(strager): Write a useful process ID.
      *this->output << "{\"ph\": \"M\", \"pid\": 0, \"tid\": ";
      this->write_number(tid);
      *this->output << ", \"name\": \"thread_name\", \"args\": {\"name\": \"";
      this->write_string_piece(thread_name);
      *this->output << "\"}}";
    }
  }
  for (auto i = samples_snapshot::size_type{ 0 }; i < snapshot.size(); ++i) {
    if (should_output_comma) {
      *this->output << ',';
    }
    should_output_comma = true;
    this->write_sample(snapshot.at(i));
  }
  *this->output << "]}";
}

auto
chrome_trace_event_writer::close() -> void
{}

auto
chrome_trace_event_writer::write_sample(sample_ref sample) -> void
{
  *this->output << "{\"ph\": \"";
  switch (sample.kind()) {
    case sample_kind::enter_span:
      *this->output << 'B';
      break;
    case sample_kind::exit_span:
      *this->output << 'E';
      break;
  }
  *this->output << "\", \"cat\": \"";
  this->write_string_piece(sample.category());
  *this->output << "\", \"name\": \"";
  this->write_string_piece(sample.name());
  *this->output << "\", \"tid\": ";
  this->write_number(sample.thread_id());
  *this->output << ", \"ts\": ";
  auto timestamp_nanoseconds =
    sample.timestamp().nanoseconds_since_reference().count();
  assert(timestamp_nanoseconds >= 0);
  this->write_number(timestamp_nanoseconds / 1000);
  *this->output << ".";
  this->output->width(3);
  this->output->fill('0');
  this->write_number(timestamp_nanoseconds % 1000);
  // TODO(strager): Write a useful process ID.
  *this->output << ", \"pid\": 0";
  *this->output << "}";
}

template<class T>
auto
chrome_trace_event_writer::write_number(T number)
  -> std::enable_if_t<std::is_integral_v<T>, void>
{
  using limits = std::numeric_limits<T>;
  constexpr auto max_length =
    (limits::digits10 + 1) + (limits::is_signed ? 1 : 0);
  auto buffer = std::array<char, max_length>{};
  auto chars = zstring{ buffer.data() };
  auto result = std::to_chars(chars, &chars[buffer.size()], number);
  assert(result.ec == std::errc{});
  auto number_chars =
    std::string_view{ chars, static_cast<std::size_t>(result.ptr - chars) };

#if CXXTRACE_WORK_AROUND_LIBCXX_42166
  {
    auto non_zero_digit_index = number_chars.find_first_not_of('0');
    if (non_zero_digit_index != number_chars.npos) {
      number_chars.remove_prefix(non_zero_digit_index);
    }
  }
#endif

  *this->output << number_chars;
}

auto
chrome_trace_event_writer::write_string_piece(czstring data) -> void
{
  // TODO(strager): Escape special JSON characters such as " and \. See the
  // names_with_special_json_characters_are_unsupported test for some examples.
  *this->output << data;
}
}
