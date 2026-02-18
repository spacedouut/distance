import asyncio
import websockets
import struct
import time
import base64
import subprocess
import shutil
import sys
import threading
from dataclasses import dataclass
from typing import Optional, Set

# ---------------------------------------------------------------------------
# Mock frame (320x240 red-to-blue gradient JPEG) — used when ffmpeg isn't up
# ---------------------------------------------------------------------------
_MOCK_JPEG_B64 = (
    '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'
)
_MOCK_JPEG = base64.b64decode(_MOCK_JPEG_B64)

def generate_test_frame() -> bytes:
    return _MOCK_JPEG


# ---------------------------------------------------------------------------
# Annex B NAL unit parser
# ---------------------------------------------------------------------------

def _find_startcode(data: bytes, start: int) -> int:
    """Return index of next 3- or 4-byte startcode at or after `start`, or -1."""
    i = start
    n = len(data)
    while i < n - 2:
        if data[i] == 0 and data[i+1] == 0:
            if data[i+2] == 1:
                return i          # 3-byte startcode
            if i+3 < n and data[i+2] == 0 and data[i+3] == 1:
                return i          # 4-byte startcode
        i += 1
    return -1

def _split_nalus(data: bytes) -> list[bytes]:
    """Split Annex B data into individual NAL unit payloads (startcodes stripped)."""
    nalus = []
    i = 0
    n = len(data)
    while i < n:
        sc = _find_startcode(data, i)
        if sc == -1:
            break
        # Skip the startcode itself
        sc_end = sc + (4 if sc + 3 < n and data[sc+2] == 0 else 3)
        next_sc = _find_startcode(data, sc_end)
        if next_sc == -1:
            nalus.append(data[sc_end:])
            break
        else:
            nalus.append(data[sc_end:next_sc])
        i = next_sc
    return [n for n in nalus if n]

def _nal_type(nalu: bytes) -> int:
    return (nalu[0] & 0x1f) if nalu else 0


# ---------------------------------------------------------------------------
# SPS parser — extracts width and height
# ---------------------------------------------------------------------------

def parse_sps_dimensions(sps: bytes) -> tuple[int, int]:
    """
    Parse pic_width / pic_height from a raw SPS NAL payload (no startcode).
    Returns (width, height) or (0, 0) on failure.
    Implements a minimal Exp-Golomb bit reader covering the fields we need.
    """
    try:
        # Skip NAL header byte
        bits = []
        for byte in sps[1:]:
            for bit in range(7, -1, -1):
                bits.append((byte >> bit) & 1)

        pos = [0]

        def read_bit():
            if pos[0] >= len(bits):
                return 0
            b = bits[pos[0]]
            pos[0] += 1
            return b

        def read_bits(n):
            v = 0
            for _ in range(n):
                v = (v << 1) | read_bit()
            return v

        def read_ue():  # unsigned Exp-Golomb
            lz = 0
            while read_bit() == 0:
                lz += 1
            val = (1 << lz) - 1 + read_bits(lz)
            return val

        def read_se():  # signed Exp-Golomb
            v = read_ue()
            return -(v >> 1) if v & 1 == 0 else (v + 1) >> 1

        profile_idc = read_bits(8)
        read_bits(8)   # constraint flags
        read_bits(8)   # level_idc
        read_ue()      # seq_parameter_set_id

        if profile_idc in (100, 110, 122, 244, 44, 83, 86, 118, 128, 138, 139, 134, 135):
            chroma = read_ue()
            if chroma == 3:
                read_bit()
            read_ue()   # bit_depth_luma_minus8
            read_ue()   # bit_depth_chroma_minus8
            read_bit()  # qpprime_y_zero_transform_bypass_flag
            if read_bit():  # seq_scaling_matrix_present_flag
                for i in range(8 if chroma != 3 else 12):
                    if read_bit():
                        size = 16 if i < 6 else 64
                        last = 8
                        next_s = 8
                        for _ in range(size):
                            if next_s:
                                next_s = (last + read_se()) % 256
                            last = next_s if next_s else last

        read_ue()   # log2_max_frame_num_minus4
        poc_type = read_ue()
        if poc_type == 0:
            read_ue()   # log2_max_pic_order_cnt_lsb_minus4
        elif poc_type == 1:
            read_bit()
            read_se(); read_se()
            for _ in range(read_ue()):
                read_se()

        read_ue()   # max_num_ref_frames
        read_bit()  # gaps_in_frame_num_value_allowed_flag

        pic_width_in_mbs   = read_ue() + 1
        pic_height_in_map  = read_ue() + 1

        frame_mbs_only = read_bit()
        height_mul = 1 if frame_mbs_only else 2

        width  = pic_width_in_mbs * 16
        height = pic_height_in_map * 16 * height_mul

        # Crop (optional — skip for now, close enough)
        return width, height
    except Exception:
        return 0, 0


