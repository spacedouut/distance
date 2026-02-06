#include <cstdio>
#include <cstring>
#include <fstream>
#include "cJSON/cJSON.h"
#include "config.hpp"

// Helper to get string from JSON object
static const char* json_get_string(cJSON *obj, const char *key, const char *default_val) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item)) {
        return item->valuestring;
    }
    return default_val;
}

// Helper to get int from JSON object
static int json_get_int(cJSON *obj, const char *key, int default_val) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) {
        return static_cast<int>(item->valuedouble);
    }
    return default_val;
}

// Helper to get bool from JSON object
static bool json_get_bool(cJSON *obj, const char *key, bool default_val) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(item)) {
        return item->type == cJSON_True;
    }
    return default_val;
}

int config_load(const char *path, EncoderConfig &out) {
    if (!path) {
        return -1;
    }

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        printf("[CONFIG] Could not open file: %s\n", path);
        return -1;
    }

    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    std::string json_str(size, '\0');
    if (!f.read(&json_str[0], size)) {
        return -1;
    }

    // Parse JSON
    cJSON *root = cJSON_Parse(json_str.c_str());
    if (!root) {
        printf("[CONFIG] JSON parse error\n");
        return -1;
    }

    // Get capture settings
    cJSON *capture = cJSON_GetObjectItemCaseSensitive(root, "capture");
    if (cJSON_IsObject(capture)) {
        out.width = json_get_int(capture, "width", out.width);
        out.height = json_get_int(capture, "height", out.height);
        out.fps = json_get_int(capture, "fps", out.fps);
        out.monitor = json_get_int(capture, "monitor", out.monitor);

        const char *encoder = json_get_string(capture, "encoder", nullptr);
        if (encoder) {
            out.encoder = encoder;
        }
    }

    // Get encoding settings
    cJSON *encoding = cJSON_GetObjectItemCaseSensitive(root, "encoding");
    if (cJSON_IsObject(encoding)) {
        out.quality = json_get_int(encoding, "quality", out.quality);

        const char *codec = json_get_string(encoding, "codec", nullptr);
        if (codec) {
            out.codec = codec;
        }
    }

    // Get shared memory settings
    cJSON *shm = cJSON_GetObjectItemCaseSensitive(root, "shared_memory");
    if (cJSON_IsObject(shm)) {
        const char *name = json_get_string(shm, "name", nullptr);
        if (name) {
            out.shm_name = name;
        }

        int size = json_get_int(shm, "size", 0);
        if (size > 0) {
            out.shm_size = size;
        }
    }

    // Get debug settings
    cJSON *debug = cJSON_GetObjectItemCaseSensitive(root, "debug");
    if (cJSON_IsObject(debug)) {
        out.verbose = json_get_bool(debug, "verbose", out.verbose);
        out.benchmark = json_get_bool(debug, "benchmark", out.benchmark);
    }

    cJSON_Delete(root);

    printf("[CONFIG] Loaded from: %s\n", path);
    return 0;
}

void config_print(const EncoderConfig &config) {
    printf("\n[CONFIG] Current settings:\n");
    printf("  Capture:\n");
    printf("    Resolution: %dx%d\n", config.width, config.height);
    printf("    FPS: %d\n", config.fps);
    printf("    Monitor: %d\n", config.monitor);
    printf("    Encoder: %s\n", config.encoder.c_str());
    printf("  Encoding:\n");
    printf("    Quality: %d\n", config.quality);
    printf("    Codec: %s\n", config.codec.c_str());
    printf("  Shared Memory:\n");
    printf("    Name: %s\n", config.shm_name.c_str());
    printf("    Size: %d MB\n", config.shm_size / (1024 * 1024));
    printf("  Debug:\n");
    printf("    Verbose: %s\n", config.verbose ? "yes" : "no");
    printf("    Benchmark: %s\n", config.benchmark ? "yes" : "no");
    printf("\n");
}
