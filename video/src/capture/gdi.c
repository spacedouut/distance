#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <turbojpeg.h>
#include "../capture.h"

typedef struct {
    HDC screen_dc;
    HDC mem_dc;
    HBITMAP bitmap;
    tjhandle compressor;
    int width;
    int height;
    unsigned char *rgb_buffer;
    unsigned char *jpeg_buffer;
    unsigned long jpeg_size;
} GDIContext;

static GDIContext gdi_ctx = {0};

static int gdi_init(int monitor, int *out_width, int *out_height) {
    // TODO: Handle multiple monitors
    if (monitor != 0) {
        printf("[GDI] Only primary monitor (0) supported currently\n");
        return -1;
    }
    
    // Get screen dimensions
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    
    printf("[GDI] Screen: %dx%d\n", screen_width, screen_height);
    
    // Initialize TurboJPEG
    gdi_ctx.compressor = tjInitCompress();
    if (!gdi_ctx.compressor) {
        printf("[GDI] TurboJPEG init failed\n");
        return -1;
    }
    
    // Allocate buffers (4K max)
    gdi_ctx.rgb_buffer = malloc(4096 * 2160 * 3);
    gdi_ctx.jpeg_buffer = malloc(10 * 1024 * 1024);
    
    if (!gdi_ctx.rgb_buffer || !gdi_ctx.jpeg_buffer) {
        printf("[GDI] Buffer allocation failed\n");
        if (gdi_ctx.rgb_buffer) free(gdi_ctx.rgb_buffer);
        if (gdi_ctx.jpeg_buffer) free(gdi_ctx.jpeg_buffer);
        tjDestroy(gdi_ctx.compressor);
        return -1;
    }
    
    gdi_ctx.width = screen_width;
    gdi_ctx.height = screen_height;
    
    *out_width = screen_width;
    *out_height = screen_height;
    
    return 0;
}

static uint8_t* gdi_capture(int *out_size) {
    if (!gdi_ctx.compressor) {
        return NULL;
    }
    
    // Get screen DC
    HDC screen_dc = GetDC(NULL);
    HDC mem_dc = CreateCompatibleDC(screen_dc);
    HBITMAP bitmap = CreateCompatibleBitmap(screen_dc, gdi_ctx.width, gdi_ctx.height);
    
    if (!bitmap) {
        printf("[GDI] CreateCompatibleBitmap failed\n");
        ReleaseDC(NULL, screen_dc);
        DeleteDC(mem_dc);
        return NULL;
    }
    
    SelectObject(mem_dc, bitmap);
    BitBlt(mem_dc, 0, 0, gdi_ctx.width, gdi_ctx.height, screen_dc, 0, 0, SRCCOPY);
    
    // Get bitmap bits (BGR format from Windows)
    BITMAPINFOHEADER bmi = {0};
    bmi.biSize = sizeof(BITMAPINFOHEADER);
    bmi.biWidth = gdi_ctx.width;
    bmi.biHeight = -gdi_ctx.height;  // Negative = top-down
    bmi.biPlanes = 1;
    bmi.biBitCount = 24;
    bmi.biCompression = BI_RGB;
    
    if (!GetDIBits(mem_dc, bitmap, 0, gdi_ctx.height, gdi_ctx.rgb_buffer, (BITMAPINFO *)&bmi, DIB_RGB_COLORS)) {
        printf("[GDI] GetDIBits failed\n");
        DeleteObject(bitmap);
        DeleteDC(mem_dc);
        ReleaseDC(NULL, screen_dc);
        return NULL;
    }
    
    // Encode to JPEG (quality 75)
    int result = tjCompress2(
        gdi_ctx.compressor,
        gdi_ctx.rgb_buffer,
        gdi_ctx.width,
        0,  // pitch (0 = width * 3)
        gdi_ctx.height,
        TJPF_BGR,  // Windows gives BGR
        &gdi_ctx.jpeg_buffer,
        &gdi_ctx.jpeg_size,
        TJSAMP_420,
        75,  // quality
        TJFLAG_FASTDCT
    );
    
    if (result != 0) {
        printf("[GDI] TurboJPEG compression failed: %s\n", tjGetErrorStr());
        DeleteObject(bitmap);
        DeleteDC(mem_dc);
        ReleaseDC(NULL, screen_dc);
        return NULL;
    }
    
    // Copy JPEG to output buffer
    uint8_t *output = malloc(gdi_ctx.jpeg_size);
    if (output) {
        memcpy(output, gdi_ctx.jpeg_buffer, gdi_ctx.jpeg_size);
        *out_size = gdi_ctx.jpeg_size;
    }
    
    // Cleanup
    DeleteObject(bitmap);
    DeleteDC(mem_dc);
    ReleaseDC(NULL, screen_dc);
    
    return output;
}

static void gdi_shutdown(void) {
    if (gdi_ctx.jpeg_buffer) {
        tjFree(gdi_ctx.jpeg_buffer);
        gdi_ctx.jpeg_buffer = NULL;
    }
    if (gdi_ctx.rgb_buffer) {
        free(gdi_ctx.rgb_buffer);
        gdi_ctx.rgb_buffer = NULL;
    }
    if (gdi_ctx.compressor) {
        tjDestroy(gdi_ctx.compressor);
        gdi_ctx.compressor = NULL;
    }
}

static int gdi_available(void) {
    // GDI is always available on Windows
    return 1;
}

CaptureBackend capture_gdi = {
    .name = "gdi",
    .init = gdi_init,
    .capture = gdi_capture,
    .shutdown = gdi_shutdown,
    .available = gdi_available,
};
