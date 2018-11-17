#ifndef CXXTRACE_UNBOUNDED_STORAGE_H
#define CXXTRACE_UNBOUNDED_STORAGE_H

#include <cxxtrace/string.h>
#include <cxxtrace/thread.h>
#include <cxxtrace/unbounded_unsafe_storage.h>
#include <mutex>
#include <vector>

namespace cxxtrace {
namespace detail {
enum class sample_kind;
struct sample;
}

class unbounded_storage
{
public:
  explicit unbounded_storage() noexcept;
  ~unbounded_storage() noexcept;

  unbounded_storage(const unbounded_storage&) = delete;
  unbounded_storage& operator=(const unbounded_storage&) = delete;
  unbounded_storage(unbounded_storage&&) = delete;
  unbounded_storage& operator=(unbounded_storage&&) = delete;

  auto reset() noexcept -> void;

  auto add_sample(czstring category,
                  czstring name,
                  detail::sample_kind,
                  thread_id) noexcept(false) -> void;
  auto add_sample(czstring category,
                  czstring name,
                  detail::sample_kind) noexcept(false) -> void;
  auto take_all_samples() noexcept(false) -> std::vector<detail::sample>;

private:
  std::mutex mutex{};
  unbounded_unsafe_storage storage{};
};
}

#endif
