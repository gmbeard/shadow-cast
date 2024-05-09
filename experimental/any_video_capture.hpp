#ifndef SHADOW_CAST_ANY_VIDEO_CAPTURE_HPP_INCLUDED
#define SHADOW_CAST_ANY_VIDEO_CAPTURE_HPP_INCLUDED

#include "capture_sink.hpp"
#include "capture_source.hpp"
#include "sticky_cancel_timer.hpp"
#include "video_capture.hpp"
#include <memory>

namespace sc
{

namespace detail
{

struct AnyVideoCaptureCallback;

template <typename Allocator,
          CaptureSink Sink,
          CaptureSource<typename Sink::input_type> Source>
struct AnyVideoCaptureState
{
    using AllocatorType = Allocator;
    using SinkType = Sink;
    using SourceType = Source;

    Allocator allocator;
    Sink sink;
    Source source;
    StickyCancelTimer timer;
    FrameTime frame_time;
};

template <typename Allocator, typename Completion>
struct AnyVideoCaptureCallbackState
{
    using AllocatorType = Allocator;
    using CompletionType = Completion;

    Allocator allocator;
    Completion completion;
};

struct AnyVideoCaptureVtable
{
    auto (*destroy)(void*) -> void;
    auto (*cancel)(void*) noexcept -> void;
    auto (*run)(void*, AnyVideoCaptureCallback&&) -> void;
};

struct AnyVideoCaptureCallbackVtable
{
    auto (*destroy)(void*) -> void;
    auto (*invoke)(void*, exios::Result<std::error_code>&&) -> void;
};

template <typename Allocator,
          CaptureSink Sink,
          CaptureSource<typename Sink::input_type> Source>
auto any_video_capture_vtable() noexcept -> AnyVideoCaptureVtable const&
{
    using State = AnyVideoCaptureState<std::decay_t<Allocator>,
                                       std::decay_t<Sink>,
                                       std::decay_t<Source>>;

    static AnyVideoCaptureVtable const vtable {
        [](void* self) -> void {
            auto& state = *reinterpret_cast<State*>(self);

            auto alloc = typename std::allocator_traits<
                Allocator>::template rebind_alloc<State> { state.allocator };

            state.~State();
            alloc.deallocate(&state, 1);
        },
        [](void* self) noexcept -> void {
            reinterpret_cast<State*>(self)->timer.cancel();
        },
        [](void* self, AnyVideoCaptureCallback&& completion) -> void {
            auto& state = *reinterpret_cast<State*>(self);

            VideoCaptureLoopOperation(
                state.sink,
                state.source,
                state.timer,
                state.frame_time,
                exios::use_allocator(std::move(completion), state.allocator))
                .initiate();
        }
    };

    return vtable;
}

template <typename Allocator, typename Completion>
auto any_video_capture_callback_vtable() noexcept
    -> AnyVideoCaptureCallbackVtable const&
{
    using State = AnyVideoCaptureCallbackState<std::decay_t<Allocator>,
                                               std::decay_t<Completion>>;

    static AnyVideoCaptureCallbackVtable const vtable {
        [](void* self) -> void {
            auto& state = *reinterpret_cast<State*>(self);

            auto alloc = typename std::allocator_traits<
                Allocator>::template rebind_alloc<State> { state.allocator };

            state.~State();
            alloc.deallocate(&state, 1);
        },
        [](void* self, exios::Result<std::error_code>&& result) -> void {
            auto& state = *reinterpret_cast<State*>(self);

            auto alloc = typename std::allocator_traits<
                Allocator>::template rebind_alloc<State> { state.allocator };

            auto completion = std::move(state.completion);
            state.~State();
            alloc.deallocate(&state, 1);

            std::move(completion)(std::move(result));
        }
    };

    return vtable;
}

template <typename State, typename Allocator, typename... Ts>
auto make_erased_state(Allocator const& allocator, Ts&&... args)
{
    auto alloc = typename std::allocator_traits<
        Allocator>::template rebind_alloc<State> { allocator };

    State* ptr = alloc.allocate(1);
    try {
        new (static_cast<void*>(ptr))
            State { allocator, std::forward<Ts>(args)... };
    }
    catch (...) {
        alloc.deallocate(ptr, 1);
    }

    return ptr;
}

struct AnyVideoCaptureCallback
{

