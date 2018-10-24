#include <cxxtrace/thread.h>
#include <type_traits>

#if defined(__APPLE__) && defined(__MACH__)
#include <exception>
#include <mach/kern_return.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <mach/thread_act.h>
#include <mach/thread_info.h>
#endif

static_assert(std::is_integral_v<cxxtrace::thread_id>);

namespace cxxtrace {
namespace {
#if defined(__APPLE__) && defined(__MACH__)
auto get_thread_identifier_info_or_terminate(thread_act_t) noexcept
  -> thread_identifier_info;
#endif
}

#if defined(__APPLE__) && defined(__MACH__)
auto
get_current_thread_id() noexcept -> thread_id
{
  auto thread_port = mach_thread_self();
  auto info = get_thread_identifier_info_or_terminate(thread_port);
  return thread_id{ info.thread_id };

  // Intentionally leak thread_port. Holding a reference does not prevent the
  // thread from being cleaned up.
}
#endif

namespace {
#if defined(__APPLE__) && defined(__MACH__)
auto
get_thread_identifier_info_or_terminate(thread_act_t thread_port) noexcept
  -> thread_identifier_info
{
  auto info = thread_identifier_info_data_t{};
  auto info_count = mach_msg_type_number_t{ THREAD_IDENTIFIER_INFO_COUNT };
  auto rc = thread_info(thread_port,
                        THREAD_IDENTIFIER_INFO,
                        reinterpret_cast<thread_info_t>(&info),
                        &info_count);
  if (rc != KERN_SUCCESS) {
    mach_error("fatal: could not determine thread ID of current thread:", rc);
    std::terminate();
  }
  return info;
}
#endif
}
}
