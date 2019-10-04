#if !CXXTRACE_ENABLE_CDSCHECKER
#error "CXXTRACE_ENABLE_CDSCHECKER must be defined and non-zero."
#endif

#include "cdschecker_exhaustive_rng.h"
#include "cdschecker_model_memory_resource.h"
#include "exhaustive_rng.h"
#include <cstddef>
#include <cxxtrace/detail/warning.h>
#include <cxxtrace/string.h>
#include <dlfcn.h>
#include <new>
#include <stdexcept>

CXXTRACE_WARNING_PUSH
CXXTRACE_WARNING_IGNORE_CLANG("-Wgnu-zero-variadic-macro-arguments")
CXXTRACE_WARNING_IGNORE_CLANG("-Wunused-parameter")
CXXTRACE_WARNING_IGNORE_GCC("-Wunused-parameter")
#include <model.h>
#include <mymemory.h>
#include <plugins.h>
#include <stl-model.h>
#include <traceanalysis.h>
// IWYU pragma: no_forward_declare ModelExecution
CXXTRACE_WARNING_POP

namespace cxxtrace_test {
auto
cdschecker_exhaustive_rng_plugin_name() noexcept -> cxxtrace::czstring
{
  return "STRAGER";
}

namespace {
class exhaustive_rng_cdschecker_plugin : public ::TraceAnalysis
{
public:
  explicit exhaustive_rng_cdschecker_plugin() noexcept
    : rng{ get_cdschecker_model_memory_resource() }
  {}

  auto setExecution(::ModelExecution*) -> void override {}

  auto analyze(::action_list_t*) -> void override {}

  auto name() -> cxxtrace::czstring override
  {
    return cdschecker_exhaustive_rng_plugin_name();
  }

  auto option(char*) -> bool override { return false; }

  auto finish() -> void override {}

  auto actionAtInstallation() -> void override
  {
    model->set_inspect_plugin(this);
  }

  auto actionAtModelCheckingFinish() -> void override
  {
    this->rng.lap();
    if (!this->rng.done()) {
      // @@@ is there a better function than restart? restart does not
      // increment 'Total executions', and adds noise to the logs ("*******
      // Model-checking RESTART: *******")
      model->restart();
    }
  }

  static auto instance() -> exhaustive_rng_cdschecker_plugin*
  {
    static auto* instance = new exhaustive_rng_cdschecker_plugin{};
    return instance;
  }

  exhaustive_rng rng;

  CXXTRACE_WARNING_PUSH
  CXXTRACE_WARNING_IGNORE_CLANG("-Wunused-parameter")
  CXXTRACE_WARNING_IGNORE_GCC("-Wunused-parameter")
  MEMALLOC
  CXXTRACE_WARNING_POP
};
}

auto
concurrency_rng_next_integer_0(int max_plus_one) noexcept -> int
{
  model->set_inspect_plugin(exhaustive_rng_cdschecker_plugin::instance());
  return exhaustive_rng_cdschecker_plugin::instance()->rng.next_integer_0(
    max_plus_one);
}
}

#if defined(__APPLE__)
auto
my_register_plugins() -> void
{
  fprintf(stderr, "my_register_plugins!\n");
}

/// @@@ doesn't work. delete.
#include "/Users/strager/Downloads/dyld-interposing.h"
DYLD_INTERPOSE(my_register_plugins, register_plugins)
#if 0
__attribute__((used))
__attribute__((section("__DATA,interpose")))
static struct {
  void *replacement;
  void *original;
} interpossie[] = {
  { .replacement = (void*)&my_register_plugins, .original = (void*)&register_plugins },
};
#endif
#else

namespace {
auto
cdschecker_register_plugins() -> void
{
  auto* cdschecker_register_plugins =
    reinterpret_cast<void (*)()>(::dlsym(RTLD_NEXT, "_Z16register_pluginsv"));
  if (!cdschecker_register_plugins) {
    throw std::runtime_error{ ::dlerror() };
  }
  return cdschecker_register_plugins();
}
}

auto
register_plugins() -> void
{
  cdschecker_register_plugins();
  getRegisteredTraceAnalysis()->push_back(
    cxxtrace_test::exhaustive_rng_cdschecker_plugin::instance());
}
#endif
