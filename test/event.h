#ifndef CXXTRACE_EVENT_H
#define CXXTRACE_EVENT_H

#include <condition_variable>
#include <mutex>

namespace cxxtrace_test {
class event
{
public:
  auto set() -> void;
  auto wait() -> void;

private:
  std::mutex mutex{};
  std::condition_variable cond_var{};
  bool value{ false };
};
}

#endif
