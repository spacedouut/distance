import asyncio
import websockets
import struct
import time
import random
from dataclasses import dataclass
from typing import Optional, Set
import threading
import struct

class FrameReader:
    def __init__(self, pipe_name=r'\\.\pipe\distance_video'):
        self.pipe_name = pipe_name
        self.pipe = None
        self.current_frame = None
        self.lock = threading.Lock()
        self.running = False
    
    def connect(self):
        """Connect to the named pipe."""
        import win32file
        import pywintypes
        
        try:
            self.pipe = win32file.CreateFile(
                self.pipe_name,
                win32file.GENERIC_READ,
                0,
                None,
                win32file.OPEN_EXISTING,
                0,
                None
            )
            print("[PIPE] Connected to video module")
            return True
        except pywintypes.error as e:
            print(f"[PIPE] Connection failed: {e}")
            return False
    
    def read_loop(self):
        """Read frames from pipe in background thread."""
        import win32file
        
        while self.running:
            if not self.pipe:
                if not self.connect():
                    time.sleep(1)
                    continue
            
            try:
                # Read 4-byte header
                header = win32file.ReadFile(self.pipe, 4)[1]
                if len(header) < 4:
                    self.pipe = None
                    continue
                
                frame_size = struct.unpack('!I', header)[0]
                
                # Read JPEG data with retry loop
                frame_data = b''
                while len(frame_data) < frame_size:
                    remaining = frame_size - len(frame_data)
                    chunk = win32file.ReadFile(self.pipe, min(65536, remaining))[1]
                    if not chunk:
                        print(f"[PIPE] Incomplete frame: got {len(frame_data)}/{frame_size} bytes")
                        break
                    frame_data += chunk
                
                if len(frame_data) == frame_size and frame_data[-2:] == b'\xff\xd9':
                    with self.lock:
                        self.current_frame = frame_data
                else:
                    print(f"[PIPE] Bad frame: size {len(frame_data)}/{frame_size}, EOI: {frame_data[-2:].hex() if frame_data else 'empty'}")
                    
            except Exception as e:
                print(f"[PIPE] Read error: {e}")
                self.pipe = None
                time.sleep(0.5)
    
    def get_frame(self):
        """Get the most recent frame."""
        with self.lock:
            frame = self.current_frame
            self.current_frame = None
        return frame
    
    def start(self):
        """Start reading in background."""
        self.running = True
        thread = threading.Thread(target=self.read_loop, daemon=True)
        thread.start()
        
# Mock frame generation (replace with real video/ module later)
def generate_test_frame(width: int = 800, height: int = 600) -> bytes:
    """Generate a fake JPEG frame for testing."""
    # This is a minimal valid JPEG header + random data
    # In reality, this comes from video/ C module
    frame_data = bytearray()
    frame_data.extend(b'\xff\xd8\xff\xe0')  # SOI + APP0
    frame_data.extend(b'\x00\x10JFIF')
    frame_data.extend(b'\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00')
    
    # Random payload (simulates actual JPEG data)
    payload_size = random.randint(15000, 25000)
    frame_data.extend(bytes(random.getrandbits(8) for _ in range(payload_size)))
    
    frame_data.extend(b'\xff\xd9')  # EOI
    return bytes(frame_data)


@dataclass
class StreamConfig:
    width: int = 1920
    height: int = 1080
    fps: int = 60
    quality: int = 75


