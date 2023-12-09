### Q. Help, I'm getting the following error

```
ERROR: Couldn't create NvFBC instance
```

#### A.
*Shadow Cast* uses the *NvFBC* facility to provide efficient, low-latency framebuffer capture on X11. By default, NVIDIA disables this on most (if not all) of its consumer-level GPUs. However, there are two ways around this restriction...

- You can find a utility to patch your NVIDIA drivers in the [keylase/nvidia-patch](https://github.com/keylase/nvidia-patch) GitHub repo.
- You can obtain a "key" to unlock this feature at runtime. The key can be set at runtime via the `SHADOW_CAST_NVFBC_KEY=<BASE64 ENCODED KEY>` environment variable. I use this method but I'm not sure how "official" it is, so no keys are provided in this repo. Feel free to message me about this.

### Q. I'm capturing gameplay footage on X11 and my game is "laggy"

#### A.
In some games, the NVIDIA Capture library (*NvFBC*) appears to interact poorly with v-sync if your refresh rate matches the capture FPS. Try disabling v-sync. Another option you can try is to set the following environment variable when capturing...

```
$ SHADOW_CAST_STRICT_FPS=0 shadow-cast ...
```

Please note, however, using this option will scale the output video's frame rate to match NvFBC's closest match (e.g. `62.5` in the case of a 60fps capture).

I haven't quite worked out the definitive cause of this "lagginess", but I suspect it is because NvFBC only accepts integer millisecond values as its sampling rate, so cannot exactly match *Shadow Cast*'s frame rate. For example, 60fps would require a fractional sampling frequency of `16.666` milliseconds, and NvFBC only allows either `16` or `17` milliseconds.