    template <typename Allocator, typename Completion>
    AnyVideoCaptureCallback(Allocator const& alloc, Completion&& completion)
        : vtable_ { &any_video_capture_callback_vtable<Allocator,
                                                       Completion>() }
        , erased_state_ { make_erased_state<
              AnyVideoCaptureCallbackState<Allocator, Completion>>(
              alloc, std::move(completion)) }
    {
    }

    AnyVideoCaptureCallback(AnyVideoCaptureCallback&& other) noexcept
        : vtable_ { other.vtable_ }
        , erased_state_ { std::exchange(other.erased_state_, nullptr) }
    {
    }

    ~AnyVideoCaptureCallback()
    {
        SC_EXPECT(vtable_);
        if (erased_state_)
            vtable_->destroy(erased_state_);
    }

    auto operator=(AnyVideoCaptureCallback&& other) noexcept
        -> AnyVideoCaptureCallback&
    {
        using std::swap;

        auto tmp { std::move(other) };
        swap(vtable_, tmp.vtable_);
        swap(erased_state_, tmp.erased_state_);

        return *this;
    }

    auto operator()(exios::Result<std::error_code> result) -> void
    {
        SC_EXPECT(erased_state_);
        SC_EXPECT(vtable_);

        vtable_->invoke(erased_state_, std::move(result));
        erased_state_ = nullptr;
    }

private:
    AnyVideoCaptureCallbackVtable const* vtable_;
    void* erased_state_;
};

} // namespace detail

struct AnyVideoCapture
{
    template <typename Allocator,
              CaptureSink Sink,
              CaptureSource<typename Sink::input_type> Source>
    explicit AnyVideoCapture(exios::Context context,
                             Allocator alloc,
                             Sink&& sink,
                             Source&& source,
                             FrameTime const& frame_time)
        : vtable_ { &detail::
                        any_video_capture_vtable<Allocator, Sink, Source>() }
        , erased_state_ { detail::make_erased_state<
              detail::AnyVideoCaptureState<Allocator, Sink, Source>>(
              alloc,
              std::move(sink),
              std::move(source),
              StickyCancelTimer { context },
              frame_time) }
    {
    }

    AnyVideoCapture(AnyVideoCapture&& other) noexcept
        : vtable_ { other.vtable_ }
        , erased_state_ { std::exchange(other.erased_state_, nullptr) }
    {
    }

    ~AnyVideoCapture()
    {
        if (erased_state_)
            vtable_->destroy(erased_state_);
    }

    auto operator=(AnyVideoCapture&& other) noexcept -> AnyVideoCapture&
    {
        using std::swap;

        auto tmp { std::move(other) };
        swap(vtable_, tmp.vtable_);
        swap(erased_state_, tmp.erased_state_);

        return *this;
    }

    auto cancel() noexcept -> void
    {
        SC_EXPECT(erased_state_);
        SC_EXPECT(vtable_);
        vtable_->cancel(erased_state_);
    }

    template <typename Completion, typename Allocator>
    auto run(Completion&& completion, Allocator const& alloc) -> void
    {
        SC_EXPECT(erased_state_);
        SC_EXPECT(vtable_);
        vtable_->run(
            erased_state_,
            detail::AnyVideoCaptureCallback(alloc, std::move(completion)));
    }

private:
    detail::AnyVideoCaptureVtable const* vtable_;
    void* erased_state_;
};

} // namespace sc

#endif // SHADOW_CAST_ANY_VIDEO_CAPTURE_HPP_INCLUDED
