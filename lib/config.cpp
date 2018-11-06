#include <cxxtrace/config.h>
#include <cxxtrace/unbounded_storage.h>

namespace cxxtrace {
default_config::default_config(unbounded_storage& storage) noexcept
  : storage_reference{ storage }
{}

auto
default_config::storage() noexcept -> unbounded_storage&
{
  return this->storage_reference;
}
}
