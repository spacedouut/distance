#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <string>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <time.h>
#endif

#include "config.hpp"
#include "shared_memory.hpp"
#include "capture.hpp"

static volatile int running = 1;

// ---------------------------------------------------------------------------
// Platform-portable timing helpers
// ---------------------------------------------------------------------------
static uint64_t get_tick_ms() {
#ifdef _WIN32
    return static_cast<uint64_t>(GetTickCount());
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
#endif
}

static void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

// ---------------------------------------------------------------------------

void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -c, --config <file>     Config file (default: config.json)\n");
    printf("  -w, --width <int>       Capture width\n");
    printf("  -h, --height <int>      Capture height\n");
    printf("  -f, --fps <int>         Frames per second\n");
    printf("  -q, --quality <int>     Encoding quality (0-100)\n");
    printf("  -m, --monitor <int>     Monitor index (0=primary)\n");
    printf("  -e, --encoder <name>    Encoder backend (gdi, dxgi, macos)\n");
    printf("  --codec <name>          Codec (h264, h265)\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("  --benchmark             Log frame timing\n");
    printf("  --list-backends         List available backends\n");
    printf("  --help                  Show this help\n");
}

void sigint_handler(int) {
    printf("\n[MAIN] Shutting down...\n");
    running = 0;
}

// Default encoder per platform
static const char* default_encoder() {
#ifdef _WIN32
    return "dxgi";
#elif defined(__APPLE__)
    return "macos";
#else
    return "unknown";
#endif
}

int main(int argc, char *argv[]) {
    printf("Distance Encoder - Starting\n");
    signal(SIGINT, sigint_handler);

    EncoderContext ctx;
    ctx.config.encoder = default_encoder();

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--list-backends") == 0) {
            list_capture_backends();
            return 0;
        } else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) && i + 1 < argc) {
            ctx.config_file = argv[++i];
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
            ctx.config.encoder = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            ctx.config.verbose = true;
        } else if (strcmp(argv[i], "--benchmark") == 0) {
            ctx.config.benchmark = true;
        }
    }

    // Try to load config file
    if (config_load(ctx.config_file.c_str(), ctx.config) != 0) {
        printf("[MAIN] Warning: Could not load config file, using defaults\n");
    }

    config_print(ctx.config);

    // Get capture backend
    auto backend = create_capture_backend(ctx.config.encoder);
    if (!backend) {
        printf("[ERROR] Unknown encoder: %s\n", ctx.config.encoder.c_str());
        return 1;
    }

    if (!backend->is_available()) {
        printf("[ERROR] Encoder not available on this system: %s\n", ctx.config.encoder.c_str());
        return 1;
    }

    printf("[MAIN] Using backend: %s\n", backend->get_name());

    // Initialize capture
    int cap_width, cap_height;
    if (!backend->init(ctx.config.monitor, cap_width, cap_height)) {
        printf("[ERROR] Failed to initialize capture backend\n");
        return 1;
    }

    printf("[CAPTURE] Initialized: %dx%d\n", cap_width, cap_height);

    // Shared memory (no-op stub on non-Windows; IPC handled inside the backend)
    auto shm = std::make_unique<SharedMemory>(ctx.config.shm_name, ctx.config.shm_size);
    if (!shm->is_valid()) {
        printf("[ERROR] Failed to create shared memory\n");
        backend->shutdown();
        return 1;
    }

    // Main capture loop
    printf("[MAIN] Starting capture loop (%d FPS)...\n", ctx.config.fps);
    shm->set_state(SHM_STATE_RUNNING, SHM_ERR_NONE);

    int frame_count = 0;
    uint64_t last_stats_time = get_tick_ms();
    int frame_interval_ms = 1000 / ctx.config.fps;

    while (running) {
        uint64_t frame_start = get_tick_ms();

        // Capture frame.
        // Push-model backends (e.g. macos) handle delivery internally and return nullptr here.
        int frame_size = 0;
        uint8_t *frame_data = backend->capture(frame_size);

        if (!frame_data) {
            // Either no new frame yet (DXGI timeout) or push-model backend — both are normal.
            sleep_ms(10);
            continue;
        }

        // Write to shared memory (Windows only; stub on other platforms)
        if (shm->write_frame(frame_data, frame_size,
                             cap_width, cap_height,
                             ctx.config.fps, ctx.config.quality,
                             ctx.config.monitor) != 0) {
            printf("[ERROR] Failed to write frame\n");
            delete[] frame_data;
            continue;
        }

        delete[] frame_data;
        frame_count++;

        // Log stats
        uint64_t now = get_tick_ms();
        if (now - last_stats_time >= 2000) {
            printf("[CAPTURE] %d frames, %d bytes/frame\n", frame_count, frame_size);
            frame_count = 0;
            last_stats_time = now;
        }

        // Frame rate limiting
        uint64_t elapsed = get_tick_ms() - frame_start;
        if (elapsed < (uint64_t)frame_interval_ms) {
            sleep_ms((int)(frame_interval_ms - elapsed));
        } else if (ctx.config.benchmark) {
            printf("[BENCH] Frame took %llu ms (target %d ms)\n",
                   (unsigned long long)elapsed, frame_interval_ms);
        }
    }

    // Cleanup
    printf("[MAIN] Cleaning up...\n");
    shm->set_state(0, SHM_ERR_NONE);
    backend->shutdown();

    printf("[MAIN] Done\n");
    return 0;
}
