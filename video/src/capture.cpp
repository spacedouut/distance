#include <cstdio>
#include "capture.hpp"

// Forward declarations
std::unique_ptr<CaptureBackend> create_gdi_backend();
std::unique_ptr<CaptureBackend> create_dxgi_backend();

std::unique_ptr<CaptureBackend> create_capture_backend(const std::string &name) {
    if (name == "gdi") {
        auto backend = create_gdi_backend();
        if (backend && backend->is_available()) {
            return backend;
        }
        printf("[CAPTURE] Backend 'gdi' not available on this system\n");
        return nullptr;
    } else if (name == "dxgi") {
        auto backend = create_dxgi_backend();
        if (backend && backend->is_available()) {
            return backend;
        }
        printf("[CAPTURE] Backend 'dxgi' not available on this system\n");
        return nullptr;
    }

    printf("[CAPTURE] Unknown backend: %s\n", name.c_str());
    return nullptr;
}

void list_capture_backends() {
    printf("Available capture backends:\n");

    auto gdi = create_gdi_backend();
    if (gdi) {
        bool avail = gdi->is_available();
        printf("  gdi %s\n", avail ? "(available)" : "(not available)");
    }

    auto dxgi = create_dxgi_backend();
    if (dxgi) {
        bool avail = dxgi->is_available();
        printf("  dxgi %s\n", avail ? "(available)" : "(not available)");
    }
}
