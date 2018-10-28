#ifndef CXXTRACE_CONFIG_H
#define CXXTRACE_CONFIG_H

namespace cxxtrace {
class unbounded_storage;

class config_base
{
public:
  static auto storage() -> void = delete;
};

class default_config : public config_base
{
public:
  explicit default_config(unbounded_storage&) noexcept;

  auto storage() noexcept -> unbounded_storage&;

private:
  unbounded_storage& storage_reference;
};
}

#endif
