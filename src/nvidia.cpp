#include "nvidia.hpp"
#include "nvidia/cuda.hpp"
#include "utils/base64.hpp"
#include "utils/cmd_line.hpp"
#include "utils/frame_time.hpp"
#include "utils/symbol.hpp"
#include <dlfcn.h>

namespace sc
{

NvFBCHandleDeleter::NvFBCHandleDeleter(NvFBC fbc) noexcept
    : fbc_ { fbc }
{
}

auto NvFBCCaptureSessionHandleDeleter::operator()(pointer handle) noexcept
    -> void
{
    NVFBC_DESTROY_CAPTURE_SESSION_PARAMS params {
        NVFBC_DESTROY_CAPTURE_SESSION_PARAMS_VER
    };
    fbc_.nvFBCDestroyCaptureSession(handle, &params);
}

auto NvFBCSessionHandleDeleter::operator()(pointer handle) noexcept -> void
{
    NVFBC_DESTROY_HANDLE_PARAMS params { NVFBC_DESTROY_HANDLE_PARAMS_VER };
    fbc_.nvFBCDestroyHandle(handle, &params);
}

auto load_nvfbc() -> NvFBCLib
{
    auto lib = dlopen("libnvidia-fbc.so.1", RTLD_LAZY);
    if (!lib)
        throw ModuleError { std::string { "Couldn't load libnvidia-fbc.so: " } +
                            dlerror() };

    NvFBCLib fbc;
    TRY_ATTACH_SYMBOL(&fbc.NvFBCCreateInstance_, "NvFBCCreateInstance", lib);

    return fbc;
}

auto NvFBCLib::NvFBCCreateInstance() -> NvFBC
{
    NvFBC instance {};
    if (auto r = NvFBCCreateInstance_(&instance); r != NVFBC_SUCCESS)
        throw ModuleError { "Failed to get NvFBC instance" };

    return instance;
}

NvFBCError::NvFBCError(NvFBC nvfbc, NVFBC_SESSION_HANDLE handle)
    : std::runtime_error { nvfbc.nvFBCGetLastErrorStr(handle) }
{
}

auto load_cuda() -> NvCuda
{
    auto lib = dlopen("libcuda.so.1", RTLD_LAZY);
    if (!lib)
        throw ModuleError { std::string { "Couldn't load libcuda.so: " } +
                            dlerror() };
    NvCuda cuda {};
    TRY_ATTACH_SYMBOL(&cuda.cuInit, "cuInit", lib);
    TRY_ATTACH_SYMBOL(&cuda.cuDeviceGetCount, "cuDeviceGetCount", lib);
    TRY_ATTACH_SYMBOL(&cuda.cuDeviceGet, "cuDeviceGet", lib);
    TRY_ATTACH_SYMBOL(&cuda.cuCtxCreate_v2, "cuCtxCreate_v2", lib);
    TRY_ATTACH_SYMBOL(&cuda.cuCtxDestroy_v2, "cuCtxDestroy_v2", lib);
    TRY_ATTACH_SYMBOL(&cuda.cuCtxPushCurrent_v2, "cuCtxPushCurrent_v2", lib);
    TRY_ATTACH_SYMBOL(&cuda.cuCtxPopCurrent_v2, "cuCtxPopCurrent_v2", lib);
    TRY_ATTACH_SYMBOL(&cuda.cuGetErrorString, "cuGetErrorString", lib);
    TRY_ATTACH_SYMBOL(&cuda.cuMemsetD8_v2, "cuMemsetD8_v2", lib);
    TRY_ATTACH_SYMBOL(&cuda.cuMemcpy2D_v2, "cuMemcpy2D_v2", lib);
    TRY_ATTACH_SYMBOL(
        &cuda.cuGraphicsEGLRegisterImage, "cuGraphicsEGLRegisterImage", lib);
    TRY_ATTACH_SYMBOL(&cuda.cuGraphicsResourceSetMapFlags,
                      "cuGraphicsResourceSetMapFlags",
                      lib);
    TRY_ATTACH_SYMBOL(
        &cuda.cuGraphicsMapResources, "cuGraphicsMapResources", lib);
    TRY_ATTACH_SYMBOL(
        &cuda.cuGraphicsUnmapResources, "cuGraphicsUnmapResources", lib);
    TRY_ATTACH_SYMBOL(&cuda.cuGraphicsUnregisterResource,
                      "cuGraphicsUnregisterResource",
                      lib);
    TRY_ATTACH_SYMBOL(&cuda.cuGraphicsSubResourceGetMappedArray,
                      "cuGraphicsSubResourceGetMappedArray",
                      lib);
    TRY_ATTACH_SYMBOL(
        &cuda.cuArrayGetDescriptor_v2, "cuArrayGetDescriptor_v2", lib);
    return cuda;
}

NvCudaError::NvCudaError(NvCuda cuda, CUresult res)
    : std::runtime_error { [cuda, res]() {
        char const* error_msg = "Unknown error";
        cuda.cuGetErrorString(res, &error_msg);
        return error_msg;
    }() }
{
}

CUcontextDeleter::CUcontextDeleter(NvCuda cuda) noexcept
    : cuda_ { cuda }
{
}

auto CUcontextDeleter::operator()(CUcontext ptr) noexcept -> void
{
    cuda_.cuCtxDestroy_v2(ptr);
}

auto create_cuda_context(NvCuda cuda) -> CUcontextPtr
{
    CUdevice dev;
    CUcontext ctx;
    if (auto const res = cuda.cuDeviceGet(&dev, 0); res != CUDA_SUCCESS)
        throw NvCudaError { cuda, res };

    if (auto const res = cuda.cuCtxCreate_v2(&ctx, CU_CTX_SCHED_AUTO, dev);
        res != CUDA_SUCCESS)
        throw NvCudaError { cuda, res };

    return CUcontextPtr { ctx, cuda };
}

auto create_nvfbc_session(sc::NvFBC instance) -> sc::NvFBCSessionHandlePtr
{
    std::string_view nvfb_key;
    if (auto const* env_var = std::getenv(kNvFBCKeyEnvVar); env_var) {
        nvfb_key = env_var;
    }
    auto const nvfbc_key_result = sc::base64_decode(nvfb_key);

    NVFBC_SESSION_HANDLE fbc_handle;
    NVFBC_CREATE_HANDLE_PARAMS create_params {};
    create_params.dwVersion = NVFBC_CREATE_HANDLE_PARAMS_VER;
    create_params.privateData = (*nvfbc_key_result).data();
    create_params.privateDataSize = (*nvfbc_key_result).size();
    if (instance.nvFBCCreateHandle(&fbc_handle, &create_params) !=
        NVFBC_SUCCESS)
        throw std::runtime_error { "Couldn't create NvFBC instance" };
    return sc::NvFBCSessionHandlePtr { fbc_handle, instance };
}

auto create_nvfbc_capture_session(
    NVFBC_SESSION_HANDLE nvfbc_handle,
    NvFBC nvfbc,
    FrameTime const& frame_time,
    std::optional<CaptureResolution> const& resolution) -> void
{
    NVFBC_CREATE_CAPTURE_SESSION_PARAMS create_capture_params {};
    create_capture_params.dwVersion = NVFBC_CREATE_CAPTURE_SESSION_PARAMS_VER;
    create_capture_params.eCaptureType = NVFBC_CAPTURE_SHARED_CUDA;
    create_capture_params.bWithCursor = NVFBC_TRUE;
    create_capture_params.eTrackingType = NVFBC_TRACKING_SCREEN;
    create_capture_params.dwSamplingRateMs = frame_time.value_in_milliseconds();
    create_capture_params.bAllowDirectCapture = NVFBC_FALSE;
    create_capture_params.bPushModel = NVFBC_FALSE;
    if (resolution) {
        create_capture_params.frameSize.w = resolution->width;
        create_capture_params.frameSize.h = resolution->height;
    }

    if (nvfbc.nvFBCCreateCaptureSession(nvfbc_handle, &create_capture_params) !=
        NVFBC_SUCCESS)
        throw NvFBCError { nvfbc, nvfbc_handle };

    NVFBC_TOCUDA_SETUP_PARAMS setup_params {};
    setup_params.dwVersion = NVFBC_TOCUDA_SETUP_PARAMS_VER;
    setup_params.eBufferFormat = NVFBC_BUFFER_FORMAT_BGRA;

    if (nvfbc.nvFBCToCudaSetUp(nvfbc_handle, &setup_params) != NVFBC_SUCCESS)
        throw sc::NvFBCError { nvfbc, nvfbc_handle };
}

auto destroy_nvfbc_capture_session(NVFBC_SESSION_HANDLE nvfbc_handle,
                                   NvFBC nvfbc) -> void
{
    NVFBC_DESTROY_CAPTURE_SESSION_PARAMS destroy_capture_params {};
    destroy_capture_params.dwVersion = NVFBC_DESTROY_CAPTURE_SESSION_PARAMS_VER;

    if (nvfbc.nvFBCDestroyCaptureSession(
            nvfbc_handle, &destroy_capture_params) != NVFBC_SUCCESS)
        throw NvFBCError { nvfbc, nvfbc_handle };
}
} // namespace sc
