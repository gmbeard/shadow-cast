#ifndef SHADOW_CAST_AV_FORMAT_HPP_INCLUDED
#define SHADOW_CAST_AV_FORMAT_HPP_INCLUDED

#include <memory>

extern "C" {
#include <libavformat/avformat.h>
}

namespace sc
{

struct FormatContextDeleter
{
    auto operator()(AVFormatContext* ptr) noexcept -> void;
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;

} // namespace sc

#endif // SHADOW_CAST_AV_FORMAT_HPP_INCLUDED
