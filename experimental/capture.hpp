#ifndef SHADOW_CAST_CAPTURE_HPP_INCLUDED
#define SHADOW_CAST_CAPTURE_HPP_INCLUDED

#include "exios/exios.hpp"
#include "frame_capture_loop.hpp"
#include "utils/contracts.hpp"
#include <memory>

namespace sc
{

namespace detail
{

struct CaptureCallback;

template <typename Allocator, typename Source, typename Sink>
requires(IntervalBasedSource<Source> || EventTriggeredSource<Source>)
struct CaptureState
{
    using AllocatorType = std::decay_t<Allocator>;
    using SourceType = std::decay_t<Source>;
    using SinkType = std::decay_t<Sink>;

    AllocatorType allocator;
    SourceType source;
    SinkType sink;
};

template <typename Allocator, typename Completion>
struct CaptureCallbackState
{
    using AllocatorType = std::decay_t<Allocator>;
    using CompletionType = std::decay_t<Completion>;

    AllocatorType allocator;
    CompletionType completion;
};

struct CaptureVtable
{
    auto (*destroy)(void*) -> void;
    auto (*cancel)(void*) noexcept -> void;
    auto (*run)(void*, CaptureCallback&&) -> void;
};

struct CaptureCallbackVtable
{
    auto (*destroy)(void*) -> void;
    auto (*invoke)(void*, exios::Result<std::error_code>&&) -> void;
};

template <typename Allocator, typename Source, typename Sink>
auto capture_vtable() noexcept -> CaptureVtable const&
requires(IntervalBasedSource<Source> || EventTriggeredSource<Source>)
{
    using State = CaptureState<Allocator, Source, Sink>;

    static CaptureVtable const vtable {
        [](void* self) -> void {
            auto& state = *reinterpret_cast<State*>(self);

            auto alloc = typename std::allocator_traits<
                Allocator>::template rebind_alloc<State> { state.allocator };

            state.~State();
            alloc.deallocate(&state, 1);
        },
        [](void* self) noexcept -> void {
            reinterpret_cast<State*>(self)->source.cancel();
        },
        [](void* self, CaptureCallback&& completion) -> void {
            auto& state = *reinterpret_cast<State*>(self);

            frame_capture_loop(
                state.source,
                state.sink,
                exios::use_allocator(std::move(completion), state.allocator));
        }
    };

    return vtable;
}

template <typename Allocator, typename Completion>
auto capture_callback_vtable() noexcept -> CaptureCallbackVtable const&
{
    using State = CaptureCallbackState<Allocator, Completion>;

    static CaptureCallbackVtable const vtable {
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

struct CaptureCallback
{
    template <typename Allocator, typename Completion>
    CaptureCallback(Allocator const& alloc, Completion&& completion)
        : vtable_ { &capture_callback_vtable<Allocator, Completion>() }
        , erased_state_ {
            make_erased_state<CaptureCallbackState<Allocator, Completion>>(
                alloc, std::move(completion))
        }
    {
    }

    CaptureCallback(CaptureCallback&& other) noexcept;

    ~CaptureCallback();

    auto operator=(CaptureCallback&& other) noexcept -> CaptureCallback&;

    auto operator()(exios::Result<std::error_code> result) -> void;

private:
    CaptureCallbackVtable const* vtable_;
    void* erased_state_;
};

} // namespace detail

struct Capture
{
    template <typename Source,
              typename Sink,
              typename Allocator = std::allocator<void>>
    explicit Capture(Source&& source,
                     Sink&& sink,
                     Allocator const& alloc = Allocator {})
    requires(IntervalBasedSource<Source> || EventTriggeredSource<Source>)
        : interface_ { std::make_unique<CaptureModel<std::decay_t<Source>,
                                                     std::decay_t<Sink>,
                                                     Allocator>>(
              std::move(source), std::move(sink), alloc) }
    {
    }

    auto cancel() noexcept -> void;

    template <typename Completion, typename Allocator = std::allocator<void>>
    auto run(Completion&& completion, Allocator const& alloc = Allocator {})
        -> void
    {
        SC_EXPECT(interface_);
        // cppcheck-suppress [nullPointerRedundantCheck]
        interface_->run(
            detail::CaptureCallback { alloc, std::move(completion) });
    }

private:
    struct CaptureInterface
    {
        virtual ~CaptureInterface();
        virtual auto cancel() noexcept -> void = 0;
        virtual auto run(detail::CaptureCallback&&) -> void = 0;
    };

    template <typename Source, typename Sink, typename Allocator>
    struct CaptureModel : CaptureInterface
    {
        explicit CaptureModel(Source&& source,
                              Sink&& sink,
                              Allocator const& alloc) noexcept
            : source_ { std::move(source) }
            , sink_ { std::move(sink) }
            , allocator_ { alloc }
        {
        }

        CaptureModel(CaptureModel const&) = delete;
        auto operator=(CaptureModel const&) -> CaptureModel& = delete;

        auto cancel() noexcept -> void override { source_.cancel(); }

        auto run(detail::CaptureCallback&& completion) -> void override
        {
            frame_capture_loop(
                source_,
                sink_,
                exios::use_allocator(std::move(completion), allocator_));
        }

    private:
        Source source_;
        Sink sink_;
        Allocator allocator_;
    };

    std::unique_ptr<CaptureInterface> interface_;
};

} // namespace sc
#endif // SHADOW_CAST_CAPTURE_HPP_INCLUDED
