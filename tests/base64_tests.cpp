#include "./testing.hpp"
#include "utils.hpp"
#include <algorithm>
#include <array>
#include <cinttypes>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

auto should_decode()
{
    std::string_view const input = "bGlnaHQgd29yay4=";
    std::string_view const expected_output = "light work.";

    auto const decode_result = sc::base64_decode(input);
    EXPECT(decode_result);

    for (auto const& b : *decode_result)
        std::cerr << int(b) << ',';
    std::cerr << '\n';

    std::string_view const output { reinterpret_cast<char const*>(
                                        sc::get_value(decode_result).data()),
                                    sc::get_value(decode_result).size() };
    std::cerr << output << '\n';
    EXPECT(output == expected_output);
}

auto should_fail_to_decode_when_not_enough_tokens()
{
    std::string_view const input = "b";

    auto const decode_result = sc::base64_decode(input);
    EXPECT(!decode_result);
    EXPECT(sc::get_error(decode_result) ==
           sc::Base64DecodeErrorCode::not_enough_tokens);
}

auto should_fail_to_decode_when_input_contains_invalid_token()
{
    std::string_view const input = "QQ%=";

    auto const decode_result = sc::base64_decode(input);
    EXPECT(!decode_result);
    EXPECT(sc::get_error(decode_result) ==
           sc::Base64DecodeErrorCode::invalid_token);
}

auto main() -> int
{
    /* Tests...
     */
    return testing::run(
        { TEST(should_decode),
          TEST(should_fail_to_decode_when_not_enough_tokens),
          TEST(should_fail_to_decode_when_input_contains_invalid_token) });
}
