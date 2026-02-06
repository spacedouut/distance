#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <turbojpeg.h>
#include <wrl/client.h>
#include "../capture.hpp"

using Microsoft::WRL::ComPtr;

class DXGIBackend : public CaptureBackend {
public:
    DXGIBackend() = default;
    ~DXGIBackend() override { shutdown(); }

    const char* get_name() const override {
        return "dxgi";
    }

    bool is_available() const override {
        // Check if DXGI 1.2+ is available (Windows 8+)
        ComPtr<ID3D11Device> test_device;
        ComPtr<ID3D11DeviceContext> test_context;

        D3D_FEATURE_LEVEL feature_level;
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &test_device,
            &feature_level,
            &test_context
        );

        return SUCCEEDED(hr);
    }

    bool init(int monitor, int &out_width, int &out_height) override {
        HRESULT hr;

        // Create D3D11 device
        D3D_FEATURE_LEVEL feature_level;
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &d3d_device,
            &feature_level,
            &d3d_context
        );

        if (FAILED(hr)) {
            printf("[DXGI] Failed to create D3D11 device: 0x%lx\n", hr);
            return false;
        }

        // Get DXGI device
        ComPtr<IDXGIDevice> dxgi_device;
        hr = d3d_device.As(&dxgi_device);
        if (FAILED(hr)) {
            printf("[DXGI] Failed to get DXGI device: 0x%lx\n", hr);
            return false;
        }

        // Get DXGI adapter
        ComPtr<IDXGIAdapter> dxgi_adapter;
        hr = dxgi_device->GetAdapter(&dxgi_adapter);
        if (FAILED(hr)) {
            printf("[DXGI] Failed to get DXGI adapter: 0x%lx\n", hr);
            return false;
        }

        // Enumerate outputs to find the requested monitor
        ComPtr<IDXGIOutput> dxgi_output;
        hr = dxgi_adapter->EnumOutputs(monitor, &dxgi_output);
        if (FAILED(hr)) {
            printf("[DXGI] Failed to enumerate output %d: 0x%lx\n", monitor, hr);
            return false;
        }

        // Get output description
        DXGI_OUTPUT_DESC output_desc;
        hr = dxgi_output->GetDesc(&output_desc);
        if (FAILED(hr)) {
            printf("[DXGI] Failed to get output description: 0x%lx\n", hr);
            return false;
        }

        width = output_desc.DesktopCoordinates.right - output_desc.DesktopCoordinates.left;
        height = output_desc.DesktopCoordinates.bottom - output_desc.DesktopCoordinates.top;

        printf("[DXGI] Monitor %d: %dx%d\n", monitor, width, height);

        // Get IDXGIOutput1 interface
        ComPtr<IDXGIOutput1> dxgi_output1;
        hr = dxgi_output.As(&dxgi_output1);
        if (FAILED(hr)) {
            printf("[DXGI] Failed to get IDXGIOutput1: 0x%lx\n", hr);
            return false;
        }

        // Create desktop duplication
        hr = dxgi_output1->DuplicateOutput(d3d_device.Get(), &duplication);
        if (FAILED(hr)) {
            printf("[DXGI] Failed to create desktop duplication: 0x%lx\n", hr);
            return false;
        }

        // Create staging texture
        D3D11_TEXTURE2D_DESC tex_desc = {};
        tex_desc.Width = width;
        tex_desc.Height = height;
        tex_desc.MipLevels = 1;
        tex_desc.ArraySize = 1;
        tex_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.Usage = D3D11_USAGE_STAGING;
        tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        hr = d3d_device->CreateTexture2D(&tex_desc, nullptr, &staging_texture);
        if (FAILED(hr)) {
            printf("[DXGI] Failed to create staging texture: 0x%lx\n", hr);
            return false;
        }

        // Initialize TurboJPEG
        compressor = tjInitCompress();
        if (!compressor) {
            printf("[DXGI] TurboJPEG init failed\n");
            return false;
        }

        // Allocate buffer
        jpeg_buffer = new unsigned char[10 * 1024 * 1024];

        out_width = width;
        out_height = height;

        return true;
    }

    uint8_t* capture(int &out_size) override {
        if (!duplication || !compressor) {
            return nullptr;
        }

        HRESULT hr;
        ComPtr<IDXGIResource> desktop_resource;
        DXGI_OUTDUPL_FRAME_INFO frame_info;

        // Acquire next frame
        hr = duplication->AcquireNextFrame(100, &frame_info, &desktop_resource);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            // No new frame, try again
            return nullptr;
        }
        if (FAILED(hr)) {
            printf("[DXGI] Failed to acquire next frame: 0x%lx\n", hr);
            return nullptr;
        }

        // Get texture from resource
        ComPtr<ID3D11Texture2D> desktop_texture;
        hr = desktop_resource.As(&desktop_texture);
        if (FAILED(hr)) {
            printf("[DXGI] Failed to get texture from resource: 0x%lx\n", hr);
            duplication->ReleaseFrame();
            return nullptr;
        }

        // Copy to staging texture
        d3d_context->CopyResource(staging_texture.Get(), desktop_texture.Get());

        // Map staging texture
        D3D11_MAPPED_SUBRESOURCE mapped;
        hr = d3d_context->Map(staging_texture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) {
            printf("[DXGI] Failed to map staging texture: 0x%lx\n", hr);
            duplication->ReleaseFrame();
            return nullptr;
        }

        // Encode to JPEG
        unsigned char *jpeg_ptr = jpeg_buffer;
        unsigned long jpeg_size_val = jpeg_size;

        int result = tjCompress2(
            compressor,
            static_cast<unsigned char*>(mapped.pData),
            width,
            mapped.RowPitch,
            height,
            TJPF_BGRA,  // DXGI gives BGRA
            &jpeg_ptr,
            &jpeg_size_val,
            TJSAMP_420,
            75,  // quality
            TJFLAG_FASTDCT
        );

        // Unmap staging texture
        d3d_context->Unmap(staging_texture.Get(), 0);

        // Release frame
        duplication->ReleaseFrame();

        if (result != 0) {
            printf("[DXGI] TurboJPEG compression failed: %s\n", tjGetErrorStr());
            return nullptr;
        }

        // Copy JPEG to output buffer
        uint8_t *output = new uint8_t[jpeg_size_val];
        memcpy(output, jpeg_buffer, jpeg_size_val);
        out_size = static_cast<int>(jpeg_size_val);

        return output;
    }

    void shutdown() override {
        if (jpeg_buffer) {
            delete[] jpeg_buffer;
            jpeg_buffer = nullptr;
        }
        if (compressor) {
            tjDestroy(compressor);
            compressor = nullptr;
        }

        staging_texture.Reset();
        duplication.Reset();
        d3d_context.Reset();
        d3d_device.Reset();
    }

private:
    ComPtr<ID3D11Device> d3d_device;
    ComPtr<ID3D11DeviceContext> d3d_context;
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<ID3D11Texture2D> staging_texture;

    tjhandle compressor = nullptr;
    int width = 0;
    int height = 0;
    unsigned char *jpeg_buffer = nullptr;
    unsigned long jpeg_size = 10 * 1024 * 1024;
};

std::unique_ptr<CaptureBackend> create_dxgi_backend() {
    return std::make_unique<DXGIBackend>();
}
