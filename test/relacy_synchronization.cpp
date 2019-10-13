#if !CXXTRACE_ENABLE_RELACY
#error "CXXTRACE_ENABLE_RELACY must be defined and non-zero."
#endif

#include "cxxtrace_concurrency_test.h"
#include "relacy_synchronization.h"
#include <limits>
#include <ostream>
#include <relacy/context_base.hpp>
#include <relacy/defs.hpp>
#include <string>

namespace cxxtrace_test {
relacy_synchronization::backoff::backoff() = default;

relacy_synchronization::backoff::~backoff() = default;

auto
relacy_synchronization::backoff::reset() -> void
{
  this->backoff_ = rl::linear_backoff{};
}

auto
relacy_synchronization::backoff::yield(debug_source_location caller) -> void
{
  this->backoff_.yield(caller);
}

auto
relacy_synchronization::relaxed_semaphore::signal(debug_source_location caller)
  -> void
{
  CXXTRACE_ASSERT(this->value_ <
                  std::numeric_limits<decltype(this->value_)>::max());
  this->value_ += 1;
  auto unblocked = this->waiters_.unpark_one(rl::ctx(), caller);
  concurrency_log(
    [&](std::ostream& out) {
      out << "<" << std::hex << this << std::dec
          << "> relaxed_semaphore: signal value=" << this->value_
          << " unblocked=" << (unblocked ? "yes" : "no");
    },
    caller);
}

auto
relacy_synchronization::relaxed_semaphore::wait(debug_source_location caller)
  -> void
{
  auto minimum_value = std::numeric_limits<decltype(this->value_)>::min();
  if (this->value_ > 0) {
    CXXTRACE_ASSERT(this->value_ > minimum_value);
    this->value_ -= 1;
  } else {
    auto& context = rl::ctx();
    auto is_timed = false;
    auto spurious_wakeups = false;
    auto do_switch = true;
    auto unpark_reason = this->waiters_.park_current(
      context, is_timed, spurious_wakeups, do_switch, caller);
    CXXTRACE_ASSERT(unpark_reason == rl::unpark_reason_normal);
    CXXTRACE_ASSERT(this->value_ > minimum_value);
    this->value_ -= 1;
    context.switch_back(caller);
  }
  concurrency_log(
    [&](std::ostream& out) {
      out << "<" << std::hex << this << std::dec
          << "> relaxed_semaphore: wait succeeded value=" << this->value_;
    },
    caller);
}
}
