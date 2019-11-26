#ifndef CXXTRACE_DETAIL_RSEQ_H
#define CXXTRACE_DETAIL_RSEQ_H

#include <cstdint>
#include <cxxtrace/detail/have.h>
// IWYU pragma: no_include <linux/rseq.h>

#if CXXTRACE_HAVE_RSEQ
struct rseq; // IWYU pragma: keep
#endif

namespace cxxtrace {
namespace detail {
class registered_rseq
{
public:
  using cpu_id_type = std::int32_t;

  static auto register_current_thread() noexcept -> registered_rseq;

  registered_rseq(const registered_rseq&) = delete;
  registered_rseq& operator=(const registered_rseq&) = delete;

  ~registered_rseq();

  [[gnu::always_inline]] auto get() const noexcept -> ::rseq*
  {
    return this->rseq;
  }

  auto read_cpu_id() const noexcept -> cpu_id_type;
  static auto read_cpu_id(const ::rseq&) noexcept -> cpu_id_type;

private:
  explicit registered_rseq(::rseq* rseq) noexcept;

  ::rseq* rseq;
};
}
}

#endif
