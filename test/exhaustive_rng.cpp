#include "exhaustive_rng.h"
#include "memory_resource.h"
#include <cassert>
#include <experimental/memory_resource>
#include <experimental/vector>
#include <memory>
#include <vector>

namespace cxxtrace_test {
class exhaustive_rng::impl
{
public:
  explicit impl(std::experimental::pmr::memory_resource* memory)
    : counters{ memory }
    , counter_limits{ memory }
  {
    assert(memory);
  }

  auto next_integer_0(int max_plus_one) noexcept -> int
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

  auto lap() noexcept -> void
  {
    // Update this->counters from right to left.
    auto i = this->counter_index;
    for (;;) {
      if (i == 0) {
        this->done = true;
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

  auto memory() noexcept -> std::experimental::pmr::memory_resource*
  {
    return this->counters.get_allocator().resource();
  }

  std::experimental::pmr::vector<int> counters;
  std::experimental::pmr::vector<int> counter_limits;
  typename std::experimental::pmr::vector<int>::size_type counter_index{ 0 };
  bool done{ false };
};

exhaustive_rng::exhaustive_rng()
  : exhaustive_rng{ std::experimental::pmr::get_default_resource() }
{}

exhaustive_rng::exhaustive_rng(std::experimental::pmr::memory_resource* memory)
  : impl_{ polymorphic_allocator<>{ memory }.new_object<impl>(memory) }
{}

exhaustive_rng::~exhaustive_rng()
{
  polymorphic_allocator<>{ this->impl_->memory() }.delete_object(this->impl_);
}

auto
exhaustive_rng::next_integer_0(int max_plus_one) noexcept -> int
{
  return this->impl_->next_integer_0(max_plus_one);
}

auto
exhaustive_rng::next_integer(int min, int max_plus_one) noexcept -> int
{
  return min + this->next_integer_0(max_plus_one - min);
}

auto
exhaustive_rng::done() const noexcept -> bool
{
  return this->impl_->done;
}

auto
exhaustive_rng::lap() noexcept -> void
{
  this->impl_->lap();
}
}
