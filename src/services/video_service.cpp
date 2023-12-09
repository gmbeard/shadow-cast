#include "services/video_service.hpp"
#include "nvidia/NvFBC.h"
#include "utils/contracts.hpp"

#ifdef SHADOW_CAST_ENABLE_METRICS
#include "utils/elapsed.hpp"
#endif

namespace
{
constexpr std::size_t kNsPerMs = 1'000'000;
}

namespace sc
{

VideoService::VideoService(
    NvFBC nvfbc,
    BorrowedPtr<std::remove_pointer_t<CUcontext>> nvcuda_ctx,
    NVFBC_SESSION_HANDLE nvfbc_session
        SC_METRICS_PARAM_DEFINE(MetricsService*, metrics_service)) noexcept
    : nvfbc_ { nvfbc }
    , nvcuda_ctx_ { nvcuda_ctx }
    , nvfbc_session_ { nvfbc_session } // clang-format off
    SC_METRICS_MEMBER_USE(metrics_service, metrics_service_)
    SC_METRICS_MEMBER_USE(0, metrics_start_time_)
// clang-format on
{
#ifdef SHADOW_CAST_ENABLE_METRICS
    SC_EXPECT(metrics_service_);
#endif
}

auto VideoService::on_init(ReadinessRegister reg) -> void
{
    frame_time_ = reg.frame_time();
    reg(FrameTimeRatio(1), &dispatch_frame);

#ifdef SHADOW_CAST_ENABLE_METRICS
    metrics_start_time_ = global_elapsed.nanosecond_value();
#endif
}

auto dispatch_frame(Service& svc) -> void
{
    auto& self = static_cast<VideoService&>(svc);

#ifdef SHADOW_CAST_ENABLE_METRICS
    static std::size_t frame_num = 0;
    self.frame_timer_.reset();
    auto const frame_timestamp =
        global_elapsed.nanosecond_value() - self.metrics_start_time_;
#endif

    CUdeviceptr cu_device_ptr {};

    NVFBC_FRAME_GRAB_INFO frame_info {};

    NVFBC_TOCUDA_GRAB_FRAME_PARAMS grab_params {};
    grab_params.dwVersion = NVFBC_TOCUDA_GRAB_FRAME_PARAMS_VER;
    grab_params.dwFlags = NVFBC_TOCUDA_GRAB_FLAGS_NOWAIT_IF_NEW_FRAME_READY;
    grab_params.pFrameGrabInfo = &frame_info;
    grab_params.pCUDADeviceBuffer = &cu_device_ptr;
    grab_params.dwTimeoutMs = self.frame_time_ / kNsPerMs;

    if (auto const status =
            self.nvfbc_.nvFBCToCudaGrabFrame(self.nvfbc_session_, &grab_params);
        status != NVFBC_SUCCESS)
        throw NvFBCError { self.nvfbc_, self.nvfbc_session_ };

    if (self.receiver_)
        (*self.receiver_)(cu_device_ptr, frame_info, self.frame_time_);

#ifdef SHADOW_CAST_ENABLE_METRICS
    self.metrics_service_->post_time_metric(
        { .category = 1,
          .id = ++frame_num,
          .timestamp_ns = frame_timestamp,
          .nanoseconds = self.frame_timer_.nanosecond_value(),
          .frame_size = 1,
          .frame_count = 1 });
#endif
}

} // namespace sc
