#include "./base64.hpp"
#include "config.hpp"

namespace
{

template <typename T>
auto begin_by_byte_order(std::span<T> s) noexcept
requires(sc::kByteOrder == sc::ByteOrder::little_endian)
{
    return s.rbegin();
}

template <typename T>
auto begin_by_byte_order(std::span<T> s) noexcept
requires(sc::kByteOrder == sc::ByteOrder::big_endian)
{
    return s.begin();
}

auto constexpr base64_alphabet() noexcept -> std::array<char, 64>
{
    std::array<char, 64> alphabet;
    auto pos = alphabet.begin();

    for (auto c = 'A'; c <= 'Z'; ++c)
        *pos++ = c;
    for (auto c = 'a'; c <= 'z'; ++c)
        *pos++ = c;
    for (auto c = '0'; c <= '9'; ++c)
        *pos++ = c;
    *pos++ = '+';
    *pos++ = '/';

    return alphabet;
}

auto constexpr kBase64Alphabet = base64_alphabet();

auto base64_decode_chunk(std::string_view input)
    -> sc::Result<std::uint32_t, sc::Base64DecodeErrorCode>
{
    using std::distance;

    std::uint32_t val {};
    int shift = 3;
    int n = 0;
    for (auto const& c : input) {
        if (c == '=')
            break;

        auto const pos =
            std::find(begin(kBase64Alphabet), end(kBase64Alphabet), c);
        if (pos == end(kBase64Alphabet))
            return sc::Base64DecodeErrorCode::invalid_token;

        auto const i = distance(begin(kBase64Alphabet), pos);
        val |= (i & 0x3f) << ((6 * shift--) + 2);
        ++n;
    }

    switch (n) {
    case 1:
        return sc::Base64DecodeErrorCode::not_enough_tokens;
    case 2:
        return (val << 6) | (1 & 0x03);
    case 3:
        return (val << 6) | (2 & 0x03);
    default:
        return (val << 6) | (3 & 0x03);
    }
}

} // namespace

namespace sc
{
auto base64_decode(std::string_view encoded)
    -> Result<std::vector<std::uint8_t>, Base64DecodeErrorCode>
{
    using std::advance;
    using std::begin;
    using std::copy_n;
    using std::distance;
    using std::end;

    std::vector<std::uint8_t> result;
    std::array<char, 4> decode_chunk {};

    auto input_pos = begin(encoded);

    while (input_pos != end(encoded)) {
        auto const num_to_read = std::min(
            decode_chunk.size(),
            static_cast<std::size_t>(distance(input_pos, end(encoded))));
        auto out_pos = copy_n(input_pos, num_to_read, begin(decode_chunk));

        while (out_pos != end(decode_chunk))
            *out_pos++ = '=';

        advance(input_pos, num_to_read);
        auto decode_result = base64_decode_chunk(
            std::string_view { decode_chunk.data(), decode_chunk.size() });
        if (!decode_result)
            return sc::get_error(decode_result);

        auto val = *decode_result;
        auto const num_bytes = val & 0x03;
        val &= ~0x03;

        std::span const bytes { reinterpret_cast<std::uint8_t const*>(&val),
                                sizeof(val) };

        auto begin = begin_by_byte_order(bytes);

        std::copy_n(begin, num_bytes, std::back_inserter(result));
    }

    return result;
}
} // namespace sc
