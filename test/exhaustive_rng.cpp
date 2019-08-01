#include "exhaustive_rng.h"
#include <vector>

namespace cxxtrace_test {
exhaustive_rng::exhaustive_rng() noexcept = default;

exhaustive_rng::~exhaustive_rng() = default;

auto
exhaustive_rng::next_integer_0(int max_plus_one) noexcept -> int
{
  if (this->counter_index >= this->counters.size()) {
    this->counters.emplace_back(0);
  }
  if (this->counter_index >= this->counter_limits.size()) {
    this->counter_limits.emplace_back(max_plus_one);
  } else {
    this->counter_limits[this->counter_index] = max_plus_one;
  }
  auto result = this->counters[this->counter_index];
  this->counter_index += 1;
  return result;
}

auto
exhaustive_rng::next_integer(int min, int max_plus_one) noexcept -> int
{
  return min + this->next_integer_0(max_plus_one - min);
}

auto
exhaustive_rng::done() const noexcept -> bool
{
  return this->done_;
}

auto
exhaustive_rng::lap() noexcept -> void
{
  // Update this->counters from right to left.
  auto i = this->counter_index;
  for (;;) {
    if (i == 0) {
      this->done_ = true;
      break;
    }
    i -= 1;
    this->counters[i] += 1;
    if (this->counters[i] != this->counter_limits[i]) {
      break;
    }
    this->counters[i] = 0;
  }
  this->counter_index = 0;
}
}
