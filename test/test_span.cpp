#include <cstddef>
#include <cxxtrace/snapshot.h>
#include <cxxtrace/span.h>
#include <cxxtrace/string.h>
#include <cxxtrace/testing.h>
#include <gtest/gtest.h>

namespace {
class test_span : public testing::Test
{
protected:
  void TearDown() noexcept override { cxxtrace::clear_all_samples(); }
};

TEST_F(test_span, no_events_exist_by_default)
{
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 0);
}

TEST_F(test_span, span_adds_event_at_scope_enter)
{
  auto span = CXXTRACE_SPAN("span category", "span name");
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 1);
  auto event = cxxtrace::event_ref{ events.at(0) };
  EXPECT_STREQ(event.category(), "span category");
  EXPECT_STREQ(event.name(), "span name");
  EXPECT_EQ(event.kind(), cxxtrace::event_kind::incomplete_span);
}

TEST_F(test_span, incomplete_spans_includes_span_in_scope)
{
  auto span = CXXTRACE_SPAN("span category", "span name");
  auto spans =
    cxxtrace::incomplete_spans_snapshot{ cxxtrace::copy_incomplete_spans() };
  EXPECT_EQ(spans.size(), 1);
  EXPECT_STREQ(spans.at(0).category(), "span category");
  EXPECT_STREQ(spans.at(0).name(), "span name");
}

TEST_F(test_span, incomplete_spans_excludes_span_for_exited_scope)
{
  {
    auto span = CXXTRACE_SPAN("span category", "span name");
  }
  auto spans =
    cxxtrace::incomplete_spans_snapshot{ cxxtrace::copy_incomplete_spans() };
  EXPECT_EQ(spans.size(), 0);
}

TEST_F(test_span, span_adds_event_at_scope_exit)
{
  {
    auto span = CXXTRACE_SPAN("span category", "span name");
  }
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 1);
  auto event = cxxtrace::event_ref{ events.at(0) };
  EXPECT_STREQ(event.category(), "span category");
  EXPECT_STREQ(event.name(), "span name");
  EXPECT_EQ(event.kind(), cxxtrace::event_kind::span);
}

TEST_F(test_span, events_for_later_incomplete_spans_spans_appear_later)
{
  auto span_1 = CXXTRACE_SPAN("span category", "span name 1");
  auto span_2 = CXXTRACE_SPAN("span category", "span name 2");
  auto span_3 = CXXTRACE_SPAN("span category", "span name 3");
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 3);
  auto event_1 = cxxtrace::event_ref{ events.at(0) };
  EXPECT_STREQ(event_1.name(), "span name 1");
  auto event_2 = cxxtrace::event_ref{ events.at(1) };
  EXPECT_STREQ(event_2.name(), "span name 2");
  auto event_3 = cxxtrace::event_ref{ events.at(2) };
  EXPECT_STREQ(event_3.name(), "span name 3");
}

TEST_F(test_span, events_for_later_spans_appear_later)
{
  {
    auto span_1 = CXXTRACE_SPAN("span category", "span name 1");
    auto span_2 = CXXTRACE_SPAN("span category", "span name 2");
    auto span_3 = CXXTRACE_SPAN("span category", "span name 3");
  }
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 3);
  EXPECT_STREQ(events.at(0).name(), "span name 3");
  EXPECT_STREQ(events.at(1).name(), "span name 2");
  EXPECT_STREQ(events.at(2).name(), "span name 1");
}

TEST_F(test_span, event_for_incomplete_span_appears_after_sibling_complete_span)
{
  {
    auto complete_span = CXXTRACE_SPAN("span category", "complete span");
  }
  auto incomplete_span = CXXTRACE_SPAN("span category", "incomplete span");
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 2);
  EXPECT_STREQ(events.at(0).name(), "complete span");
  EXPECT_STREQ(events.at(1).name(), "incomplete span");
}

TEST_F(test_span, event_for_complete_span_appears_after_parent_incomplete_span)
{
  auto incomplete_span = CXXTRACE_SPAN("span category", "incomplete span");
  {
    auto complete_span = CXXTRACE_SPAN("span category", "complete span");
  }
  auto events = cxxtrace::events_snapshot{ cxxtrace::copy_all_events() };
  EXPECT_EQ(events.size(), 2);
  EXPECT_STREQ(events.at(0).name(), "incomplete span");
  EXPECT_STREQ(events.at(1).name(), "complete span");
}
}
