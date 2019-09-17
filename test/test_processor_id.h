#ifndef CXXTRACE_TEST_PROCESSOR_ID_H
#define CXXTRACE_TEST_PROCESSOR_ID_H

#include <cxxtrace/detail/have.h>
#include <cxxtrace/detail/processor.h>
#include <cxxtrace/string.h>
#include <gtest/gtest.h>

namespace cxxtrace_test {
namespace detail {
class any_processor_id_lookup
{
public:
  constexpr explicit any_processor_id_lookup(cxxtrace::czstring name) noexcept
    : name{ name }
  {}

  virtual ~any_processor_id_lookup() = default;

  virtual auto supported() const noexcept -> bool = 0;
  virtual auto get_current_processor_id() const noexcept
    -> cxxtrace::detail::processor_id = 0;

  virtual auto get_processor_id_namespace() const noexcept
    -> cxxtrace::detail::processor_id_namespace = 0;

  cxxtrace::czstring name;
};

template<class ProcessorIDLookup, int /*Counter*/>
struct type_erased_processor_id_lookup : public any_processor_id_lookup
{
  using any_processor_id_lookup::any_processor_id_lookup;

  auto supported() const noexcept -> bool override
  {
    return lookup.supported();
  }

  auto get_current_processor_id() const noexcept
    -> cxxtrace::detail::processor_id override
  {
    thread_local auto thread_local_cache =
      typename ProcessorIDLookup::thread_local_cache{ this->lookup };
    return lookup.get_current_processor_id(thread_local_cache);
  }

  auto get_processor_id_namespace() const noexcept
    -> cxxtrace::detail::processor_id_namespace override
  {
    return ProcessorIDLookup::id_namespace;
  }

  static auto instance(cxxtrace::czstring name)
    -> type_erased_processor_id_lookup&
  {
    static auto instance = type_erased_processor_id_lookup{ name };
    return instance;
  }

  ProcessorIDLookup lookup{};
};
}

namespace { // Avoid ODR violations due to __COUNTER__.
class test_processor_id
  : public testing::TestWithParam<detail::any_processor_id_lookup*>
{
public:
#define CXXTRACE_PROCESSOR_ID_LOOKUP(lookup)                                   \
  (&::cxxtrace_test::detail::type_erased_processor_id_lookup<                  \
    ::cxxtrace::detail::lookup,                                                \
    __COUNTER__>::instance(#lookup))
  static inline auto param_values = testing::Values(
#if defined(__x86_64__) && CXXTRACE_HAVE_APPLE_COMMPAGE
    CXXTRACE_PROCESSOR_ID_LOOKUP(
      processor_id_lookup_x86_cpuid_commpage_preempt_cached),
#endif
    CXXTRACE_PROCESSOR_ID_LOOKUP(processor_id_lookup),
#if CXXTRACE_HAVE_SCHED_GETCPU
    CXXTRACE_PROCESSOR_ID_LOOKUP(processor_id_lookup_sched_getcpu),
#endif
    CXXTRACE_PROCESSOR_ID_LOOKUP(processor_id_lookup_x86_cpuid_01h),
    CXXTRACE_PROCESSOR_ID_LOOKUP(processor_id_lookup_x86_cpuid_0bh),
    CXXTRACE_PROCESSOR_ID_LOOKUP(processor_id_lookup_x86_cpuid_1fh),
    CXXTRACE_PROCESSOR_ID_LOOKUP(processor_id_lookup_x86_cpuid_uncached));
#undef CXXTRACE_PROCESSOR_ID_LOOKUP

  static auto param_name(const testing::TestParamInfo<ParamType>& param)
    -> cxxtrace::czstring
  {
    return param.param->name;
  }

protected:
  auto supported() const noexcept -> bool
  {
    return this->GetParam()->supported();
  }

  auto get_current_processor_id() const noexcept
    -> cxxtrace::detail::processor_id
  {
    return this->GetParam()->get_current_processor_id();
  }

  auto get_processor_id_namespace() const noexcept
    -> cxxtrace::detail::processor_id_namespace
  {
    return this->GetParam()->get_processor_id_namespace();
  }
};
}
}

#endif
