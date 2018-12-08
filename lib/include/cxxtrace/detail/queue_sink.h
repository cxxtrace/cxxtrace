#ifndef CXXTRACE_DETAIL_QUEUE_SINK_H
#define CXXTRACE_DETAIL_QUEUE_SINK_H

#include <cassert>
#include <utility>
#include <vector>

namespace cxxtrace {
namespace detail {
template<class T>
class vector_queue_sink
{
public:
  using container = std::vector<T>;
  using size_type = typename container::size_type;
  using value_type = T;

  explicit vector_queue_sink(container& output) noexcept
    : output{ output }
    , start_index{ output.size() }
  {}

  auto push_back(value_type item) noexcept -> void
  {
    assert(this->output.capacity() >= this->output.size() + 1);
    this->output.emplace_back(std::move(item));
  }

  auto pop_front_n(size_type size) noexcept -> void
  {
    auto begin = this->output.begin() + this->start_index;
    this->output.erase(begin, begin + size);
  }

  auto reserve(size_type size) noexcept(false) -> void
  {
    this->output.reserve(this->start_index + size);
  }

private:
  container& output;
  size_type start_index;
};
}
}

#endif
