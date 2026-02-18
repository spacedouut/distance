#ifndef SHARED_MEMORY_HPP
#define SHARED_MEMORY_HPP

#include <cstdint>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

constexpr uint32_t MAGIC_NUMBER = 0xDEADBEEF;
constexpr int HEADER_SIZE = 256;
constexpr int DEFAULT_FRAME_SIZE = 10 * 1024 * 1024;  // 10MB max frame

struct SharedFrameBuffer {
    uint32_t magic;
    uint32_t sequence;
    uint32_t frame_size;
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t quality;
    float    timestamp;
    uint32_t monitor;
    uint32_t state;
    uint8_t  error_code;
    uint8_t  _reserved[215];
    uint8_t  frame_data[DEFAULT_FRAME_SIZE];
};

// State flags
constexpr uint32_t SHM_STATE_RUNNING  = 0x01;
constexpr uint32_t SHM_STATE_PAUSED   = 0x02;
constexpr uint32_t SHM_STATE_ERROR    = 0x04;

// Error codes
constexpr uint8_t SHM_ERR_NONE        = 0x00;
constexpr uint8_t SHM_ERR_NO_DISPLAY  = 0x01;
constexpr uint8_t SHM_ERR_DXGI_FAIL   = 0x02;
constexpr uint8_t SHM_ERR_ENCODE_FAIL = 0x03;

#ifdef _WIN32

class SharedMemory {
public:
    SharedMemory(const std::string &name, int size);
    ~SharedMemory();

    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    bool is_valid() const { return buffer != nullptr; }

    int write_frame(const uint8_t *frame_data, uint32_t size,
                    uint32_t width, uint32_t height, uint32_t fps, uint32_t quality,
                    uint32_t monitor);

    void set_state(uint32_t state, uint8_t error_code);
    uint32_t get_sequence() const;

private:
    HANDLE mapping = nullptr;
    SharedFrameBuffer *buffer = nullptr;
    int size = 0;
    std::string name;
};

#else // !_WIN32

// On non-Windows platforms IPC is handled by the platform capture backend
// (e.g. Unix domain socket in macos.mm). Provide a no-op stub so main.cpp
// compiles without changes.
class SharedMemory {
public:
    SharedMemory(const std::string &, int) {}
    ~SharedMemory() {}

    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    bool is_valid() const { return true; }

    int write_frame(const uint8_t *, uint32_t, uint32_t, uint32_t,
                    uint32_t, uint32_t, uint32_t) { return 0; }

    void set_state(uint32_t, uint8_t) {}
    uint32_t get_sequence() const { return 0; }
};

#endif // _WIN32

#endif // SHARED_MEMORY_HPP
