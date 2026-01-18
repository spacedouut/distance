#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <windows.h>

#include "config.h"
#include "shared_memory.h"
#include "capture.h"

static volatile int running = 1;
static EncoderContext ctx = {0};
static SharedMemory *shm = NULL;
static CaptureBackend *backend = NULL;

void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -c, --config <file>     Config file (default: config.json)\n");
    printf("  -w, --width <int>       Capture width\n");
    printf("  -h, --height <int>      Capture height\n");
    printf("  -f, --fps <int>         Frames per second\n");
    printf("  -q, --quality <int>     Encoding quality (0-100)\n");
    printf("  -m, --monitor <int>     Monitor index (0=primary)\n");
    printf("  -e, --encoder <name>    Encoder backend (gdi, dxgi)\n");
    printf("  --codec <name>          Codec (h264, h265)\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("  --benchmark             Log frame timing\n");
    printf("  --list-backends         List available backends\n");
    printf("  --help                  Show this help\n");
}

void sigint_handler(int sig) {
    printf("\n[MAIN] Shutting down...\n");
    running = 0;
}

int main(int argc, char *argv[]) {
    printf("Distance Encoder - Starting\n");
    signal(SIGINT, sigint_handler);
    
    // Default config
    ctx.config.width = 1920;
    ctx.config.height = 1080;
    ctx.config.fps = 30;
    ctx.config.quality = 75;
    ctx.config.monitor = 0;
    ctx.config.encoder = strdup("gdi");
    ctx.config.codec = strdup("h264");
    ctx.config.shm_name = strdup("distance_video_0");
    ctx.config.shm_size = DEFAULT_FRAME_SIZE + HEADER_SIZE;
    ctx.config.verbose = 0;
    ctx.config_file = strdup("config.json");
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--list-backends") == 0) {
            capture_list_backends();
            return 0;
        } else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) && i + 1 < argc) {
            free(ctx.config_file);
            ctx.config_file = strdup(argv[++i]);
        } else if ((strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--width") == 0) && i + 1 < argc) {
            ctx.config.width = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--height") == 0) && i + 1 < argc) {
            ctx.config.height = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fps") == 0) && i + 1 < argc) {
            ctx.config.fps = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quality") == 0) && i + 1 < argc) {
            ctx.config.quality = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--monitor") == 0) && i + 1 < argc) {
            ctx.config.monitor = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--encoder") == 0) && i + 1 < argc) {
            free(ctx.config.encoder);
            ctx.config.encoder = strdup(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            ctx.config.verbose = 1;
        } else if (strcmp(argv[i], "--benchmark") == 0) {
            ctx.config.benchmark = 1;
        }
    }
    
    // Try to load config file
    if (config_load(ctx.config_file, &ctx.config) != 0) {
        printf("[MAIN] Warning: Could not load config file, using defaults\n");
    }
    
    config_print(&ctx.config);
    
    // Get capture backend
    backend = capture_get_backend(ctx.config.encoder);
    if (!backend) {
        printf("[ERROR] Unknown encoder: %s\n", ctx.config.encoder);
        return 1;
    }
    
    if (!backend->available()) {
        printf("[ERROR] Encoder not available on this system: %s\n", ctx.config.encoder);
        return 1;
    }
    
    printf("[MAIN] Using backend: %s\n", backend->name);
    
    // Initialize capture
    int cap_width, cap_height;
    if (backend->init(ctx.config.monitor, &cap_width, &cap_height) != 0) {
        printf("[ERROR] Failed to initialize capture backend\n");
        return 1;
    }
    
    printf("[CAPTURE] Initialized: %dx%d\n", cap_width, cap_height);
    
    // Create shared memory
    shm = shm_create(ctx.config.shm_name, ctx.config.shm_size);
    if (!shm) {
        printf("[ERROR] Failed to create shared memory\n");
        backend->shutdown();
        return 1;
    }
    
    printf("[SHM] Created: %s (%d bytes)\n", ctx.config.shm_name, ctx.config.shm_size);
    
    // Main capture loop
    printf("[MAIN] Starting capture loop (%d FPS)...\n", ctx.config.fps);
    shm_set_state(shm, SHM_STATE_RUNNING, SHM_ERR_NONE);
    
    int frame_count = 0;
    DWORD last_stats_time = GetTickCount();
    int frame_interval_ms = 1000 / ctx.config.fps;
    
    while (running) {
        DWORD frame_start = GetTickCount();
        
        // Capture frame
        int frame_size = 0;
        uint8_t *frame_data = backend->capture(&frame_size);
        
        if (!frame_data) {
            printf("[ERROR] Capture failed\n");
            shm_set_state(shm, SHM_STATE_ERROR, SHM_ERR_DXGI_FAIL);
            Sleep(100);
            continue;
        }
        
        // Write to shared memory
        if (shm_write_frame(shm, frame_data, frame_size,
                           cap_width, cap_height,
                           ctx.config.fps, ctx.config.quality,
                           ctx.config.monitor) != 0) {
            printf("[ERROR] Failed to write frame to shared memory\n");
            free(frame_data);
            continue;
        }
        
        free(frame_data);
        frame_count++;
        
        // Log stats
        DWORD now = GetTickCount();
        if (now - last_stats_time >= 2000) {
            printf("[CAPTURE] %d frames, %d bytes/frame\n", frame_count, frame_size);
            frame_count = 0;
            last_stats_time = now;
        }
        
        // Frame rate limiting
        DWORD elapsed = GetTickCount() - frame_start;
        if (elapsed < frame_interval_ms) {
            Sleep(frame_interval_ms - elapsed);
        } else if (ctx.config.benchmark) {
            printf("[BENCH] Frame took %lu ms (target %d ms)\n", elapsed, frame_interval_ms);
        }
    }
    
    // Cleanup
    printf("[MAIN] Cleaning up...\n");
    shm_set_state(shm, 0, SHM_ERR_NONE);
    shm_close(shm);
    backend->shutdown();
    config_free(&ctx.config);
    free(ctx.config_file);
    
    printf("[MAIN] Done\n");
    return 0;
}
