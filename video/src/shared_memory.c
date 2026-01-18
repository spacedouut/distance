#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "shared_memory.h"

SharedMemory* shm_create(const char *name, int size) {
    if (!name || size <= 0) {
        return NULL;
    }
    
    SharedMemory *shm = malloc(sizeof(SharedMemory));
    if (!shm) {
        return NULL;
    }
    
    // Convert name to wide char for Windows API
    int name_len = strlen(name);
    wchar_t wide_name[256];
    if (MultiByteToWideChar(CP_UTF8, 0, name, name_len, wide_name, 256) == 0) {
        free(shm);
        return NULL;
    }
    
    // Create file mapping
    HANDLE mapping = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        size,
        wide_name
    );
    
    if (!mapping) {
        printf("[SHM] CreateFileMapping failed: %lu\n", GetLastError());
        free(shm);
        return NULL;
    }
    
    // Map view of file
    void *buffer = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!buffer) {
        printf("[SHM] MapViewOfFile failed: %lu\n", GetLastError());
        CloseHandle(mapping);
        free(shm);
        return NULL;
    }
    
    // Initialize structure
    shm->mapping = mapping;
    shm->buffer = (SharedFrameBuffer *)buffer;
    shm->size = size;
    shm->name = strdup(name);
    
    // Initialize header
    shm->buffer->magic = MAGIC_NUMBER;
    shm->buffer->sequence = 0;
    shm->buffer->frame_size = 0;
    shm->buffer->state = SHM_STATE_RUNNING;
    shm->buffer->error_code = SHM_ERR_NONE;
    
    printf("[SHM] Created: %s (%d bytes)\n", name, size);
    return shm;
}

SharedMemory* shm_open(const char *name) {
    if (!name) {
        return NULL;
    }
    
    SharedMemory *shm = malloc(sizeof(SharedMemory));
    if (!shm) {
        return NULL;
    }
    
    // Convert name to wide char
    int name_len = strlen(name);
    wchar_t wide_name[256];
    if (MultiByteToWideChar(CP_UTF8, 0, name, name_len, wide_name, 256) == 0) {
        free(shm);
        return NULL;
    }
    
    // Open existing file mapping
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, wide_name);
    if (!mapping) {
        printf("[SHM] OpenFileMapping failed: %lu\n", GetLastError());
        free(shm);
        return NULL;
    }
    
    // Map view
    void *buffer = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!buffer) {
        printf("[SHM] MapViewOfFile failed: %lu\n", GetLastError());
        CloseHandle(mapping);
        free(shm);
        return NULL;
    }
    
    shm->mapping = mapping;
    shm->buffer = (SharedFrameBuffer *)buffer;
    shm->size = 0;  // Unknown when opening existing
    shm->name = strdup(name);
    
    printf("[SHM] Opened: %s\n", name);
    return shm;
}

void shm_close(SharedMemory *shm) {
    if (!shm) {
        return;
    }
    
    if (shm->buffer) {
        UnmapViewOfFile(shm->buffer);
    }
    
    if (shm->mapping) {
        CloseHandle(shm->mapping);
    }
    
    if (shm->name) {
        free(shm->name);
    }
    
    free(shm);
    printf("[SHM] Closed\n");
}

int shm_write_frame(SharedMemory *shm, const uint8_t *frame_data, uint32_t size,
                    uint32_t width, uint32_t height, uint32_t fps, uint32_t quality,
                    uint32_t monitor) {
    if (!shm || !shm->buffer || !frame_data || size == 0) {
        return -1;
    }
    
    // Validate frame size
    if (size > DEFAULT_FRAME_SIZE) {
        printf("[SHM] Frame too large: %u bytes (max %u)\n", size, DEFAULT_FRAME_SIZE);
        return -1;
    }
    
    // Copy frame data
    memcpy(shm->buffer->frame_data, frame_data, size);
    
    // Update header (atomic-ish, increment sequence last)
    shm->buffer->frame_size = size;
    shm->buffer->width = width;
    shm->buffer->height = height;
    shm->buffer->fps = fps;
    shm->buffer->quality = quality;
    shm->buffer->monitor = monitor;
    shm->buffer->timestamp = (float)GetTickCount() / 1000.0f;
    
    // Increment sequence to signal new frame
    shm->buffer->sequence++;
    
    return 0;
}

void shm_set_state(SharedMemory *shm, uint32_t state, uint8_t error_code) {
    if (!shm || !shm->buffer) {
        return;
    }
    
    shm->buffer->state = state;
    shm->buffer->error_code = error_code;
    
    const char *state_str = "UNKNOWN";
    if (state == SHM_STATE_RUNNING) state_str = "RUNNING";
    else if (state == SHM_STATE_PAUSED) state_str = "PAUSED";
    else if (state == SHM_STATE_ERROR) state_str = "ERROR";
    
    printf("[SHM] State: %s (error: 0x%02x)\n", state_str, error_code);
}

uint32_t shm_get_sequence(SharedMemory *shm) {
    if (!shm || !shm->buffer) {
        return 0;
    }
    return shm->buffer->sequence;
}