# ---------------------------------------------------------------------------
# FFmpegReader: spawns ffmpeg, parses Annex B stdout into frame packets
# ---------------------------------------------------------------------------

def _build_ffmpeg_cmd() -> list[str]:
    ffmpeg = shutil.which('ffmpeg') or '/usr/local/bin/ffmpeg'
    common = ['-profile:v', 'baseline', '-level:v', '3.1', '-f', 'h264', 'pipe:1']

    if sys.platform == 'darwin':
        return [
            ffmpeg,
            '-f', 'avfoundation',
            '-capture_cursor', '1',
            '-framerate', '30',
            '-i', '1',                      # display index 1 in avfoundation list
            '-c:v', 'h264_videotoolbox',
            '-realtime', '1',
            '-b:v', '8M',
            '-flags', '+low_delay',
            '-max_delay', '0',
        ] + common

    elif sys.platform == 'win32':
        # Try NVENC via ddagrab; fall back handled in FFmpegReader.start()
        return [
            ffmpeg,
            '-init_hw_device', 'd3d11va',
            '-filter_complex', 'ddagrab=0',
            '-c:v', 'h264_nvenc',
            '-preset:v', 'll',
            '-zerolatency', '1',
        ] + common

    else:
        # Generic Linux / fallback
        return [
            ffmpeg,
            '-f', 'x11grab',
            '-framerate', '30',
            '-i', ':0.0',
            '-c:v', 'libx264',
            '-tune', 'zerolatency',
            '-preset', 'superfast',
        ] + common


def _build_ffmpeg_sw_cmd() -> list[str]:
    """CPU fallback command (libx264) for any platform."""
    ffmpeg = shutil.which('ffmpeg') or '/usr/local/bin/ffmpeg'
    common = ['-profile:v', 'baseline', '-level:v', '3.1', '-f', 'h264', 'pipe:1']

    if sys.platform == 'darwin':
        return [
            ffmpeg,
            '-f', 'avfoundation',
            '-capture_cursor', '1',
            '-framerate', '30',
            '-i', '1',
            '-c:v', 'libx264',
            '-tune', 'zerolatency',
            '-preset', 'superfast',
        ] + common
    elif sys.platform == 'win32':
        return [
            ffmpeg,
            '-f', 'gdigrab',
            '-framerate', '30',
            '-i', 'desktop',
            '-c:v', 'libx264',
            '-tune', 'zerolatency',
            '-preset', 'superfast',
        ] + common
    else:
        return [
            ffmpeg,
            '-f', 'x11grab',
            '-framerate', '30',
            '-i', ':0.0',
            '-c:v', 'libx264',
            '-tune', 'zerolatency',
            '-preset', 'superfast',
        ] + common


