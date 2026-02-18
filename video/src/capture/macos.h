#pragma once

#ifdef __APPLE__

#include "../capture.hpp"
#include <memory>

std::unique_ptr<CaptureBackend> create_macos_backend();

#endif // __APPLE__
