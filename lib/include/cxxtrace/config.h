#ifndef CXXTRACE_CONFIG_H
#define CXXTRACE_CONFIG_H

namespace cxxtrace {
class config_base
{
public:
  static auto storage() -> void = delete;
};

template<class Storage>
class default_config : public config_base
{
public:
  explicit default_config(Storage&) noexcept;

  auto storage() noexcept -> Storage&;

private:
  Storage& storage_reference;
};

template<class Storage>
default_config(Storage&)->default_config<Storage>;
}

#include <cxxtrace/config_impl.h>

#endif
