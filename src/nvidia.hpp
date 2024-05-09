#ifndef SHADOW_CAST_NVIDIA_HPP_INCLUDED
#define SHADOW_CAST_NVIDIA_HPP_INCLUDED

#include "./nvidia/NvFBC.h"
#include "./nvidia/cuda.hpp"
#include "./utils.hpp"
#include "error.hpp"
#include <array>
#include <cinttypes>
#include <memory>
#include <optional>
#include <stdexcept>

namespace sc
{

[[maybe_unused]] auto constexpr kNvFBCKeyEnvVar = "SHADOW_CAST_NVFBC_KEY";

using NvFBC = NVFBC_API_FUNCTION_LIST;
struct NvFBCHandleDeleter
{
    using pointer = NonPointer<NVFBC_SESSION_HANDLE>;
    NvFBCHandleDeleter(NvFBC fbc) noexcept;

protected:
    NvFBC fbc_;
};

struct NvFBCError : std::runtime_error
{
    explicit NvFBCError(NvFBC, NVFBC_SESSION_HANDLE);
};

struct NvFBCSessionHandleDeleter : NvFBCHandleDeleter
{
    using NvFBCHandleDeleter::NvFBCHandleDeleter;
    auto operator()(pointer) noexcept -> void;
};

struct NvFBCCaptureSessionHandleDeleter : NvFBCHandleDeleter
{
    using NvFBCHandleDeleter::NvFBCHandleDeleter;
    auto operator()(pointer) noexcept -> void;
};

using NvFBCSessionHandlePtr =
    std::unique_ptr<NVFBC_SESSION_HANDLE, NvFBCSessionHandleDeleter>;

using NvFBCCaptureSessionHandlePtr =
    std::unique_ptr<NVFBC_SESSION_HANDLE, NvFBCCaptureSessionHandleDeleter>;

struct NvFBCLib
{
    friend auto load_nvfbc() -> NvFBCLib;
    auto NvFBCCreateInstance() -> NvFBC;

private:
    PNVFBCCREATEINSTANCE NvFBCCreateInstance_;
};

auto load_nvfbc() -> NvFBCLib;

[[nodiscard]] auto create_nvfbc_session(sc::NvFBC instance)
    -> NvFBCSessionHandlePtr;

auto create_nvfbc_capture_session(NVFBC_SESSION_HANDLE nvfbc_handle,
                                  NvFBC nvfbc,
                                  FrameTime const&,
                                  std::optional<NVFBC_SIZE> = std::nullopt)
    -> void;
auto destroy_nvfbc_capture_session(NVFBC_SESSION_HANDLE nvfbc_handle,
                                   NvFBC nvfbc) -> void;
} // namespace sc
#endif // SHADOW_CAST_NVIDIA_HPP_INCLUDED
