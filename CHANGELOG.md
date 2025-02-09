## 0.7.2
### PATCH Changes:
- The mouse cursor is now captured on Wayland (fixes #14)

## 0.7.1
### PATCH Changes:
- Users now are now no longer required to have the `shadow-cast-kms` executable in their `$PATH` if installed to a custom location. The process will look for it in the same location as the main `shadow-cast` executable.
- Reverts to using FPS as the video encoding timebase. This avoids some surprising behaviour with nvenc
- Fixes color conversion on Wayland compositors that don't use a linear pixel format for DRM planes (#39) (thanks: @SleepingPanda)
- Fixes an issue where a DRM driver can be incorretly reported as unavailable in Wayland (thanks: @SleepingPanda).
- Adds ability to output audio and video frame-time histogram metrics. This is enabled via the configure-time option `-DSHADOW_CAST_ENABLE_HISTOGRAMS=ON`
- Fixes an A/V desync issue with the opus encoder
- Fixes an issue where builds would fail if the FFMpeg headers are located under a subfolder within `/usr/include` (thanks: @SleepingPanda)

## 0.7.0
### MINOR Changes:
- Improves encoding performance by using a dedicated thread
- Adds a build-time option to enable frame time metric collection
- Reduces frame "jitter" when capturing certain games in X11
- Enables LTO when supported by compiler/linker

### PATCH Changes:
- Fixes an issue that was causing builds to fail on musl platforms (#24)
- Fixes a memory leak in H/W encoding pipeline

## 0.6.3
### PATCH Changes:
- Fixes an issue where the build would fail if `cppcheck` executable wasn't available

## 0.6.2
### PATCH Changes:
- Fixes a bug that caused no valid values to be accepted for the `-V` cmdline option (#18)
- Fixes build incompatibilities with ffmpeg6 - thankyou, @guihkx (#17)

## 0.6.1
### PATCH Changes:
- Fixes bug where capture would fail with `ERROR: No DRM planes received` on some Wayland compositors ([#12](https://github.com/gmbeard/shadow-cast/issues/12))

## 0.6.0
### MINOR Changes:
- Adds Wayland support

## 0.5.0
### MINOR Changes:
- Removes excess `AVPacket` allocations
- Adds command line options
- Allows video/audio codecs, audio sample rate, and video frame rate to be specified on cmdline

## 0.4.0
### MINOR Changes:
- Removes unnecessary thread synchronization to reduce contention
- Reads the screen resolution from the default X display

## 0.3.0
### MINOR Changes:
- Disables NvFBC push model by default to prevent some capture artifacts
- Performance improvement to audio capture by using a memory pool to reduce allocations
- Adds multi-threaded encoding

## 0.2.0
### MINOR Changes:
- Fixes some timing issues that were causing audio / video desync.
- Fixes `mp4` output containers.

## 0.1.0
### Initial Release:
- Limited support
- GPU accelerated framebuffer capture
- nVidia GPUs only (1440p, 60fps, h264)
- Pipewire audio only (stereo, 48Khz, aac)
- Continuous capture to a specified file
- Only reliably creates`.mkv` containers

