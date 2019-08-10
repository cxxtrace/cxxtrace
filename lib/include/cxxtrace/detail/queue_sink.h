#ifndef CXXTRACE_DETAIL_QUEUE_SINK_H
#define CXXTRACE_DETAIL_QUEUE_SINK_H

#include <cassert>
#include <memory>
#include <utility>
#include <vector>

namespace cxxtrace {
namespace detail {
template<class T, class Func, class Allocator = std::allocator<T>>
class transform_vector_queue_sink
{
public:
  using container = std::vector<T, Allocator>;
  using size_type = typename container::size_type;
  using value_type = T;

  explicit transform_vector_queue_sink(container& output, Func func) noexcept
    : output{ output }
    , start_index{ output.size() }
    , func{ std::move(func) }
  {}

  template<class U>
  auto push_back(U item) noexcept -> void
  {
    assert(this->output.capacity() >= this->output.size() + 1);
    this->output.emplace_back(this->func(std::move(item)));
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
  Func func;
};

template<class T, class Allocator = std::allocator<T>>
class vector_queue_sink
{
public:
  using container = std::vector<T, Allocator>;
  using size_type = typename container::size_type;
  using value_type = T;

  explicit vector_queue_sink(container& output) noexcept
    : sink{ output, identity_functor{} }
  {}

  auto push_back(value_type item) noexcept -> void
  {
    this->sink.push_back(std::move(item));
  }

  auto pop_front_n(size_type size) noexcept -> void
  {
    this->sink.pop_front_n(size);
  }

  auto reserve(size_type size) noexcept(false) -> void
  {
    this->sink.reserve(size);
  }

private:
  struct identity_functor
  {
    auto operator()(T x) const noexcept -> T { return std::move(x); }
  };

  transform_vector_queue_sink<T, identity_functor, Allocator> sink;
};
}
}

#endif
