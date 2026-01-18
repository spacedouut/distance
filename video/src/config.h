#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

typedef struct {
    // Capture settings
    int width;
    int height;
    int fps;
    int monitor;  // 0 = primary, 1+ = additional monitors
    
    // Encoding settings
    int quality;  // 0-100, meaning varies by encoder
    char *encoder;  // "gdi", "dxgi", "nvenc"
    char *codec;    // "h264", "h265", "vp9"
    
    // Output settings
    char *shm_name;  // Shared memory name
    int shm_size;    // Bytes
    
    // Debug
    int verbose;
    int benchmark;  // Log frame timing
} EncoderConfig;

typedef struct {
    EncoderConfig config;
    char *config_file;
} EncoderContext;

// Load config from file (JSON)
int config_load(const char *path, EncoderConfig *out);

// Parse command-line arguments
int config_parse_args(int argc, char *argv[], EncoderConfig *out);

// Merge args over file config (args take precedence)
int config_merge(EncoderConfig *file_config, EncoderConfig *args_config, EncoderConfig *out);

// Print config
void config_print(EncoderConfig *config);

// Free config resources
void config_free(EncoderConfig *config);

#endif
