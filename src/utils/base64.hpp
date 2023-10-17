#ifndef SHADOW_CAST_UTILS_BASE64_HPP_INCLUDED
#define SHADOW_CAST_UTILS_BASE64_HPP_INCLUDED

#include "result.hpp"
#include <algorithm>
#include <array>
#include <cinttypes>
#include <span>
#include <string_view>
#include <vector>

namespace sc
{

enum struct Base64DecodeErrorCode
{
    invalid_token,
    not_enough_tokens
};

[[nodiscard]] auto base64_decode(std::string_view encoded)
    -> Result<std::vector<std::uint8_t>, Base64DecodeErrorCode>;
} // namespace sc

#endif // SHADOW_CAST_UTILS_BASE64_HPP_INCLUDED
