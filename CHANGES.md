# FreenectTD — Performance Improvements for Apple Silicon (M-Series)

This documents the changes made to bring the Kinect v2 plugin from 3–4 fps to stable,
smooth 30 fps on Apple M4 (and likely all Apple Silicon Macs).

---

## Summary

| Change | Effect |
|---|---|
| Producer/Consumer architecture | 3–4 fps → 10–15 fps |
| OpenCL packet pipeline | 10–15 fps → 30 fps (measured) |
| Async Metal pipeline | Eliminates frame stutter / freeze |
| Zero-allocation steady state | Eliminates cook-thread jitter |

---

## Change 1: Producer/Consumer Architecture (`FreenectV2.cpp`)

**Problem:** All heavy work (Metal GPU calls with `waitUntilCompleted`, `reg->apply()`,
point cloud loop) ran on TouchDesigner's cook thread, blocking TD's frame scheduler.

**Fix:** Dedicated worker thread handles all processing. Cook thread only swaps
pre-processed buffers into TD's output.

- `runWorker()` loop: `processFramesRaw()` → `processAndSubmitAsync()` → `collectAndPublish()`
- Cook thread: `getColorFrame()` / `getDepthFrame()` etc. are non-blocking O(1) swaps
- Three mutex-protected buffer layers: raw → ready → TD output
- Cook time went from ~33ms (blocking) to ~0.28ms (non-blocking)

---

## Change 2: OpenCL Packet Pipeline (`FreenectTOP.cpp`)

**Problem:** `libfreenect2::CpuPacketPipeline` decodes Kinect v2 JPEG (1920×1080) and
ToF depth data entirely on CPU, capping throughput at ~15 fps internally.

**Fix:** Use `OpenCLPacketPipeline` which offloads JPEG decoding and depth processing
to the GPU via OpenCL. Falls back to `CpuPacketPipeline` automatically if OpenCL fails.

```cpp
// In fn2_initDevice():
fn2_pipeline = new libfreenect2::OpenCLPacketPipeline(-1); // -1 = auto device
libfreenect2::Freenect2Device* dev = fn2_ctx->openDevice(fn2_serial, fn2_pipeline);
fn2_pipeline = nullptr;
if (!dev) {
    // Fallback to CPU
    fn2_pipeline = new libfreenect2::CpuPacketPipeline();
    dev = fn2_ctx->openDevice(fn2_serial, fn2_pipeline);
    fn2_pipeline = nullptr;
}
```

Note: On first launch, OpenCL kernels are compiled (5–30 s). Cached on subsequent runs.

---

## Change 3: Async Metal Pipeline (`KinectMetal.mm`, `FreenectV2.cpp`)

**Problem:** `[cmd waitUntilCompleted]` in `KinectMetal::processColor()` and
`processDepth()` blocked the worker thread until the GPU finished. Because the GPU is
shared with TouchDesigner's own renderer, this wait time was variable (1–20 ms).
When the worker cycle exceeded 33 ms (one Kinect frame interval), the next frame
arrived in `ready_` too late — TD displayed the same frame for 3 instead of 2 cook
cycles, causing a visible freeze every few frames.

**Fix:** Split Metal processing into non-blocking submit + deferred collect.
The GPU processes frame N during the ~33 ms `waitForNewFrame()` call for frame N+1.
By the time `collectAsync()` is called, the GPU has been idle for ~30 ms and returns
immediately.

### New worker loop structure

```
loop:
  Phase A: collectAndPublish()     — Metal waitUntilCompleted (returns instantly)
  Phase B: waitForNewFrame(~33ms)  — GPU processes frame N here, in parallel
  Phase C: processAndSubmitAsync() — CPU pre-processing + non-blocking Metal commit
```

### New KinectMetal API

```cpp
// Submit GPU work non-blocking. Pass nullptr to skip a stream.
bool submitAsync(const uint8_t* bgrx,  int cSrcW, int cSrcH, int cDstW, int cDstH,
                 const float*   depth, int dSrcW, int dSrcH, int dDstW, int dDstH,
                 float minD, float maxD);

// Wait for completion and copy results. Returns almost instantly (~30ms of GPU headstart).
bool collectAsync(uint8_t*  rgba,     int cDstW, int cDstH,
                  uint16_t* depthOut, int dDstW, int dDstH);
```

