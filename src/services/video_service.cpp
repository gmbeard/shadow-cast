#include "services/video_service.hpp"

namespace sc
{

VideoService::VideoService(
    NvFBC nvfbc,
    BorrowedPtr<std::remove_pointer_t<CUcontext>> nvcuda_ctx,
    NVFBC_SESSION_HANDLE nvfbc_session) noexcept
    : nvfbc_ { nvfbc }
    , nvcuda_ctx_ { nvcuda_ctx }
    , nvfbc_session_ { nvfbc_session }
{
}

auto VideoService::on_init(ReadinessRegister reg) -> void
{
    reg(FrameTimeRatio(1), &dispatch_frame);
}

auto dispatch_frame(Service& svc) -> void
{
    auto& self = static_cast<VideoService&>(svc);
    CUdeviceptr cu_device_ptr {};

    NVFBC_FRAME_GRAB_INFO frame_info {};

    NVFBC_TOCUDA_GRAB_FRAME_PARAMS grab_params {};
    grab_params.dwVersion = NVFBC_TOCUDA_GRAB_FRAME_PARAMS_VER;
    grab_params.dwFlags = NVFBC_TOCUDA_GRAB_FLAGS_NOWAIT;
    grab_params.pFrameGrabInfo = &frame_info;
    grab_params.pCUDADeviceBuffer = &cu_device_ptr;
    grab_params.dwTimeoutMs = 0;

    if (auto const status =
            self.nvfbc_.nvFBCToCudaGrabFrame(self.nvfbc_session_, &grab_params);
        status != NVFBC_SUCCESS)
        throw NvFBCError { self.nvfbc_, self.nvfbc_session_ };

    if (self.receiver_)
        (*self.receiver_)(cu_device_ptr, frame_info);
}

} // namespace sc
