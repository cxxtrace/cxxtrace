#ifndef CXXTRACE_EXHAUSTIVE_RNG_H
#define CXXTRACE_EXHAUSTIVE_RNG_H

#include "memory_resource_fwd.h"
#include <memory>

namespace cxxtrace_test {
class exhaustive_rng
{
public:
  explicit exhaustive_rng();
  explicit exhaustive_rng(std::experimental::pmr::memory_resource*);

  exhaustive_rng(const exhaustive_rng&) = delete;
  exhaustive_rng& operator=(const exhaustive_rng&) = delete;

  ~exhaustive_rng();

  auto next_integer_0(int max_plus_one) noexcept -> int;
  auto next_integer(int min, int max_plus_one) noexcept -> int;

  auto done() const noexcept -> bool;
  auto lap() noexcept -> void;

private:
  class impl;

  impl* impl_;
};
}

#endif