**Tradeoff:** Each frame is published one Kinect-frame-interval (~33 ms) later than
before. For interactive installations this is imperceptible. If sub-33ms latency is
critical, revert to the synchronous Metal path.

---

## Change 4: Zero-Allocation Steady State (`FreenectV2.h/cpp`, `FreenectTOP.h/cpp`)

**Problem:** Per-frame heap allocations (8 MB for raw RGB, 3.5 MB for processed color,
plus several smaller buffers) caused unpredictable `malloc`/`free` latency on both the
worker and cook threads.

**Fix:** All intermediate buffers are persistent member variables (`sc_rawRGB_`,
`sc_color_`, `sc_depth_`, etc.). `resize()` is a no-op in steady state (dimensions
don't change). Buffers are exchanged via `std::swap` (O(1) pointer swap, no copy).

Key details:
- Raw snapshot under `rawMutex_`: `std::swap(sc_rawRGB_, rgbBuffer)` instead of copy
- Publish under `readyMutex_`: `std::swap(ready_.color, sc_color_)` — no free under lock
- Cook-thread getters: `std::swap(ready_.color, out)` — creates a stable 3-buffer
  ping-pong (`sc_color_` ↔ `ready_.color` ↔ `fn2_colorBuf_`) with zero allocations
- `FreenectTOP` persistent buffers: `fn2_colorBuf_`, `fn2_depthBuf_`, `fn2_irBuf_`,
  `fn2_pcBuf_` as class members replace per-cook local vectors

---

## Files Changed

| File | Changes |
|---|---|
| `FreenectTOP.cpp` | OpenCL pipeline in `fn2_initDevice()`; persistent frame buffers in `fn2_execute()` |
| `FreenectTOP.h` | Added `fn2_colorBuf_`, `fn2_depthBuf_`, `fn2_irBuf_`, `fn2_pcBuf_` members |
| `FreenectV2.cpp` | Rewrote `runWorker()`; replaced `processAndCache()` with `processAndSubmitAsync()` + `collectAndPublish()`; zero-alloc buffer management |
| `FreenectV2.h` | Added `sc_*` scratch member buffers; pending frame state for async pipeline |
| `KinectMetal.h` | Replaced `processColorAndDepth()` with `submitAsync()` + `collectAsync()` |
| `KinectMetal.mm` | Implemented async Metal API; added `pendingCmd` state to `Impl` |

---

## Things That Were Tried and Did NOT Help

- **Removing Metal entirely** (using vImage only): Slower (4–5 fps) due to
  `kvImageHighQualityResampling` without `kvImageDoNotTile` spawning excessive threads.
  Do not remove Metal.

- **Batching Color+Depth in one command buffer** (`processColorAndDepth`): No measurable
  impact — Metal was not the throughput bottleneck, only the latency/timing bottleneck.

- **CPU-side micro-optimizations** (swap vs. copy under mutex, batch Metal passes):
  No impact while `CpuPacketPipeline` or blocking `waitUntilCompleted` were the
  actual bottlenecks.

- **`waitForNewFrame(frames, 0)`** (0 ms timeout): Can block indefinitely on some
  libfreenect2 builds. Always use at least 1 ms + iteration cap.

---

## Build Instructions (macOS, Apple Silicon)

```bash
cd "Kinect Plugin"
xcodebuild -project FreenectTD.xcodeproj -target FreenectTOP \
  -configuration Release -sdk macosx26.0 \
  ARCHS=arm64 VALID_ARCHS=arm64 CODE_SIGNING_ALLOWED=NO
```

Install:
```bash
SRC="build/Release/FreenectTOP.plugin"
DST="$HOME/Library/Application Support/Derivative/TouchDesigner099/Plugins/FreenectTOP.plugin"
rm -rf "$DST" && cp -r "$SRC" "$DST" && xattr -cr "$DST" && codesign --force --deep --sign - "$DST"
```

Common build errors:
- `unable to find sdk 'macosx15.x'` → use `-sdk macosx26.0`
- `VALID_ARCHS is empty` → pass `ARCHS=arm64 VALID_ARCHS=arm64`
- `resource fork not allowed` (codesign) → run `xattr -cr` before codesign
