#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdint>
#include <string>

struct EncoderConfig {
    // Capture settings
    int width = 1920;
    int height = 1080;
    int fps = 30;
    int monitor = 0;  // 0 = primary, 1+ = additional monitors

    // Encoding settings
    int quality = 75;  // 0-100, meaning varies by encoder
    std::string encoder = "gdi";  // "gdi", "dxgi", "nvenc"
    std::string codec = "h264";    // "h264", "h265", "vp9"

    // Output settings
    std::string shm_name = "distance_video_0";
    int shm_size = 10 * 1024 * 1024 + 256;  // DEFAULT_FRAME_SIZE + HEADER_SIZE

    // Debug
    bool verbose = false;
    bool benchmark = false;
};

struct EncoderContext {
    EncoderConfig config;
    std::string config_file = "config.json";
};

// Load config from file (JSON)
int config_load(const char *path, EncoderConfig &out);

// Print config
void config_print(const EncoderConfig &config);

#endif
