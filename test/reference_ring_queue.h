#ifndef CXXTRACE_REFERENCE_RING_QUEUE_H
#define CXXTRACE_REFERENCE_RING_QUEUE_H

#include <cstdint>
#include <vector>

template<class T, std::size_t Capacity>
class reference_ring_queue
{
public:
  explicit reference_ring_queue() noexcept = default;

  auto push(T item) -> void { this->items.emplace_back(std::move(item)); }

  auto pop_n_and_clear(std::size_t max_size) -> std::vector<T>
  {
    auto result =
      this->items.size() <= max_size
        ? this->items
        : std::vector<T>{ this->items.end() - max_size, this->items.end() };
    this->items.clear();
    return result;
  }

  auto clear() -> void { this->items.clear(); }

private:
  std::vector<T> items;
};

#endif
