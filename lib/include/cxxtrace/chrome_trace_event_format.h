#ifndef CXXTRACE_CHROME_TRACE_EVENT_FORMAT_H
#define CXXTRACE_CHROME_TRACE_EVENT_FORMAT_H

#include <cxxtrace/string.h>
#include <iosfwd>
#include <type_traits>

namespace cxxtrace {
class samples_snapshot;

// Write a trace file in the Chrome Trace Event format [1].
//
// Chrome Trace Event files can be interpreted by tools such as Catapult's Trace
// Viewer [2] and Trace Compass (with a plugin from the TraceCompass Incubator)
// [3].
//
// [1]
// https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview#
// [2]
// https://github.com/catapult-project/catapult/blob/53913cecb11a3ef993f6496b9110964e2e2aeec3/tracing/README.md
// [3] https://www.eclipse.org/tracecompass/
class chrome_trace_event_writer
{
public:
  explicit chrome_trace_event_writer(std::ostream* output) noexcept;

  chrome_trace_event_writer(const chrome_trace_event_writer&) = delete;
  chrome_trace_event_writer& operator=(const chrome_trace_event_writer&) =
    delete;

  chrome_trace_event_writer(chrome_trace_event_writer&&) noexcept = default;
  chrome_trace_event_writer& operator=(chrome_trace_event_writer&&) noexcept =
    default;

  auto write_snapshot(const samples_snapshot& snapshot) -> void;

  auto close() -> void;

private:
  template<class T>
  auto write_number(T number) -> std::enable_if_t<std::is_integral_v<T>, void>;

  auto write_string_piece(czstring data) -> void;

  std::ostream* output;
};
}

#endif
