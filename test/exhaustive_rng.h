#ifndef CXXTRACE_EXHAUSTIVE_RNG_H
#define CXXTRACE_EXHAUSTIVE_RNG_H

#include <vector>

namespace cxxtrace_test {
class exhaustive_rng
{
public:
  explicit exhaustive_rng() noexcept;
  ~exhaustive_rng();

  auto next_integer_0(int max_plus_one) noexcept -> int;
  auto next_integer(int min, int max_plus_one) noexcept -> int;

  auto done() const noexcept -> bool;
  auto lap() noexcept -> void;

private:
  std::vector<int> counters{};
  std::vector<int> counter_limits{};
  typename std::vector<int>::size_type counter_index{ 0 };
  bool done_{ false };
};
}

#endif
