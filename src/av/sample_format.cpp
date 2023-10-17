#include "av/sample_format.hpp"
#include <algorithm>

namespace sc
{
auto find_supported_formats(sc::BorrowedPtr<AVCodec const> codec)
    -> std::vector<SampleFormat>
{
    std::vector<SampleFormat> results;
    for (auto supported_fmt = codec->sample_fmts;
         supported_fmt && *supported_fmt >= 0;
         ++supported_fmt) {
        results.push_back(convert_from_libav_format(*supported_fmt));
    }

    std::sort(rbegin(results), rend(results));

    return results;
}
} // namespace sc
