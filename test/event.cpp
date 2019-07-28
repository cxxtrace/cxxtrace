#include "event.h"
#include <mutex>

namespace cxxtrace_test {
auto
event::set() -> void
{
  auto lock = std::lock_guard{ this->mutex };
  this->value = true;
  this->cond_var.notify_all();
}

auto
event::wait() -> void
{
  auto lock = std::unique_lock{ this->mutex };
  this->cond_var.wait(lock, [&] { return this->value; });
}
}
