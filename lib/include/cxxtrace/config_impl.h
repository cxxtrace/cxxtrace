#ifndef CXXTRACE_CONFIG_IMPL_H
#define CXXTRACE_CONFIG_IMPL_H

#if !defined(CXXTRACE_CONFIG_H)
#error                                                                         \
  "Include <cxxtrace/config.h> instead of including <cxxtrace/config_impl.h> directly."
#endif

namespace cxxtrace {
template<class Storage, class Clock>
basic_config<Storage, Clock>::basic_config(Storage& storage,
                                           Clock& clock) noexcept
  : clock_reference{ clock }
  , storage_reference{ storage }
{}

template<class Storage, class Clock>
auto
basic_config<Storage, Clock>::clock() noexcept -> Clock&
{
  return this->clock_reference;
}

template<class Storage, class Clock>
auto
basic_config<Storage, Clock>::storage() noexcept -> Storage&
{
  return this->storage_reference;
}
}

#endif
