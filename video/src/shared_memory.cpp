#include <cstdio>
#include <cstring>
#include "shared_memory.hpp"

#ifdef _WIN32

SharedMemory::SharedMemory(const std::string &name, int size) : size(size), name(name) {
    if (name.empty() || size <= 0) {
        return;
    }

    // Convert name to wide char for Windows API
    wchar_t wide_name[256];
    if (MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wide_name, 256) == 0) {
        return;
    }

    // Create file mapping
    mapping = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        size,
        wide_name
    );

    if (!mapping) {
        printf("[SHM] CreateFileMapping failed: %lu\n", GetLastError());
        return;
    }

    // Map view of file
    void *buf = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!buf) {
        printf("[SHM] MapViewOfFile failed: %lu\n", GetLastError());
        CloseHandle(mapping);
        mapping = nullptr;
        return;
    }

    buffer = static_cast<SharedFrameBuffer *>(buf);

    // Initialize header
    buffer->magic = MAGIC_NUMBER;
    buffer->sequence = 0;
    buffer->frame_size = 0;
    buffer->state = SHM_STATE_RUNNING;
    buffer->error_code = SHM_ERR_NONE;

    printf("[SHM] Created: %s (%d bytes)\n", name.c_str(), size);
}

SharedMemory::~SharedMemory() {
    if (buffer) {
        UnmapViewOfFile(buffer);
        buffer = nullptr;
    }

    if (mapping) {
        CloseHandle(mapping);
        mapping = nullptr;
    }

    printf("[SHM] Closed\n");
}

int SharedMemory::write_frame(const uint8_t *frame_data, uint32_t size,
                               uint32_t width, uint32_t height, uint32_t fps, uint32_t quality,
                               uint32_t monitor) {
    if (!buffer || !frame_data || size == 0) {
        return -1;
    }

    // Validate frame size
    if (size > DEFAULT_FRAME_SIZE) {
        printf("[SHM] Frame too large: %u bytes (max %u)\n", size, DEFAULT_FRAME_SIZE);
        return -1;
    }

    // Copy frame data
    memcpy(buffer->frame_data, frame_data, size);

    // Update header (atomic-ish, increment sequence last)
    buffer->frame_size = size;
    buffer->width = width;
    buffer->height = height;
    buffer->fps = fps;
    buffer->quality = quality;
    buffer->monitor = monitor;
    buffer->timestamp = static_cast<float>(GetTickCount()) / 1000.0f;

    // Increment sequence to signal new frame
    buffer->sequence++;

    return 0;
}

void SharedMemory::set_state(uint32_t state, uint8_t error_code) {
    if (!buffer) {
        return;
    }

    buffer->state = state;
    buffer->error_code = error_code;

    const char *state_str = "UNKNOWN";
    if (state == SHM_STATE_RUNNING) state_str = "RUNNING";
    else if (state == SHM_STATE_PAUSED) state_str = "PAUSED";
    else if (state == SHM_STATE_ERROR) state_str = "ERROR";

    printf("[SHM] State: %s (error: 0x%02x)\n", state_str, error_code);
}

uint32_t SharedMemory::get_sequence() const {
    if (!buffer) {
        return 0;
    }
    return buffer->sequence;
}

#endif // _WIN32
