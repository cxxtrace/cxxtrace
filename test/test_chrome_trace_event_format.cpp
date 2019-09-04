#include "event.h"
#include "nlohmann_json.h"
#include "stringify.h"
#include "thread.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <cxxtrace/chrome_trace_event_format.h>
#include <cxxtrace/clock.h>
#include <cxxtrace/config.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/span.h>
#include <cxxtrace/unbounded_storage.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iterator>
#include <locale>
#include <sstream>
#include <thread>
#include <type_traits>

#define CXXTRACE_SPAN(category, name)                                          \
  CXXTRACE_SPAN_WITH_CONFIG(this->get_cxxtrace_config(), category, name)

using testing::AnyOf;
using testing::Eq;

namespace nlohmann {
auto
PrintTo(const nlohmann::json& value, std::ostream* output) -> void
{
  *output << value;
}
}

namespace {
auto
drop_metadata_events(const nlohmann::json& trace_events) -> nlohmann::json;

auto
get(const nlohmann::json& object, cxxtrace::czstring key) -> nlohmann::json;
}

namespace cxxtrace_test {
class test_chrome_trace_event_format : public testing::Test
{
public:
  explicit test_chrome_trace_event_format()
    : cxxtrace_config{ cxxtrace_storage, clock }
  {}

  auto TearDown() -> void override { this->reset_storage(); }

protected:
  using clock_type = cxxtrace::fake_clock;
  using storage_type = cxxtrace::unbounded_storage<clock_type::sample>;

  auto write_snapshot_and_parse() -> nlohmann::json
  {
    return this->write_snapshot_and_parse(this->take_all_samples());
  }

  auto write_snapshot_and_parse(const cxxtrace::samples_snapshot& snapshot)
    -> nlohmann::json
  {
    auto output = std::ostringstream{};
    auto writer = cxxtrace::chrome_trace_event_writer{ &output };
    writer.write_snapshot(snapshot);
    writer.close();
    return nlohmann::json::parse(output.str());
  }

  auto take_all_samples() -> cxxtrace::samples_snapshot
  {
    return this->cxxtrace_storage.take_all_samples(this->clock);
  }

  auto reset_storage() -> void { this->cxxtrace_storage.reset(); }

  auto get_cxxtrace_config() noexcept
    -> cxxtrace::basic_config<storage_type, clock_type>&
  {
    return this->cxxtrace_config;
  }

