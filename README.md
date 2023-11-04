## Shadow Cast - GPU assisted screen and audio recording
*Shadow Cast* is a low-latency, low-overhead, GPU assisted screen plus audio capture utility.

Typical screen capture utilities copy the framebuffer data between host and GPU memory when encoding, causing significant load on the host CPU. This can affect both the running applications and the quality of the resulting media. *Shadow Cast* captures and encodes the framebuffer directly on the GPU with very little or no performance penalty to running applications. This makes *Shadow Cast* ideal for capturing gameplay footage, even in the most hardware intensive games.

### Capturing
You can start a capture session using...

```
$ shadow-cast <PATH TO OUTPUT FILE>
```

Ctrl+C / SIGINT will stop the capture session and finalize the output media.

### Help. I'm getting the following error

#### `ERROR: Couldn't create NvFBC instance`
*Shadow Cast* uses the *NvFBC* facility to provide efficient, low-latency framebuffer capture. By default, nvidia disables this on most (if not all) of its commercial GPUs. However, there are two ways around this restriction...

- You can find a utility to patch your nvidia drivers in the [keylase/nvidia-patch](https://github.com/keylase/nvidia-patch) GitHub repo.
- You can obtain a "key" to unlock this feature at runtime. You can then set the `SHADOW_CAST_NVFBC_KEY` environment variable to the base64 encoded value of this key prior to invoking `shadow-cast`. Unfortunately, due to the uncertainty around the legalities of this method, I'm unable to provide this key in this project's source.

### Requirements
- FFMpeg (libav)
- nvidia GPU, supporting NVENC and NvFBC
- Pipewire
- X11

### Building from source

After obtaining the source, change to the project's directory and run the following commands...

```
$ mkdir ./build
$ cd ./build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ cmake --build .
```

### Installing

Following the steps to build, above, you can install the built binary using...

```
$ sudo cmake --build . --target install
```

By default, this will install the `shadow-cast` binary to `/usr/local/bin`
