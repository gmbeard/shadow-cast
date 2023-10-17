#include "./testing.hpp"
#include "nvidia.hpp"
#include "utils.hpp"
#include <cstdlib>

auto constexpr kNvFBCKeyEnvVar = "SHADOW_CAST_NVFBC_KEY";

namespace utils
{

[[nodiscard]] auto load_nvfbc() -> sc::NvFBC
{
    static auto fbc_lib = sc::load_nvfbc();
    return fbc_lib.NvFBCCreateInstance();
}

[[nodiscard]] auto create_nvfbc_session(sc::NvFBC instance)
    -> sc::NvFBCSessionHandlePtr
{
    std::string_view nvfb_key;
    if (auto const* env_var = std::getenv(kNvFBCKeyEnvVar); env_var) {
        nvfb_key = env_var;
    }
    auto const nvfbc_key_result = sc::base64_decode(nvfb_key);
    EXPECT(nvfbc_key_result);

    NVFBC_SESSION_HANDLE fbc_handle;
    NVFBC_CREATE_HANDLE_PARAMS create_params {};
    create_params.dwVersion = NVFBC_CREATE_HANDLE_PARAMS_VER;
    create_params.privateData = (*nvfbc_key_result).data();
    create_params.privateDataSize = (*nvfbc_key_result).size();
    EXPECT(instance.nvFBCCreateHandle(&fbc_handle, &create_params) ==
           NVFBC_SUCCESS);
    return sc::NvFBCSessionHandlePtr { fbc_handle, instance };
}

auto create_capture_session(CUcontext cuda_ctx = nullptr)
{
    auto nvfbc = utils::load_nvfbc();
    auto nvfbc_handle = utils::create_nvfbc_session(nvfbc);

    NVFBC_CREATE_CAPTURE_SESSION_PARAMS create_capture_params {};
    create_capture_params.dwVersion = NVFBC_CREATE_CAPTURE_SESSION_PARAMS_VER;
    create_capture_params.eCaptureType = NVFBC_CAPTURE_SHARED_CUDA;
    create_capture_params.bWithCursor = NVFBC_FALSE;
    create_capture_params.eTrackingType = NVFBC_TRACKING_SCREEN;
    create_capture_params.dwSamplingRateMs = 1000u / 60;
    create_capture_params.bAllowDirectCapture = NVFBC_FALSE;
    create_capture_params.bPushModel = NVFBC_FALSE;

    EXPECT(nvfbc.nvFBCCreateCaptureSession(
               nvfbc_handle.get(), &create_capture_params) == NVFBC_SUCCESS);

    sc::NvFBCCaptureSessionHandlePtr ptr { nvfbc_handle.get(), nvfbc };

    if (cuda_ctx) {
        NVFBC_TOCUDA_SETUP_PARAMS setup_params {};
        setup_params.dwVersion = NVFBC_TOCUDA_SETUP_PARAMS_VER;
        setup_params.eBufferFormat = NVFBC_BUFFER_FORMAT_BGRA;

        if (auto const result =
                nvfbc.nvFBCToCudaSetUp(nvfbc_handle.get(), &setup_params);
            result != NVFBC_SUCCESS)
            throw sc::NvFBCError { nvfbc, nvfbc_handle.get() };
    }
}

} // namespace utils

auto should_have_nvfbc_key() -> void
{
    EXPECT(std::getenv(kNvFBCKeyEnvVar) != nullptr);
}

auto should_initialize_session() -> void
{
    static_cast<void>(utils::create_nvfbc_session(utils::load_nvfbc()));
}

auto should_create_capture_session() -> void
{
    utils::create_capture_session();
}

auto should_load_cuda_library() -> void { static_cast<void>(sc::load_cuda()); }

auto should_initialize_cuda() -> void
{
    auto cuda = sc::load_cuda();
    auto const init_result = cuda.cuInit(0);
    if (init_result != CUDA_SUCCESS)
        throw sc::NvCudaError { cuda, init_result };
}

auto should_create_cuda_context() -> void
{
    auto cuda = sc::load_cuda();
    auto const init_result = cuda.cuInit(0);
    EXPECT(init_result == CUDA_SUCCESS);

    auto ctx = sc::create_cuda_context(cuda);
    EXPECT(ctx);

    utils::create_capture_session(ctx.get());
}

auto main() -> int
{
    /* Run tests...
     */
    return testing::run({ TEST(should_initialize_session),
                          TEST(should_have_nvfbc_key),
                          TEST(should_create_capture_session),
                          TEST(should_load_cuda_library),
                          TEST(should_initialize_cuda),
                          TEST(should_create_cuda_context) });
}
