#ifndef SHARED_MEMORY_HPP
#define SHARED_MEMORY_HPP

#include <cstdint>
#include <string>
#include <windows.h>

constexpr uint32_t MAGIC_NUMBER = 0xDEADBEEF;
constexpr int HEADER_SIZE = 256;
constexpr int DEFAULT_FRAME_SIZE = 10 * 1024 * 1024;  // 10MB max frame

struct SharedFrameBuffer {
    // Header (256 bytes total)
    uint32_t magic;          // [0:4]   0xDEADBEEF
    uint32_t sequence;       // [4:8]   Frame sequence number
    uint32_t frame_size;     // [8:12]  Actual frame data size
    uint32_t width;          // [12:16] Capture width
    uint32_t height;         // [16:20] Capture height
    uint32_t fps;            // [20:24] Frames per second
    uint32_t quality;        // [24:28] Encoding quality
    float timestamp;         // [28:32] Frame timestamp (seconds)
    uint32_t monitor;        // [32:36] Which monitor captured
    uint32_t state;          // [36:40] State flags (RUNNING, PAUSED, ERROR, etc)
    uint8_t error_code;      // [40:41] Last error if state=ERROR
    uint8_t _reserved[215];  // [41:256] Future expansion

    // Frame data
    uint8_t frame_data[DEFAULT_FRAME_SIZE];
};

class SharedMemory {
public:
    SharedMemory(const std::string &name, int size);
    ~SharedMemory();

    // Disable copy
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    // Check if creation was successful
    bool is_valid() const { return buffer != nullptr; }

    // Write frame to shared memory
    int write_frame(const uint8_t *frame_data, uint32_t size,
                    uint32_t width, uint32_t height, uint32_t fps, uint32_t quality,
                    uint32_t monitor);

    // Set state (RUNNING, PAUSED, ERROR)
    void set_state(uint32_t state, uint8_t error_code);

    // Get current sequence number
    uint32_t get_sequence() const;

private:
    HANDLE mapping = nullptr;
    SharedFrameBuffer *buffer = nullptr;
    int size = 0;
    std::string name;
};

// State flags
constexpr uint32_t SHM_STATE_RUNNING = 0x01;
constexpr uint32_t SHM_STATE_PAUSED = 0x02;
constexpr uint32_t SHM_STATE_ERROR = 0x04;

// Error codes
constexpr uint8_t SHM_ERR_NONE = 0x00;
constexpr uint8_t SHM_ERR_NO_DISPLAY = 0x01;
constexpr uint8_t SHM_ERR_DXGI_FAIL = 0x02;
constexpr uint8_t SHM_ERR_ENCODE_FAIL = 0x03;

#endif
