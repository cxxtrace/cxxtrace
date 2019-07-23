#include "nlohmann_json.h"
#include <nlohmann/json.hpp>

template class nlohmann::basic_json<>;
template class nlohmann::detail::iter_impl<nlohmann::json>;
template class nlohmann::detail::serializer<nlohmann::json>;