class FFmpegReader:
    """
    Spawns ffmpeg, reads raw Annex B H.264 from its stdout, and accumulates
    complete frame packets (one IDR or non-IDR slice group, possibly with
    leading SPS/PPS) for the agent to broadcast.
    """

    def __init__(self):
        self.proc: Optional[subprocess.Popen] = None
        self.sps: bytes = b''
        self.pps: bytes = b''
        self.width: int = 0
        self.height: int = 0
        self._pending_packet: Optional[bytes] = None  # latest complete frame
        self._pending_is_key: bool = False
        self._init_sent: bool = False
        self.lock = threading.Lock()
        self.running = False
        self._on_init = None   # callable(sps, pps, w, h) — called once
        self._on_frame = None  # callable(data, is_key) — called per frame

    def set_callbacks(self, on_init, on_frame):
        self._on_init = on_init
        self._on_frame = on_frame

    def start(self):
        self.running = True
        thread = threading.Thread(target=self._run, daemon=True)
        thread.start()

    def _try_start_proc(self, cmd: list[str]) -> bool:
        try:
            print(f"[FFMPEG] Starting: {' '.join(cmd)}")
            self.proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                bufsize=0,
            )
            # Give it a moment to fail fast if the device/encoder is wrong
            time.sleep(1.5)
            if self.proc.poll() is not None:
                print(f"[FFMPEG] Process exited early (code {self.proc.returncode})")
                return False
            return True
        except Exception as e:
            print(f"[FFMPEG] Failed to start: {e}")
            return False

    def _run(self):
        hw_cmd = _build_ffmpeg_cmd()
        sw_cmd = _build_ffmpeg_sw_cmd()

        if not self._try_start_proc(hw_cmd):
            print("[FFMPEG] HW encoder failed, trying SW fallback (libx264)...")
            if not self._try_start_proc(sw_cmd):
                print("[FFMPEG] Both encoders failed — mock frames will be used")
                self.running = False
                return

        self._parse_stdout()

    def _parse_stdout(self):
        """
        Read ffmpeg stdout in chunks, accumulate Annex B data, split into
        NAL units, and group into per-frame packets.
        """
        CHUNK = 65536
        buf = bytearray()
        current_frame_nalus: list[bytes] = []
        current_is_key = False

        try:
            while self.running and self.proc and self.proc.poll() is None:
                chunk = self.proc.stdout.read(CHUNK)
                if not chunk:
                    break
                buf.extend(chunk)

                # Process complete NAL units — keep last incomplete one in buf
                # by only scanning up to a safe point
                while True:
                    sc = _find_startcode(bytes(buf), 0)
                    if sc == -1:
                        break
                    sc_end = sc + (4 if sc + 3 < len(buf) and buf[sc+2] == 0 else 3)
                    next_sc = _find_startcode(bytes(buf), sc_end)
                    if next_sc == -1:
                        # Incomplete NAL — leave in buf
                        break

                    nalu = bytes(buf[sc_end:next_sc])
                    del buf[:next_sc]

                    if not nalu:
                        continue

                    ntype = _nal_type(nalu)

                    if ntype == 7:  # SPS
                        self.sps = nalu
                        w, h = parse_sps_dimensions(nalu)
                        if w and h:
                            self.width, self.height = w, h

                    elif ntype == 8:  # PPS
                        self.pps = nalu

                        # Fire on_init once we have both SPS and PPS
                        if self.sps and not self._init_sent and self._on_init:
                            self._init_sent = True
                            print(f"[H264] Got SPS ({len(self.sps)}B), PPS ({len(self.pps)}B), "
                                  f"stream is {self.width}x{self.height}")
                            self._on_init(self.sps, self.pps, self.width, self.height)

                    elif ntype == 5:  # IDR (keyframe)
                        if current_frame_nalus:
                            self._emit_frame(current_frame_nalus, current_is_key)
                        current_frame_nalus = [nalu]
                        current_is_key = True

                    elif ntype == 1:  # Non-IDR slice
                        if current_frame_nalus and current_is_key:
                            # Key frame was accumulating — emit it before starting delta
                            self._emit_frame(current_frame_nalus, current_is_key)
                            current_frame_nalus = [nalu]
                            current_is_key = False
                        elif current_frame_nalus and not current_is_key:
                            # Previous delta — emit and start fresh
                            self._emit_frame(current_frame_nalus, current_is_key)
                            current_frame_nalus = [nalu]
                            current_is_key = False
                        else:
                            current_frame_nalus = [nalu]
                            current_is_key = False

                    else:
                        # SEI, AUD, filler etc — append to current frame
                        if current_frame_nalus:
                            current_frame_nalus.append(nalu)

        except Exception as e:
            print(f"[FFMPEG] Parse error: {e}")
        finally:
            print("[FFMPEG] Stream ended")
            self.running = False

    def _emit_frame(self, nalus: list[bytes], is_key: bool):
        """Reassemble NALUs into Annex B and fire on_frame callback."""
        packet = b''
        for n in nalus:
            packet += b'\x00\x00\x00\x01' + n
        if self._on_frame:
            self._on_frame(packet, is_key)

    def stop(self):
        self.running = False
        if self.proc:
            self.proc.terminate()
            self.proc = None