  clock_type clock{};

private:
  storage_type cxxtrace_storage{};
  cxxtrace::basic_config<storage_type, clock_type> cxxtrace_config;
};

TEST_F(test_chrome_trace_event_format, empty_trace_has_no_events)
{
  auto parsed = this->write_snapshot_and_parse();
  auto trace_events = get(parsed, "traceEvents");
  EXPECT_EQ(trace_events.type(), nlohmann::json::value_t::array);
  EXPECT_TRUE(trace_events.empty());
}

TEST_F(test_chrome_trace_event_format, span_adds_span_event)
{
  {
    auto span = CXXTRACE_SPAN("test category", "test span");
  }
  auto parsed = this->write_snapshot_and_parse();
  auto trace_events = drop_metadata_events(get(parsed, "traceEvents"));
  EXPECT_THAT(trace_events.size(), AnyOf(Eq(1), Eq(2)));
  if (trace_events.size() == 1) {
    auto event = trace_events.at(0);
    EXPECT_EQ(get(event, "ph"), "X");
    EXPECT_EQ(get(event, "cat"), "test category");
    EXPECT_EQ(get(event, "name"), "test span");
  } else {
    auto event_1 = trace_events.at(0);
    EXPECT_EQ(get(event_1, "ph"), "B");
    EXPECT_EQ(get(event_1, "cat"), "test category");
    EXPECT_EQ(get(event_1, "name"), "test span");

    auto event_2 = trace_events.at(1);
    EXPECT_EQ(get(event_2, "ph"), "E");
    EXPECT_EQ(get(event_2, "cat"), "test category");
    EXPECT_EQ(get(event_2, "name"), "test span");
  }
}

TEST_F(test_chrome_trace_event_format, events_have_required_fields)
{
  {
    auto span = CXXTRACE_SPAN("test category", "test span");
  }
  auto parsed = this->write_snapshot_and_parse();
  auto trace_events = get(parsed, "traceEvents");
  for (const auto& event : trace_events) {
    using value_t = nlohmann::json::value_t;
    SCOPED_TRACE(stringify(event));
    EXPECT_EQ(get(event, "ph").type(), value_t::string);
    auto ph = std::string{ event.value<std::string>("ph", "") };
    if (ph == "B" || ph == "E") {
      EXPECT_EQ(get(event, "cat").type(), value_t::string);
      EXPECT_EQ(get(event, "name").type(), value_t::string);
      EXPECT_TRUE(get(event, "pid").is_number_integer());
      EXPECT_TRUE(get(event, "tid").is_number_integer());
      EXPECT_TRUE(get(event, "ts").is_number());
    } else if (ph == "M") {
      auto name = get(event, "name");
      EXPECT_EQ(name.type(), value_t::string);
      EXPECT_TRUE(get(event, "pid").is_number_integer());
      EXPECT_TRUE(get(event, "tid").is_number_integer());
      if (name == "thread_name") {
        auto args = get(event, "args");
        EXPECT_EQ(args.type(), value_t::object);
        EXPECT_EQ(get(args, "name").type(), value_t::string);
      } else {
        ADD_FAILURE() << "Unknown metadata name: " << name;
      }
    } else {
      ADD_FAILURE() << "Unknown ph: \"" << ph << '"';
    }
  }
}

TEST_F(test_chrome_trace_event_format, span_timestamps_are_in_microseconds)
{
  constexpr auto span_enter_microseconds = 1;
  constexpr auto span_exit_microseconds = 11;
  constexpr auto span_duration_microseconds = 10;

  this->clock.set_duration_between_samples(
    std::chrono::microseconds{ span_duration_microseconds });
  this->clock.set_next_time_point(
    std::chrono::microseconds{ span_enter_microseconds });
  {
    auto span = CXXTRACE_SPAN("test category", "test span");
  }

  auto parsed = this->write_snapshot_and_parse();
  auto trace_events = drop_metadata_events(get(parsed, "traceEvents"));
  EXPECT_THAT(trace_events.size(), AnyOf(Eq(1), Eq(2)));
  if (trace_events.size() == 1) {
    auto event = trace_events.at(0);
    EXPECT_EQ(get(event, "ph"), "X");
    EXPECT_EQ(get(event, "ts"), span_enter_microseconds);
    EXPECT_EQ(get(event, "dur"), span_duration_microseconds);
  } else {
    auto event_1 = trace_events.at(0);
    EXPECT_EQ(get(event_1, "ph"), "B");
    EXPECT_EQ(get(event_1, "ts"), span_enter_microseconds);

    auto event_2 = trace_events.at(1);
    EXPECT_EQ(get(event_2, "ph"), "E");
    EXPECT_EQ(get(event_2, "ts"), span_exit_microseconds);
  }
}

TEST_F(test_chrome_trace_event_format,
       timestamps_include_fractional_microseconds)
{
  struct
  {
    int span_enter_nanoseconds;
    double expected_ts;
  } test_cases[] = {
    { 1234'567, 1234.567 }, //
    { 1'024, 1.024 },       // One leading zero in fraction.
    { 9'001, 9.001 },       // Two leading zeros in fraction.
    { 3'140, 3.140 },       // One trailing zero in fraction.
    { 7'600, 7.600 },       // Two trailing zeros in fraction.
    { 20, 0.020 },          // Zero integer component.
  };
  // fake_clock must be strictly increasing, and we reuse the clock between
  // iterations. Ensure faked clock values are strictly increasing.
  std::sort(std::begin(test_cases),
            std::end(test_cases),
            [](const auto& test_case_x, const auto& test_case_y) {
              return test_case_x.span_enter_nanoseconds <
                     test_case_y.span_enter_nanoseconds;
            });

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(
      stringify("span_enter_nanoseconds = ", test_case.span_enter_nanoseconds));

    this->clock.set_next_time_point(
      std::chrono::nanoseconds{ test_case.span_enter_nanoseconds });
    {
      auto span = CXXTRACE_SPAN("test category", "test span");
    }

    auto parsed = this->write_snapshot_and_parse();
    auto trace_events = drop_metadata_events(get(parsed, "traceEvents"));

    auto event = trace_events.at(0);
    EXPECT_NEAR(get(event, "ts").get<double>(), test_case.expected_ts, 0.001);
  }
}

TEST_F(test_chrome_trace_event_format, large_timestamps_round_trip)
{
  // In libc++ 8.0.0, std::to_chars writes leading zeros for large integers.
  // (See bug report: https://bugs.llvm.org/show_bug.cgi?id=42166) Leading zeros
  // confuse JSON parsers. Verify (indirectly) that chrome_trace_event_writer
  // does not write leading zeros.

  this->clock.set_next_time_point(std::chrono::nanoseconds{ 292986141227516 });
  auto span = CXXTRACE_SPAN("test category", "test span");

  EXPECT_NO_THROW({
    auto parsed = this->write_snapshot_and_parse();
    auto trace_events = drop_metadata_events(get(parsed, "traceEvents"));
    auto event = trace_events.at(0);
    get(event, "ts").dump(); // Force the number to be parsed.
  });
}

TEST_F(test_chrome_trace_event_format, incomplete_span_adds_span_begin_event)
{
  auto incomplete_span = CXXTRACE_SPAN("test category", "test span");
  auto parsed = this->write_snapshot_and_parse();
  auto trace_events = drop_metadata_events(get(parsed, "traceEvents"));
  EXPECT_EQ(trace_events.size(), 1);
  auto event = trace_events.at(0);
  EXPECT_EQ(get(event, "ph"), "B");
  EXPECT_EQ(get(event, "cat"), "test category");
  EXPECT_EQ(get(event, "name"), "test span");
}

TEST_F(test_chrome_trace_event_format, spans_include_thread_id)
{
  {
    auto main_span = CXXTRACE_SPAN("category", "main span");
  }
  auto main_thread_id = cxxtrace::get_current_thread_id();
  SCOPED_TRACE(stringify("main_thread_id: ", main_thread_id));

  auto thread_1_id = std::atomic<cxxtrace::thread_id>{};
  std::thread{
    [&] {
      auto thread_1_span = CXXTRACE_SPAN("category", "thread 1 span");
      thread_1_id = cxxtrace::get_current_thread_id();
    }
  }.join();
  SCOPED_TRACE(stringify("thread_1_id: ", thread_1_id));

  auto thread_2_id = std::atomic<cxxtrace::thread_id>{};
  std::thread{
    [&] {
      auto thread_2_span = CXXTRACE_SPAN("category", "thread 2 span");
      thread_2_id = cxxtrace::get_current_thread_id();
    }
  }.join();
  SCOPED_TRACE(stringify("thread_2_id: ", thread_2_id));

  auto parsed = this->write_snapshot_and_parse();
  auto trace_events = drop_metadata_events(get(parsed, "traceEvents"));
  auto found_main_span = false;
  auto found_thread_1_span = false;
  auto found_thread_2_span = false;
  for (const auto& event : trace_events) {
    SCOPED_TRACE(stringify(event));
    auto event_thread_id = get(event, "tid");
    auto event_name = get(event, "name");
    if (event_name == "main span") {
      EXPECT_EQ(event_thread_id, main_thread_id);
      found_main_span = true;
    } else if (event_name == "thread 1 span") {
      EXPECT_EQ(event_thread_id, thread_1_id.load());
      found_thread_1_span = true;
    } else if (event_name == "thread 2 span") {
      EXPECT_EQ(event_thread_id, thread_2_id.load());
      found_thread_2_span = true;
    }
  }
  EXPECT_TRUE(found_main_span);
  EXPECT_TRUE(found_thread_1_span);
  EXPECT_TRUE(found_thread_2_span);
}

TEST_F(test_chrome_trace_event_format, metadata_includes_thread_names)
{
  set_current_thread_name("main thread");
  auto main_thread_id = cxxtrace::get_current_thread_id();
  {
    auto main_thread_span = CXXTRACE_SPAN("category", "main thread span");
  }

  auto test_done = cxxtrace_test::event{};

  auto live_thread_ready = cxxtrace_test::event{};
  auto live_thread_id = cxxtrace::thread_id{};
  auto live_thread = std::thread{ [&] {
    set_current_thread_name("live thread");
    live_thread_id = cxxtrace::get_current_thread_id();
    {
      auto live_thread_span = CXXTRACE_SPAN("category", "live thread span");
    }
    live_thread_ready.set();
    test_done.wait();
  } };
  live_thread_ready.wait();

  auto dead_thread_id = cxxtrace::thread_id{};
  auto dead_thread = std::thread{ [&] {
    set_current_thread_name("dead thread");
    dead_thread_id = cxxtrace::get_current_thread_id();
    auto dead_thread_span = CXXTRACE_SPAN("category", "dead thread span");

    cxxtrace::remember_current_thread_name_for_next_snapshot(
      this->get_cxxtrace_config());
  } };
  dead_thread.join();

  auto parsed = this->write_snapshot_and_parse();
  auto trace_events = get(parsed, "traceEvents");
  auto found_main_thread_name = false;
  auto found_live_thread_name = false;
  auto found_dead_thread_name = false;
  for (const auto& event : trace_events) {
    SCOPED_TRACE(stringify(event));
    if (get(event, "ph") == "M" && get(event, "name") == "thread_name") {
      auto event_thread_id = get(event, "tid");
      if (event_thread_id == main_thread_id) {
        EXPECT_EQ(get(get(event, "args"), "name"), "main thread");
        found_main_thread_name = true;
      } else if (event_thread_id == live_thread_id) {
        EXPECT_EQ(get(get(event, "args"), "name"), "live thread");
        found_live_thread_name = true;
      } else if (event_thread_id == dead_thread_id) {
        EXPECT_EQ(get(get(event, "args"), "name"), "dead thread");
        found_dead_thread_name = true;
      }
    }
  }
  EXPECT_TRUE(found_main_thread_name);
  EXPECT_TRUE(found_live_thread_name);
  EXPECT_TRUE(found_dead_thread_name);

  test_done.set();
  live_thread.join();
}

TEST_F(test_chrome_trace_event_format,
       events_parse_as_json_with_funky_ostream_locale)
{
  for (auto locale_name : { "C", "en_US.UTF-8", "fr_FR.UTF-8" }) {
    SCOPED_TRACE(locale_name);

    auto locale = std::locale{};
    try {
      locale = std::locale{ locale_name };
    } catch (std::runtime_error& error) {
      std::fprintf(stderr,
                   "note: ignoring error constructing locale '%s': %s\n",
                   locale_name,
                   error.what());
      continue;
    }

    auto output = std::ostringstream{};
    output.imbue(locale);

    {
      auto span = CXXTRACE_SPAN("test category", "test span");
    }
    auto writer = cxxtrace::chrome_trace_event_writer{ &output };
    writer.write_snapshot(this->take_all_samples());
    writer.close();

    auto parsed = nlohmann::json::parse(output.str());
    EXPECT_EQ(parsed.type(), nlohmann::json::value_t::object);
  }
}

TEST_F(test_chrome_trace_event_format,
       events_parse_as_json_with_funky_ostream_formatting)
{
  auto output = std::ostringstream{};
  output.fill('@');
  output.flags(std::ios_base::floatfield | std::ios_base::hex |
               std::ios_base::internal | std::ios_base::scientific |
               std::ios_base::showbase | std::ios_base::showpoint |
               std::ios_base::showpos | std::ios_base::uppercase);
  output.width(20);

  {
    auto span = CXXTRACE_SPAN("test category", "test span");
  }
  auto writer = cxxtrace::chrome_trace_event_writer{ &output };
  writer.write_snapshot(this->take_all_samples());
  writer.close();

  auto parsed = nlohmann::json::parse(output.str());
  EXPECT_EQ(parsed.type(), nlohmann::json::value_t::object);
}

// TODO(strager): Teach chrome_trace_event_writer to escape strings to avoid
// these problems. This test documents the current behavior, not the desired
// behavior.
TEST_F(test_chrome_trace_event_format,
       names_with_special_json_characters_are_unsupported)
{
  auto parse_and_get_name = [&] {
    auto parsed = this->write_snapshot_and_parse();
    auto trace_events = drop_metadata_events(get(parsed, "traceEvents"));
    get(trace_events.at(0), "name");
  };

  {
    auto span = CXXTRACE_SPAN("test category", "\\");
    EXPECT_THROW({ parse_and_get_name(); }, nlohmann::json::parse_error);
    this->reset_storage();
  }

  {
    auto span = CXXTRACE_SPAN("test category", "\"");
    EXPECT_THROW({ parse_and_get_name(); }, nlohmann::json::parse_error);
    this->reset_storage();
  }
}
}

namespace {
auto
drop_metadata_events(const nlohmann::json& trace_events) -> nlohmann::json
{
  auto result = nlohmann::json{};
  for (auto& event : trace_events) {
    if (get(event, "ph") == "M") {
      continue;
    }
    result.emplace_back(event);
  }
  return result;
}

auto
get(const nlohmann::json& object, cxxtrace::czstring key) -> nlohmann::json
{
  return object.value(key, nlohmann::json{});
}
}
