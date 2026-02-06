#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include <turbojpeg.h>
#include "../capture.hpp"

class GDIBackend : public CaptureBackend {
public:
    GDIBackend() = default;
    ~GDIBackend() override { shutdown(); }

    const char* get_name() const override {
        return "gdi";
    }

    bool is_available() const override {
        // GDI is always available on Windows
        return true;
    }

    bool init(int monitor, int &out_width, int &out_height) override {
        // TODO: Handle multiple monitors
        if (monitor != 0) {
            printf("[GDI] Only primary monitor (0) supported currently\n");
            return false;
        }

        // Get screen dimensions
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);

        printf("[GDI] Screen: %dx%d\n", width, height);

        // Initialize TurboJPEG
        compressor = tjInitCompress();
        if (!compressor) {
            printf("[GDI] TurboJPEG init failed\n");
            return false;
        }

        // Allocate buffers (4K max)
        rgb_buffer = new unsigned char[4096 * 2160 * 3];
        jpeg_buffer = new unsigned char[10 * 1024 * 1024];

        out_width = width;
        out_height = height;

        return true;
    }

    uint8_t* capture(int &out_size) override {
        if (!compressor) {
            return nullptr;
        }

        // Get screen DC
        HDC screen_dc = GetDC(nullptr);
        HDC mem_dc = CreateCompatibleDC(screen_dc);
        HBITMAP bitmap = CreateCompatibleBitmap(screen_dc, width, height);

        if (!bitmap) {
            printf("[GDI] CreateCompatibleBitmap failed\n");
            ReleaseDC(nullptr, screen_dc);
            DeleteDC(mem_dc);
            return nullptr;
        }

        SelectObject(mem_dc, bitmap);
        BitBlt(mem_dc, 0, 0, width, height, screen_dc, 0, 0, SRCCOPY);

        // Get bitmap bits (BGR format from Windows)
        BITMAPINFOHEADER bmi = {0};
        bmi.biSize = sizeof(BITMAPINFOHEADER);
        bmi.biWidth = width;
        bmi.biHeight = -height;  // Negative = top-down
        bmi.biPlanes = 1;
        bmi.biBitCount = 24;
        bmi.biCompression = BI_RGB;

        if (!GetDIBits(mem_dc, bitmap, 0, height, rgb_buffer, reinterpret_cast<BITMAPINFO *>(&bmi), DIB_RGB_COLORS)) {
            printf("[GDI] GetDIBits failed\n");
            DeleteObject(bitmap);
            DeleteDC(mem_dc);
            ReleaseDC(nullptr, screen_dc);
            return nullptr;
        }

        // Encode to JPEG (quality 75)
        unsigned char *jpeg_ptr = jpeg_buffer;
        unsigned long jpeg_size_val = jpeg_size;

        int result = tjCompress2(
            compressor,
            rgb_buffer,
            width,
            0,  // pitch (0 = width * 3)
            height,
            TJPF_BGR,  // Windows gives BGR
            &jpeg_ptr,
            &jpeg_size_val,
            TJSAMP_420,
            75,  // quality
            TJFLAG_FASTDCT
        );

        if (result != 0) {
            printf("[GDI] TurboJPEG compression failed: %s\n", tjGetErrorStr());
            DeleteObject(bitmap);
            DeleteDC(mem_dc);
            ReleaseDC(nullptr, screen_dc);
            return nullptr;
        }

        // Copy JPEG to output buffer
        uint8_t *output = new uint8_t[jpeg_size_val];
        memcpy(output, jpeg_buffer, jpeg_size_val);
        out_size = static_cast<int>(jpeg_size_val);

        // Cleanup
        DeleteObject(bitmap);
        DeleteDC(mem_dc);
        ReleaseDC(nullptr, screen_dc);

        return output;
    }

    void shutdown() override {
        if (jpeg_buffer) {
            delete[] jpeg_buffer;
            jpeg_buffer = nullptr;
        }
        if (rgb_buffer) {
            delete[] rgb_buffer;
            rgb_buffer = nullptr;
        }
        if (compressor) {
            tjDestroy(compressor);
            compressor = nullptr;
        }
    }

private:
    tjhandle compressor = nullptr;
    int width = 0;
    int height = 0;
    unsigned char *rgb_buffer = nullptr;
    unsigned char *jpeg_buffer = nullptr;
    unsigned long jpeg_size = 10 * 1024 * 1024;
};

std::unique_ptr<CaptureBackend> create_gdi_backend() {
    return std::make_unique<GDIBackend>();
}
