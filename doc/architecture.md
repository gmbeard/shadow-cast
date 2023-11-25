## Architecture
In order to effectively record media output, *Shadow Cast* must capture audio and video data in a stable, timely manner. It must also be able to detect external events, such as when the user wants to stop capturing, or when exceptions occur. To achieve this, it runs an event loop at its core. The event loop synchronizes the capture of audio and video data to the required frame rate, while simultaneously listening for other events such as the `SIGINT` signal.

This document aims to explain how the event loop is defined, how it operates, and how it notifies the other parts of the software to process workloads.

### 1. Contexts and Services

A `Context` instance is effectively an event loop. Invoking `Context::run()` will run the event loop and block the calling thread until the context instance is stopped, either by a signal, by an application exception, or by calling `Context::request_stop()` from somewhere else in the call stack, or from another thread.

Contexts work in conjunction with Services (see [*2. Defining and Using Services*](#2-defining-and-using-services)). Services are very much required to drive a context's event loop. Without at least one service, the event loop will effectively do nothing and exit immediately.

Before entering the event loop, `Context::run()` will first ask its services to register their notification handlers. It does this by calling `Service::init(ReadinessRegister)` for each of its services. Within `Service::init()`, the service can tell the event loop how and when it wishes to be notified by providing...

- A file handle to watch for readable events
- A timer
- One or more of both.

In addition, the service also provides a *dispatch* function to call when any of these events occur.

#### Timer Notifications
Timers are specified as a ratio of the current framerate. The framerate is given as a runtime argument to a context. To give some examples; A service that wishes to be notified every frame will supply a ratio of `1/1` (`FrameTimeRatio(1, 1)`). This is typically used for video frames; We want to render video frames at the given framerate. An audio service, however, may need to process audio data more frequently than the video frame rate, say twice for every frame, so the ratio would be `1/2` (`FrameTimeRatio(1, 2)`). For examples of this, see the `VideoService` and `AudioService` definitions.

#### File Handle Notifications
A service may wish to be notified when an event occurs at some indeterminate point in time, such as if the process receives a `SIGINT` signal to stop capturing. The service can do this by supplying a file descriptor in the `Service::init(ReadinessRegister)` call. In the case of file handle notification, it is the service's responsibility to ensure the provided file handle is notifiable when used with the `ppoll()` system call. For an example, see the `SignalService` definition.

### 2. Defining and Using Services
Services are where the main work of the capture session happens. They have the following responsibilities...

- Initialise their state and register their notification interest with the event loop, additionally supplying a notification callback (`Service::on_init()`)
- Handle the notification when they are signalled by the event loop
- Uninitialized their state when the event loop terminates (`Service::on_uninit()`)

Each service must derive from the abstract base type `Service`, and provide, at a minimum, an implementation of `on_init()`.

To illustrate, here's an example of a hypothetical definition of `MyService`...

```c++
struct MyService final : Service
{
    ...

protected:
    /* on_init is required...
     */
    auto on_init(ReadinessRegister reg) -> void override
    {
        /* Initialize the service's state and other parameters...
         */

        ...

        /* This informs the event loop that we want to
         * be notified on every frame, and to call our
         * static `dispatch()` function when doing so...
         */
        reg(FrameTimeRatio(1, 1), &MyService::dispatch);
    }

    /* Not required, but useful if the service needs to do
     * some clean up tasks...
     */
    auto on_uninit() noexcept -> void override
    {
        /* Uninitialize any state here. The service should
         * ensure that `on_init()` can be safely called
         * again at some point in the future.
         *
         * NOTE: This function is marked `noexcept` so
         * that services can be reliably torn down in
         * the event of an execption elsewhere in the
         * application.
         */
    }

private:
    static auto dispatch(Service& svc) -> void
    {
        /* Here's where we handle a frame timer event.
         * We've asked to be called every frame, so this
         * could be whatever the user of the application
         * has specified at runtime - 30fps, 45fps, etc.
         */

         /*
         * The `svc` parameter is guaranteed to be
         * an instance of `MyService`, so can be
         * safely casted as such...
         */
         MyService& self = static_cast<MyService&>(svc);

        ...
    }
}
```

#### Adding Services
Services must be added to an instance of `Context` in order to be of any use. This is done via `Context::services().add<ServiceType>()` or `Context::services().add_from_factory<ServiceType>(...)`. The former can be used when `ServiceType` is movable and default constructible. The latter can be used when `ServiceType` is neither movable nor default constructible.

Example...

```c++
/* MyService is default constructible...
 */
sc::Context ctx { frame_rate };
ctx.services().add<MyService>();
ctx.run();
```

```c++
/* MyService is not movable. It may
 * contain non movable members, such
 * as a `std::mutex`...
 */
sc::Context ctx { frame_rate };
ctx.services().add_from_factory<MyService>([] {
    return std::make_unique<MyService>( ... );
});
ctx.run();
```

**IMPORTANT NOTE: Services must not be added anywhere within the call to** `Context::run()`**. Doing so is currently undefined behaviour.**

#### Retrieving Services
Services can be retrieved from a context using `Context::services().use_if<ServiceType>()`. This will return a `ServiceType*`, which _could_ be a `nullptr` if the service hasn't already been added to the context.
