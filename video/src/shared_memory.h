#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stdint.h>

#define MAGIC_NUMBER 0xDEADBEEF
#define HEADER_SIZE 256
#define DEFAULT_FRAME_SIZE (10 * 1024 * 1024)  // 10MB max frame

typedef struct {
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
} SharedFrameBuffer;

typedef struct {
    void *mapping;           // mmap handle (HANDLE on Windows)
    SharedFrameBuffer *buffer;
    int size;
    char *name;
} SharedMemory;

// Create shared memory (creates if not exists)
SharedMemory* shm_create(const char *name, int size);

// Open existing shared memory
SharedMemory* shm_open(const char *name);

// Close and cleanup
void shm_close(SharedMemory *shm);

// Write frame to shared memory
int shm_write_frame(SharedMemory *shm, const uint8_t *frame_data, uint32_t size,
                    uint32_t width, uint32_t height, uint32_t fps, uint32_t quality,
                    uint32_t monitor);

// Set state (RUNNING, PAUSED, ERROR)
void shm_set_state(SharedMemory *shm, uint32_t state, uint8_t error_code);

// Get current sequence number
uint32_t shm_get_sequence(SharedMemory *shm);

// State flags
#define SHM_STATE_RUNNING 0x01
#define SHM_STATE_PAUSED  0x02
#define SHM_STATE_ERROR   0x04

// Error codes
#define SHM_ERR_NONE      0x00
#define SHM_ERR_NO_DISPLAY 0x01
#define SHM_ERR_DXGI_FAIL 0x02
#define SHM_ERR_ENCODE_FAIL 0x03

#endif