# ---------------------------------------------------------------------------
# Agent
# ---------------------------------------------------------------------------

@dataclass
class StreamConfig:
    width: int = 1920
    height: int = 1080
    fps: int = 30
    quality: int = 75


class DistanceAgent:
    def __init__(self, host: str = "localhost", port: int = 8080):
        self.host = host
        self.port = port
        self.clients: Set[websockets.WebSocketServerProtocol] = set()
        self.config = StreamConfig()
        self.frame_count = 0
        self.last_stats_time = time.time()
        self.total_bytes = 0

        # H.264 state
        self._h264_init_msg: Optional[bytes] = None  # cached VIDEO_INIT message
        self._h264_ready = threading.Event()          # set when init is sent

        # Latest H.264 frame (set from ffmpeg thread, read from async loop)
        self._latest_h264: Optional[bytes] = None
        self._latest_is_key: bool = False
        self._h264_lock = threading.Lock()
        self._h264_new = threading.Event()  # signalled when a new frame arrives

    # ------------------------------------------------------------------
    # WebSocket connection handler
    # ------------------------------------------------------------------

    async def handler(self, websocket: websockets.WebSocketServerProtocol):
        self.clients.add(websocket)
        print(f"[CLIENT] Connected. Total clients: {len(self.clients)}")
        try:
            await self.send_config(websocket)
            # If H.264 is already initialised, send VIDEO_INIT to this new client
            if self._h264_init_msg:
                await websocket.send(self._h264_init_msg)
            async for message in websocket:
                await self.handle_input(websocket, message)
        except websockets.exceptions.ConnectionClosed:
            print(f"[CLIENT] Disconnected. Remaining: {len(self.clients) - 1}")
        finally:
            self.clients.discard(websocket)

    # ------------------------------------------------------------------
    # Protocol helpers
    # ------------------------------------------------------------------

    async def send_config(self, websocket):
        msg = struct.pack(
            '!BBHHII',
            0x01, 0,
            self.config.width, self.config.height,
            self.config.fps, self.config.quality,
        )
        await websocket.send(msg)
        print(f"[CONFIG] Sent {self.config.width}x{self.config.height} @ {self.config.fps}fps")

    async def _broadcast(self, message: bytes):
        if not self.clients:
            return
        await asyncio.gather(
            *[c.send(message) for c in self.clients],
            return_exceptions=True,
        )

    async def broadcast_h264_init(self, sps: bytes, pps: bytes, w: int, h: int):
        """Send VIDEO_INIT (0x03) to all current and future clients."""
        msg = struct.pack('!BHH', 0x03, w, h)
        msg += struct.pack('!I', len(sps)) + sps
        msg += struct.pack('!I', len(pps)) + pps
        self._h264_init_msg = msg

        # Also update config dimensions
        self.config.width = w or self.config.width
        self.config.height = h or self.config.height
        print(f"[H264] Sending VIDEO_INIT to {len(self.clients)} client(s)")
        await self._broadcast(msg)

    async def broadcast_h264_frame(self, data: bytes, is_key: bool):
        """Send VIDEO_FRAME (0x04)."""
        flags = 0x01 if is_key else 0x00
        msg = struct.pack('!BBQ', 0x04, flags, len(data)) + data
        await self._broadcast(msg)

        self.frame_count += 1
        self.total_bytes += len(data)
        now = time.time()
        if now - self.last_stats_time >= 1.0:
            fps = self.frame_count / (now - self.last_stats_time)
            mbps = (self.total_bytes * 8) / (now - self.last_stats_time) / 1e6
            print(f"[STREAM] {fps:.1f} FPS  {mbps:.2f} Mbps  {len(self.clients)} clients")
            self.frame_count = 0
            self.total_bytes = 0
            self.last_stats_time = now

    async def broadcast_jpeg_frame(self, frame_data: bytes):
        """Legacy JPEG broadcast (mock fallback)."""
        msg = struct.pack('!BI', 0x02, len(frame_data)) + frame_data
        await self._broadcast(msg)
        self.frame_count += 1

    # ------------------------------------------------------------------
    # Input handling (stubs)
    # ------------------------------------------------------------------

    async def handle_input(self, websocket, message: bytes):
        if len(message) < 1:
            return
        t = message[0]
        if t == 0x10 and len(message) >= 5:
            x, y = struct.unpack('!HH', message[1:5])
            print(f"[INPUT] Mouse: ({x}, {y})")
        elif t == 0x11 and len(message) >= 2:
            print(f"[INPUT] Click: button {message[1]}")
        elif t == 0x20 and len(message) >= 3:
            key, pressed = struct.unpack('!HB', message[1:4])
            print(f"[INPUT] Key: {key} ({'down' if pressed else 'up'})")

    # ------------------------------------------------------------------
    # FFmpeg callbacks (called from background thread → schedule on loop)
    # ------------------------------------------------------------------

    def _on_h264_init(self, sps: bytes, pps: bytes, w: int, h: int):
        asyncio.get_event_loop().call_soon_threadsafe(
            lambda: asyncio.ensure_future(self.broadcast_h264_init(sps, pps, w, h))
        )

    def _on_h264_frame(self, data: bytes, is_key: bool):
        with self._h264_lock:
            self._latest_h264 = data
            self._latest_is_key = is_key
        self._h264_new.set()

    # ------------------------------------------------------------------
    # Main streaming loop
    # ------------------------------------------------------------------

    async def stream_frames(self):
        reader = FFmpegReader()
        reader.set_callbacks(self._on_h264_init, self._on_h264_frame)
        reader.start()

        frame_interval = 1.0 / self.config.fps

        while True:
            try:
                # Check for a new H.264 frame (non-blocking)
                with self._h264_lock:
                    h264_data = self._latest_h264
                    is_key = self._latest_is_key
                    self._latest_h264 = None

                if h264_data is not None:
                    await self.broadcast_h264_frame(h264_data, is_key)
                elif not reader.running or self._h264_init_msg is None:
                    # ffmpeg not yet up — send mock JPEG
                    await self.broadcast_jpeg_frame(generate_test_frame())

                await asyncio.sleep(frame_interval)

            except Exception as e:
                print(f"[ERROR] Stream loop: {e}")
                await asyncio.sleep(0.1)

    # ------------------------------------------------------------------

    async def start(self):
        print(f"[AGENT] Starting on {self.host}:{self.port}")
        async with websockets.serve(self.handler, self.host, self.port):
            print(f"[AGENT] WebSocket server listening on ws://{self.host}:{self.port}")
            await self.stream_frames()


async def main():
    agent = DistanceAgent()
    await agent.start()


if __name__ == "__main__":
    asyncio.run(main())
