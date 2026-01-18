#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "capture.h"

// Forward declare backends
extern CaptureBackend capture_gdi;
// extern CaptureBackend capture_dxgi;  // TODO

static CaptureBackend *backends[] = {
    &capture_gdi,
    // &capture_dxgi,
    NULL
};

CaptureBackend* capture_get_backend(const char *name) {
    if (!name) {
        return NULL;
    }
    
    for (int i = 0; backends[i]; i++) {
        if (strcmp(backends[i]->name, name) == 0) {
            if (backends[i]->available()) {
                return backends[i];
            } else {
                printf("[CAPTURE] Backend '%s' not available on this system\n", name);
                return NULL;
            }
        }
    }
    
    printf("[CAPTURE] Unknown backend: %s\n", name);
    return NULL;
}

void capture_list_backends(void) {
    printf("Available capture backends:\n");
    for (int i = 0; backends[i]; i++) {
        int avail = backends[i]->available();
        printf("  %s %s\n", backends[i]->name, avail ? "(available)" : "(not available)");
    }
}
