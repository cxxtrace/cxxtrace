#ifndef CXXTRACE_CONFIG_H
#define CXXTRACE_CONFIG_H

namespace cxxtrace {
class config_base
{
public:
  static auto clock() -> void = delete;
  static auto storage() -> void = delete;
};

template<class Storage, class Clock>
class basic_config : public config_base
{
public:
  explicit basic_config(Storage&, Clock&) noexcept;

  auto clock() noexcept -> Clock&;
  auto storage() noexcept -> Storage&;

private:
  Clock& clock_reference;
  Storage& storage_reference;
};

template<class Storage, class Clock>
basic_config(Storage&, Clock&)->basic_config<Storage, Clock>;
}

#include <cxxtrace/config_impl.h>

#endif
