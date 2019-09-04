#include "test_span.h"
#include <algorithm>
#include <cxxtrace/clock.h>
#include <cxxtrace/sample.h>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/span.h>
#include <gtest/gtest.h>
#include <memory>

#define CXXTRACE_SPAN(category, name)                                          \
  CXXTRACE_SPAN_WITH_CONFIG(this->get_cxxtrace_config(), category, name)

namespace cxxtrace_test {
TYPED_TEST(test_span, no_samples_exist_by_default)
{
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 0);
}

TYPED_TEST(test_span, span_adds_sample_at_scope_enter)
{
  auto span = CXXTRACE_SPAN("span category", "span name");
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 1);
  auto sample = cxxtrace::sample_ref{ samples.at(0) };
  EXPECT_STREQ(sample.category(), "span category");
  EXPECT_STREQ(sample.name(), "span name");
  EXPECT_EQ(sample.kind(), cxxtrace::sample_kind::enter_span);
}

TYPED_TEST(test_span, span_adds_sample_at_scope_exit)
{
  {
    auto span = CXXTRACE_SPAN("span category", "span name");
  }
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 2);
  auto sample = cxxtrace::sample_ref{ samples.at(samples.size() - 1) };
  EXPECT_STREQ(sample.category(), "span category");
  EXPECT_STREQ(sample.name(), "span name");
  EXPECT_EQ(sample.kind(), cxxtrace::sample_kind::exit_span);
}

TYPED_TEST(test_span, enter_samples_for_nested_spans_are_ordered)
{
  auto span_1 = CXXTRACE_SPAN("span category", "span name 1");
  auto span_2 = CXXTRACE_SPAN("span category", "span name 2");
  auto span_3 = CXXTRACE_SPAN("span category", "span name 3");
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 3);
  auto sample_1 = cxxtrace::sample_ref{ samples.at(0) };
  EXPECT_STREQ(sample_1.name(), "span name 1");
  auto sample_2 = cxxtrace::sample_ref{ samples.at(1) };
  EXPECT_STREQ(sample_2.name(), "span name 2");
  auto sample_3 = cxxtrace::sample_ref{ samples.at(2) };
  EXPECT_STREQ(sample_3.name(), "span name 3");
}

TYPED_TEST(test_span, exit_samples_for_nested_spans_are_ordered_and_adjacent)
{
  {
    auto span_1 = CXXTRACE_SPAN("span category", "span name 1");
    auto span_2 = CXXTRACE_SPAN("span category", "span name 2");
    auto span_3 = CXXTRACE_SPAN("span category", "span name 3");
  }
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 6);
  EXPECT_STREQ(samples.at(samples.size() - 3).name(), "span name 3");
  EXPECT_STREQ(samples.at(samples.size() - 2).name(), "span name 2");
  EXPECT_STREQ(samples.at(samples.size() - 1).name(), "span name 1");
}

TYPED_TEST(test_span,
           samples_for_incomplete_span_appears_after_sibling_complete_span)
{
  {
    auto complete_span = CXXTRACE_SPAN("span category", "complete span");
  }
  auto incomplete_span = CXXTRACE_SPAN("span category", "incomplete span");
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 3);
  EXPECT_STREQ(samples.at(0).name(), "complete span");
  EXPECT_STREQ(samples.at(1).name(), "complete span");
  EXPECT_STREQ(samples.at(2).name(), "incomplete span");
}

TYPED_TEST(test_span,
           sample_for_complete_span_appears_after_parent_incomplete_span)
{
  auto incomplete_span = CXXTRACE_SPAN("span category", "incomplete span");
  {
    auto complete_span = CXXTRACE_SPAN("span category", "complete span");
  }
  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  EXPECT_EQ(samples.size(), 3);
  EXPECT_STREQ(samples.at(0).name(), "incomplete span");
  EXPECT_STREQ(samples.at(1).name(), "complete span");
  EXPECT_STREQ(samples.at(2).name(), "complete span");
}

TYPED_TEST(test_span, later_snapshot_includes_only_new_spans)
{
  {
    auto span_before = CXXTRACE_SPAN("category", "before");
  }
  auto samples_1 = cxxtrace::samples_snapshot{ this->take_all_samples() };
  {
    auto span_after = CXXTRACE_SPAN("category", "after");
  }
  auto samples_2 = cxxtrace::samples_snapshot{ this->take_all_samples() };

  EXPECT_EQ(samples_1.size(), 2);
  EXPECT_STREQ(samples_1.at(0).name(), "before");
  EXPECT_STREQ(samples_1.at(1).name(), "before");
  EXPECT_EQ(samples_2.size(), 2);
  EXPECT_STREQ(samples_2.at(0).name(), "after");
  EXPECT_STREQ(samples_2.at(1).name(), "after");
}

TYPED_TEST(test_span, later_snapshot_includes_exit_sample_of_completed_span)
{
  auto samples_1 = [&] {
    auto span = CXXTRACE_SPAN("category", "test span");
    return cxxtrace::samples_snapshot{ this->take_all_samples() };
  }();
  auto samples_2 = cxxtrace::samples_snapshot{ this->take_all_samples() };

  EXPECT_EQ(samples_1.size(), 1);
  EXPECT_STREQ(samples_1.at(0).name(), "test span");
  EXPECT_EQ(samples_1.at(0).kind(), cxxtrace::sample_kind::enter_span);
  EXPECT_EQ(samples_2.size(), 1);
  EXPECT_STREQ(samples_2.at(0).name(), "test span");
  EXPECT_EQ(samples_2.at(0).kind(), cxxtrace::sample_kind::exit_span);
}

TYPED_TEST(test_span, span_samples_include_timestamps)
{
  auto& clock = this->clock();
  auto timestamp_before_span = clock.make_time_point(clock.query());
  auto timestamp_inside_span = [&] {
    auto span = CXXTRACE_SPAN("category", "span");
    return clock.make_time_point(clock.query());
  }();
  auto timestamp_after_span = clock.make_time_point(clock.query());

  auto samples = cxxtrace::samples_snapshot{ this->take_all_samples() };
  auto span_begin_timestamp = samples.at(0).timestamp();
  auto span_end_timestamp = samples.at(1).timestamp();
  EXPECT_LE(timestamp_before_span, span_begin_timestamp);
  EXPECT_LE(span_begin_timestamp, timestamp_inside_span);
  EXPECT_LE(timestamp_inside_span, span_end_timestamp);
  EXPECT_LE(span_end_timestamp, timestamp_after_span);
}
}
