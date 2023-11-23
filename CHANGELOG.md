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

