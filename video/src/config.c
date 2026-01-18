#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "config.h"

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
        return (int)item->valuedouble;
    }
    return default_val;
}

// Helper to get bool from JSON object
static int json_get_bool(cJSON *obj, const char *key, int default_val) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(item)) {
        return item->type == cJSON_True;
    }
    return default_val;
}

int config_load(const char *path, EncoderConfig *out) {
    if (!path || !out) {
        return -1;
    }
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("[CONFIG] Could not open file: %s\n", path);
        return -1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *json_str = malloc(size + 1);
    if (!json_str) {
        fclose(f);
        return -1;
    }
    
    size_t read = fread(json_str, 1, size, f);
    fclose(f);
    
    if (read != size) {
        free(json_str);
        return -1;
    }
    
    json_str[size] = '\0';
    
    // Parse JSON
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    
    if (!root) {
        printf("[CONFIG] JSON parse error\n");
        return -1;
    }
    
    // Get capture settings
    cJSON *capture = cJSON_GetObjectItemCaseSensitive(root, "capture");
    if (cJSON_IsObject(capture)) {
        out->width = json_get_int(capture, "width", out->width);
        out->height = json_get_int(capture, "height", out->height);
        out->fps = json_get_int(capture, "fps", out->fps);
        out->monitor = json_get_int(capture, "monitor", out->monitor);
        
        const char *encoder = json_get_string(capture, "encoder", NULL);
        if (encoder) {
            free(out->encoder);
            out->encoder = strdup(encoder);
        }
    }
    
    // Get encoding settings
    cJSON *encoding = cJSON_GetObjectItemCaseSensitive(root, "encoding");
    if (cJSON_IsObject(encoding)) {
        out->quality = json_get_int(encoding, "quality", out->quality);
        
        const char *codec = json_get_string(encoding, "codec", NULL);
        if (codec) {
            free(out->codec);
            out->codec = strdup(codec);
        }
    }
    
    // Get shared memory settings
    cJSON *shm = cJSON_GetObjectItemCaseSensitive(root, "shared_memory");
    if (cJSON_IsObject(shm)) {
        const char *name = json_get_string(shm, "name", NULL);
        if (name) {
            free(out->shm_name);
            out->shm_name = strdup(name);
        }
        
        int size = json_get_int(shm, "size", 0);
        if (size > 0) {
            out->shm_size = size;
        }
    }
    
    // Get debug settings
    cJSON *debug = cJSON_GetObjectItemCaseSensitive(root, "debug");
    if (cJSON_IsObject(debug)) {
        out->verbose = json_get_bool(debug, "verbose", out->verbose);
        out->benchmark = json_get_bool(debug, "benchmark", out->benchmark);
    }
    
    cJSON_Delete(root);
    
    printf("[CONFIG] Loaded from: %s\n", path);
    return 0;
}

int config_parse_args(int argc, char *argv[], EncoderConfig *out) {
    if (!out) {
        return -1;
    }
    
    // Already handled in main.c, this is just a placeholder
    // for future abstraction if needed
    return 0;
}

int config_merge(EncoderConfig *file_config, EncoderConfig *args_config, EncoderConfig *out) {
    if (!file_config || !args_config || !out) {
        return -1;
    }
    
    // Start with file config
    memcpy(out, file_config, sizeof(EncoderConfig));
    
    // Duplicate strings
    out->encoder = strdup(file_config->encoder);
    out->codec = strdup(file_config->codec);
    out->shm_name = strdup(file_config->shm_name);
    
    // Override with args if set
    if (args_config->width > 0) out->width = args_config->width;
    if (args_config->height > 0) out->height = args_config->height;
    if (args_config->fps > 0) out->fps = args_config->fps;
    if (args_config->quality > 0) out->quality = args_config->quality;
    if (args_config->monitor >= 0) out->monitor = args_config->monitor;
    if (args_config->encoder) {
        free(out->encoder);
        out->encoder = strdup(args_config->encoder);
    }
    if (args_config->codec) {
        free(out->codec);
        out->codec = strdup(args_config->codec);
    }
    if (args_config->verbose) out->verbose = 1;
    if (args_config->benchmark) out->benchmark = 1;
    
    return 0;
}

void config_print(EncoderConfig *config) {
    if (!config) return;
    
    printf("\n[CONFIG] Current settings:\n");
    printf("  Capture:\n");
    printf("    Resolution: %dx%d\n", config->width, config->height);
    printf("    FPS: %d\n", config->fps);
    printf("    Monitor: %d\n", config->monitor);
    printf("    Encoder: %s\n", config->encoder ? config->encoder : "unknown");
    printf("  Encoding:\n");
    printf("    Quality: %d\n", config->quality);
    printf("    Codec: %s\n", config->codec ? config->codec : "unknown");
    printf("  Shared Memory:\n");
    printf("    Name: %s\n", config->shm_name ? config->shm_name : "unknown");
    printf("    Size: %d MB\n", config->shm_size / (1024 * 1024));
    printf("  Debug:\n");
    printf("    Verbose: %s\n", config->verbose ? "yes" : "no");
    printf("    Benchmark: %s\n", config->benchmark ? "yes" : "no");
    printf("\n");
}

void config_free(EncoderConfig *config) {
    if (!config) return;
    
    if (config->encoder) free(config->encoder);
    if (config->codec) free(config->codec);
    if (config->shm_name) free(config->shm_name);
    
    memset(config, 0, sizeof(EncoderConfig));
}