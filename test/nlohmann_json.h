#ifndef CXXTRACE_NLOHMANN_JSON
#define CXXTRACE_NLOHMANN_JSON

#include <nlohmann/json.hpp>

// Reduce compilation times by compiling some class templates in their own
// translation unit (nlohmann_json.cpp).
extern template class nlohmann::basic_json<>;
extern template class nlohmann::detail::iter_impl<nlohmann::json>;
extern template class nlohmann::detail::serializer<nlohmann::json>;

#endif