class DistanceAgent:
    def __init__(self, host: str = "localhost", port: int = 8080):
        self.host = host
        self.port = port
        self.clients: Set[websockets.WebSocketServerProtocol] = set()
        self.config = StreamConfig()
        self.frame_count = 0
        self.last_stats_time = time.time()
        
    async def handler(self, websocket: websockets.WebSocketServerProtocol):
        """Handle incoming WebSocket connections."""
        self.clients.add(websocket)
        print(f"[CLIENT] Connected. Total clients: {len(self.clients)}")
        
        try:
            # Send initial config
            await self.send_config(websocket)
            
            # Keep connection alive, listen for input
            async for message in websocket:
                await self.handle_input(websocket, message)
                
        except websockets.exceptions.ConnectionClosed:
            print(f"[CLIENT] Disconnected. Remaining: {len(self.clients)}")
        finally:
            self.clients.discard(websocket)
    
    async def send_config(self, websocket: websockets.WebSocketServerProtocol):
        """Send initial stream config to client."""
        config_msg = struct.pack(
            '!BBHHII',
            0x01,  # Message type: CONFIG
            0,     # Reserved
            self.config.width,
            self.config.height,
            self.config.fps,
            self.config.quality
        )
        await websocket.send(config_msg)
        print(f"[CONFIG] Sent to client: {self.config.width}x{self.config.height} @ {self.config.fps}fps")
    
    async def handle_input(self, websocket: websockets.WebSocketServerProtocol, message: bytes):
        """Handle input from client (mouse, keyboard, settings)."""
        if len(message) < 1:
            return
        
        msg_type = message[0]
        
        if msg_type == 0x10:  # MOUSE_MOVE
            if len(message) >= 5:
                x, y = struct.unpack('!HH', message[1:5])
                print(f"[INPUT] Mouse: ({x}, {y})")
                # TODO: Inject mouse movement
                
        elif msg_type == 0x11:  # MOUSE_CLICK
            if len(message) >= 2:
                button = message[1]
                print(f"[INPUT] Click: button {button}")
                # TODO: Inject click
                
        elif msg_type == 0x20:  # KEYBOARD
            if len(message) >= 3:
                key_code, is_pressed = struct.unpack('!HB', message[1:4])
                print(f"[INPUT] Key: {key_code} ({'down' if is_pressed else 'up'})")
                # TODO: Inject key
    
    async def broadcast_frame(self, frame_data: bytes):
        """Send frame to all connected clients."""
        if not self.clients:
            return
        
        # Frame format: 1 byte type (0x02) + 4 bytes size + JPEG data
        frame_size = len(frame_data)
        message = struct.pack('!BI', 0x02, frame_size) + frame_data
        # Debug: check if frame ends with JPEG EOI marker
        if frame_data[-2:] != b'\xff\xd9':
            print(f"[WARN] Frame doesn't end with JPEG EOI marker! Last bytes: {frame_data[-4:].hex()}")
        
        # Send to all clients concurrently
        if self.clients:
            await asyncio.gather(
                *[client.send(message) for client in self.clients],
                return_exceptions=True
            )
        
        self.frame_count += 1
        
        # Log stats every second
        now = time.time()
        if now - self.last_stats_time >= 1.0:
            fps = self.frame_count / (now - self.last_stats_time)
            print(f"[STREAM] {fps:.1f} FPS, {frame_size} bytes/frame, {len(self.clients)} clients")
            self.frame_count = 0
            self.last_stats_time = now
    
    async def stream_frames(self):
        """Main streaming loop. Read from video/ module."""
        frame_interval = 1.0 / self.config.fps
        frame_reader = FrameReader()
        frame_reader.start()
        
        while True:
            try:
                # Get frame from video module
                frame = frame_reader.get_frame()
                
                if frame:  # Only send if we have a new frame
                    await self.broadcast_frame(frame)
                
                await asyncio.sleep(frame_interval)
                
            except Exception as e:
                print(f"[ERROR] Frame streaming: {e}")
                await asyncio.sleep(0.1)
    
    async def start(self):
        """Start the agent server."""
        print(f"[AGENT] Starting on {self.host}:{self.port}")
        
        # Start WebSocket server
        async with websockets.serve(self.handler, self.host, self.port):
            print(f"[AGENT] WebSocket server listening on ws://{self.host}:{self.port}")
            
            # Start frame streaming loop
            await self.stream_frames()


async def main():
    agent = DistanceAgent()
    await agent.start()


if __name__ == "__main__":
    asyncio.run(main())
