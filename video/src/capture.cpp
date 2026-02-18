#include <cstdio>
#include "capture.hpp"

// Platform-specific backend factory declarations.
// Only the backends compiled for the current platform are declared here.
#ifdef _WIN32
std::unique_ptr<CaptureBackend> create_gdi_backend();
std::unique_ptr<CaptureBackend> create_dxgi_backend();
#endif

#ifdef __APPLE__
std::unique_ptr<CaptureBackend> create_macos_backend();
#endif

std::unique_ptr<CaptureBackend> create_capture_backend(const std::string &name) {
#ifdef _WIN32
    if (name == "gdi") {
        auto backend = create_gdi_backend();
        if (backend && backend->is_available()) return backend;
        printf("[CAPTURE] Backend 'gdi' not available\n");
        return nullptr;
    } else if (name == "dxgi") {
        auto backend = create_dxgi_backend();
        if (backend && backend->is_available()) return backend;
        printf("[CAPTURE] Backend 'dxgi' not available\n");
        return nullptr;
    }
#endif

#ifdef __APPLE__
    if (name == "macos") {
        auto backend = create_macos_backend();
        if (backend && backend->is_available()) return backend;
        printf("[CAPTURE] Backend 'macos' not available\n");
        return nullptr;
    }
#endif

    printf("[CAPTURE] Unknown backend: %s\n", name.c_str());
    return nullptr;
}

void list_capture_backends() {
    printf("Available capture backends:\n");

#ifdef _WIN32
    { auto b = create_gdi_backend();  if (b) printf("  gdi  %s\n",  b->is_available() ? "(available)" : "(not available)"); }
    { auto b = create_dxgi_backend(); if (b) printf("  dxgi %s\n",  b->is_available() ? "(available)" : "(not available)"); }
#endif

#ifdef __APPLE__
    { auto b = create_macos_backend(); if (b) printf("  macos %s\n", b->is_available() ? "(available)" : "(not available)"); }
#endif
}
