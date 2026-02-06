#ifndef CAPTURE_HPP
#define CAPTURE_HPP

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

// Abstract base class for capture backends
class CaptureBackend {
public:
    virtual ~CaptureBackend() = default;

    // Get backend name
    virtual const char* get_name() const = 0;

    // Check if backend is available on this system
    virtual bool is_available() const = 0;

    // Initialize capture (returns actual capture dimensions)
    virtual bool init(int monitor, int &out_width, int &out_height) = 0;

    // Capture a frame (returns allocated buffer, caller must free)
    virtual uint8_t* capture(int &out_size) = 0;

    // Shutdown and cleanup
    virtual void shutdown() = 0;
};

// Factory function to create backend by name
std::unique_ptr<CaptureBackend> create_capture_backend(const std::string &name);

// List all available backends
void list_capture_backends();

#endif
