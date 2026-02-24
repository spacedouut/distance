# Distance Remote Desktop Project

## Overview
Getting Distance (https://github.com/spacedouut/distance/) running on macOS using H.264 video streaming over WebSockets.

## Current Architecture
- **Agent** (`agent/main.py`): Spawns ffmpeg subprocess for screen capture + H.264 encoding, parses Annex B stdout into NAL units, broadcasts via WebSocket
- **Client** (`client/index.html`): WebCodecs path (Chrome/Safari/Edge) + MSE fallback (Firefox) for H.264 decoding, draws to canvas
- **Video module** (`video/*`): Cross-platform C++ (vestigial, kept for reference)

## Platform-Specific FFmpeg Commands

### macOS
```
ffmpeg -f avfoundation -i <screen_idx>:0 -c:v h264_videotoolbox -flags +low_delay -f h264 pipe:1
```
Auto-detects screen device index by parsing `ffmpeg -f avfoundation -list_devices true` output for "Capture screen 0".
Fallback: index 1 if detection fails.
CPU fallback: `libx264` with `-tune zerolatency`

### Windows
- **NVIDIA NVENC**: `ffmpeg -init_hw_device d3d11va -filter_complex ddagrab=0 -c:v h264_nvenc -zerolatency 1 -f h264 pipe:1`
- **Intel QSV**: `ffmpeg -init_hw_device d3d11va:,vendor_id=0x8086 -filter_complex ddagrab=0,hwmap=derive_device=qsv,format=qsv -c:v h264_qsv -f h264 pipe:1`
- CPU fallback: `gdigrab` + `libx264`

## Wire Protocol

### VIDEO_INIT (0x03) — sent once per stream
```
[1B type=0x03][2B width][2B height][4B sps_len][sps_bytes][4B pps_len][pps_bytes]
```

### VIDEO_FRAME (0x04) — sent per frame
```
[1B type=0x04][1B flags (0x01=keyframe)][8B size uint64 BE][Annex B data]
```
Note: size is 8 bytes (uint64), not 4.

### METADATA (0x01) — sent on connect
```
[1B type=0x01][1B reserved][2B width][2B height][4B fps][4B quality] = 14 bytes total
```

## Key Implementation Details

### Agent (`agent/main.py`)
- **FFmpegReader class**: Background thread spawns ffmpeg, reads Annex B stdout, parses NAL units, groups into frames
- **Annex B parsing**: `_find_startcode()` scans for 3-byte (`\x00\x00\x01`) or 4-byte (`\x00\x00\x00\x01`) startcodes
- **NAL type handling**: Type 7=SPS, 8=PPS, 5=IDR (keyframe), 1=non-IDR (delta), others appended to current frame
- **SPS parser**: Pure Python Exp-Golomb bit reader extracts `pic_width_in_mbs_minus1` and `pic_height_in_map_units_minus1`, converts to pixel dimensions
- **Thread-safe handoff**: ffmpeg thread → asyncio loop via `call_soon_threadsafe()` and lock-protected frame buffer
- **HW/SW fallback**: Tries HW encoder first (NVENC/h264_videotoolbox), falls back to libx264 on failure
- **No mock frames**: Pure H.264 only; agent blocks on stream_frames until ffmpeg produces data

### Client (`client/index.html`)
- **WebCodecs path** (Chrome/Safari/Edge):
  - `buildAVCDecoderConfig(sps, pps)` creates AVCC `AVCDecoderConfigurationRecord`
  - `codecStringFromSPS(sps)` extracts profile/level bytes → `avc1.XXXXXX` codec string
  - `VideoDecoder` configured with `optimizeForLatency: true`
  - Each frame wrapped in `EncodedVideoChunk` with `timestamp` in microseconds
  - Output frames drawn directly to canvas

- **MSE path** (Firefox fallback):
  - `buildFtypBox()` + `buildMoovBox()` for fMP4 initialization segment
  - `buildFragmentBoxes()` creates moof + mdat per frame with correct trun/tfdt/mfhd headers
  - `SourceBuffer` appended in segments, mode=`segments`
  - Hidden `<video>` element auto-played, drawn to canvas via `requestAnimationFrame`
  - Buffer trimmed to 2s to prevent unbounded growth
  - Queue prevents concurrent `appendBuffer` calls

- **Fallback on error**: WebCodecs failures → MSE on the fly
- **Stats**: `setInterval` loop decoupled from render for accurate FPS regardless of path
- **Codec detection**: Status overlay shows which decoder is active

## Recent Changes (Cleanup)
- Removed all mock JPEG code (base64 blob, `generate_test_frame()`, `broadcast_jpeg_frame()`)
- Removed unused `_h264_ready` and `_h264_new` threading events
- Agent's `stream_frames()` now just blocks on H.264; no fallback frame sends
- Client no longer parses `0x02` JPEG frames or has `renderJpeg()` method
- Stripped ~60 lines of dead code total

## Running
```bash
# Agent
cd distance/agent && uv run python main.py

# Client (dev server)
cd distance/client && npm run dev
# Open http://localhost:5173
```

## Browser Permissions Required
- **macOS**: Terminal must have Screen Recording permission (System Settings → Privacy & Security → Screen Recording)

## Known Limitations
- MSE fMP4 packing is minimal (no fancy box optimizations)
- SPS parser doesn't handle crop offsets (close enough for most streams)
- No input handling yet (mouse/keyboard stubs in place)
