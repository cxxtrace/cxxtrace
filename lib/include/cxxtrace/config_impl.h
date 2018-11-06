#ifndef CXXTRACE_CONFIG_IMPL_H
#define CXXTRACE_CONFIG_IMPL_H

#if !defined(CXXTRACE_CONFIG_H)
#error                                                                         \
  "Include <cxxtrace/config.h> instead of including <cxxtrace/config_impl.h> directly."
#endif

namespace cxxtrace {
template<class Storage>
default_config<Storage>::default_config(Storage& storage) noexcept
  : storage_reference{ storage }
{}

template<class Storage>
auto
default_config<Storage>::storage() noexcept -> Storage&
{
  return this->storage_reference;
}
}

#endif
