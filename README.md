## Shadow Cast - GPU accelerated screen and audio recording
*Shadow Cast* is a low-latency, low-overhead, GPU accelerated screen &amp; audio capture utility.

Typical screen capture utilities copy the framebuffer data between host and GPU memory when encoding, causing significant load on the host CPU. This can affect both the running applications and the quality of the resulting media. *Shadow Cast* captures and encodes the framebuffer directly on the GPU with very little or no performance penalty to running applications. This makes *Shadow Cast* ideal for capturing gameplay footage, even in the most hardware intensive games.

- [Example](#example---cyberpunk-2077)
- [Usage](#usage)
- [Requirements](#requirements)
- [Building from source](#building-from-source)
- [Installing](#installing)
- [Alternative projects](#some-alternative-projects)

#### Example - Cyberpunk 2077
[![Cyberpunk 2077](http://i3.ytimg.com/vi/frXGxrdgTLY/hqdefault.jpg)](https://www.youtube.com/watch?v=frXGxrdgTLY)

More examples can be found in [this YouTube playlist](https://www.youtube.com/watch?v=cczxo_S7tV8&list=PLnsSpqzSlJMKbVEuUbIfMhdgcoPpG-FoQ)

### Usage
You can start a capture session using...

```
$ shadow-cast [ <OPTIONS> ] <OUTPUT FILE>
```

The resulting media/container type is determined by the extension of `<OUTPUT FILE>`, e.g. `.mp4` or `.mkv`.

If no `OPTIONS` are specified then *Shadow Cast* will pick some sensible defaults for the audio/video encoders and sample/frame rates, but these can be changed by specifying the following `OPTIONS` on the command line...

- `-A <AUDIO ENCODER>` - All options available to `ffmpeg` should work here. Defaults to `libopus`
- `-V <VIDEO ENCODER>` - Available options are `h264_nvenc` and `hevc_nvenc`. Defaults to `hevc_nvenc`
- `-f <FRAME RATE>` - Values from `20` to `60` are accepted. Defaults to `60`
- `-s <SAMPLE RATE>` - Defaults to `48000` (_NOTE: Some encoders will only support certain sample rates. Shadow Cast will display an error if your chosen sample rate isn't supported_)

Ctrl+C / SIGINT will stop the capture session and finalize the output media.

### Help. I'm getting the following error

#### `ERROR: Couldn't create NvFBC instance`
*Shadow Cast* uses the *NvFBC* facility to provide efficient, low-latency framebuffer capture on X11. By default, NVIDIA disables this on most (if not all) of its consumer-level GPUs. However, there are two ways around this restriction...

- You can find a utility to patch your NVIDIA drivers in the [keylase/nvidia-patch](https://github.com/keylase/nvidia-patch) GitHub repo.
- You can obtain a "key" to unlock this feature at runtime. The key can be set at runtime via the `SHADOW_CAST_NVFBC_KEY=<BASE64 ENCODED KEY>` environment variable. I use this method but I'm not sure how "official" it is, so no keys are provided in this repo. Feel free to message me about this.

### Requirements
- FFMpeg (libav)
- NVIDIA GPU, supporting NVENC and NvFBC
- Pipewire
- X11 or Wayland

### Building from source
In order to build *Shadow Cast* you will need a C++20 compiler, plus the following libraries' headers/libs installed...

- ffmpeg / libav
- X11
- Pipewire
- Wayland (wayland-client, wayland-egl)
- libdrm

These can usually be obtained through your Linux distribution's package manager using the `-devel` packages of each.

After obtaining the source, change to the project's directory and run the following commands...

```
$ mkdir ./build
$ cd ./build
$ cmake ..
$ cmake --build .
```

_TIP: You may be able to speed up the_ `cmake --build .` _step by appending_ ` -- -j$(nproc)`

### Installing

Following the steps to build, above, you can install the built binary using...

```
$ sudo ./install-helper.sh
```

#### Installing to a Different Location

By default, *Shadow Cast* will be installed to `/usr/local/bin`. If you wish to install to a different location, run the following command _before_ running the above install step...

```
$ cmake -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> ..
```

... replacing `<INSTALL_DIR>` with your custom install path (E.g. `$HOME/.local`, `/opt`, etc.)

This will install the binary to `<INSTALL_DIR>/bin/shadow-cast`. Once installed, you must ensure that `<INSTALL_DIR>/bin` is in your `$PATH`.

### Some Alternative Projects
#### [gpu-screen-recorder](https://git.dec05eba.com/gpu-screen-recorder/about/)
*Shadow Cast* takes most of its inspiration from this project and it has been a very helpful reference. It seems to be the only other alternative that offers the same (if not better) performance and ease-of-use characteristics. *gpu-screen-recorder* offers AMD and Intel support in addition to NVIDIA.

#### [ffmpeg command-line utility](https://ffmpeg.org/)
This is the utility I used for capture prior to creating *Shadow Cast*. You can capture an X display using the `ffmpeg -f x11grab ...` command. In my experience, the performance when capturing gameplay footage is pretty terrible, and the resulting media will often contain lots of frame drops / tearing ([Example of `x11grab` vs *Shadow Cast* on YouTube](https://www.youtube.com/watch?v=ogFdvVON4z0)).

#### [OBS Studio](https://obsproject.com/)
Admittedly, I've never used this software. I'm aware that it is very popular for capturing / streaming but I don't really consider it to align with my use-cases. I much prefer a simple / small utility, where it is quick to setup a capture.
