// macOS screen capture backend using ScreenCaptureKit (macOS 12.3+)
// IPC: Unix domain socket at /tmp/distance_video.sock
// Wire format: 4-byte big-endian frame length + raw JPEG bytes

#ifdef __APPLE__

#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>
#include <turbojpeg.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <signal.h>
#include <cstdio>
#include <atomic>
#include <memory>

#include "../capture.hpp"

#define UNIX_SOCKET_PATH "/tmp/distance_video.sock"

// ---------------------------------------------------------------------------
// FrameSender: holds the Unix socket fd and TurboJPEG compressor.
// Called from SCStream's sample buffer callback on a background queue.
// ---------------------------------------------------------------------------
@interface FrameSender : NSObject <SCStreamOutput>
@property (nonatomic, assign) int clientFd;
@property (nonatomic, assign) tjhandle compressor;
@property (nonatomic, assign) int quality;
@property (nonatomic, assign) BOOL verbose;
@end

@implementation FrameSender

- (void)stream:(SCStream *)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type
{
    if (type != SCStreamOutputTypeScreen) return;
    if (self.clientFd < 0) return;

    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imageBuffer) return;

    CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

    size_t width  = CVPixelBufferGetWidth(imageBuffer);
    size_t height = CVPixelBufferGetHeight(imageBuffer);
    size_t stride = CVPixelBufferGetBytesPerRow(imageBuffer);
    uint8_t *data = (uint8_t *)CVPixelBufferGetBaseAddress(imageBuffer);

    unsigned char *jpegBuf = NULL;
    unsigned long jpegSize = 0;

    int rc = tjCompress2(
        self.compressor,
        data,
        (int)width, (int)stride, (int)height,
        TJPF_BGRA,
        &jpegBuf, &jpegSize,
        TJSAMP_420, self.quality,
        TJFLAG_FASTDCT
    );

    CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

    if (rc != 0 || !jpegBuf) {
        if (self.verbose) printf("[MACOS] TurboJPEG error: %s\n", tjGetErrorStr());
        return;
    }

    // Send: [4-byte big-endian length][jpeg bytes]
    uint32_t lenBE = htonl((uint32_t)jpegSize);
    ssize_t sent = send(self.clientFd, &lenBE, 4, 0);
    if (sent == 4) {
        send(self.clientFd, jpegBuf, jpegSize, 0);
    }

    tjFree(jpegBuf);
}

@end

// ---------------------------------------------------------------------------
// macOS CaptureBackend implementation
// ---------------------------------------------------------------------------
class MacOSBackend : public CaptureBackend {
public:
    MacOSBackend() = default;
    ~MacOSBackend() override { shutdown(); }

    const char* get_name() const override { return "macos"; }

    bool is_available() const override {
        // ScreenCaptureKit requires macOS 12.3+
        if (@available(macOS 12.3, *)) {
            return true;
        }
        return false;
    }

    // init() creates the socket server and waits for the agent to connect,
    // then starts the SCStream. Dimensions are set from the display.
    bool init(int monitor, int &out_width, int &out_height) override {
        if (!is_available()) {
            printf("[MACOS] ScreenCaptureKit not available (requires macOS 12.3+)\n");
            return false;
        }

        // Suppress SIGPIPE so a disconnected agent doesn't crash us
        signal(SIGPIPE, SIG_IGN);

        // Create Unix domain socket server
        serverFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (serverFd_ < 0) { perror("[MACOS] socket"); return false; }

        int opt = 1;
        setsockopt(serverFd_, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));

        struct sockaddr_un addr = {};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, UNIX_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        unlink(UNIX_SOCKET_PATH);

        if (bind(serverFd_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("[MACOS] bind"); return false;
        }
        if (listen(serverFd_, 1) < 0) {
            perror("[MACOS] listen"); return false;
        }

        printf("[MACOS] Waiting for agent on %s ...\n", UNIX_SOCKET_PATH);
        clientFd_ = accept(serverFd_, NULL, NULL);
        if (clientFd_ < 0) { perror("[MACOS] accept"); return false; }

        opt = 1;
        setsockopt(clientFd_, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
        printf("[MACOS] Agent connected\n");

        // Get shareable content and start stream
        __block bool success = false;
        __block int bWidth = 0, bHeight = 0;
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);

        if (@available(macOS 12.3, *)) {
            [SCShareableContent
                getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
                    if (error || !content || content.displays.count == 0) {
                        printf("[MACOS] SCShareableContent error: %s\n",
                               error ? [[error localizedDescription] UTF8String] : "no displays");
                        dispatch_semaphore_signal(sem);
                        return;
                    }

                    NSUInteger idx = (NSUInteger)monitor_;
                    if (idx >= content.displays.count) idx = 0;
                    SCDisplay *display = content.displays[idx];

                    bWidth  = (int)display.width;
                    bHeight = (int)display.height;

                    SCContentFilter *filter =
                        [[SCContentFilter alloc] initWithDisplay:display
                                                excludingWindows:@[]];

                    SCStreamConfiguration *cfg = [[SCStreamConfiguration alloc] init];
                    cfg.width       = (size_t)bWidth;
                    cfg.height      = (size_t)bHeight;
                    cfg.pixelFormat = kCVPixelFormatType_32BGRA;
                    cfg.showsCursor = YES;
                    cfg.minimumFrameInterval = CMTimeMake(1, fps_);

                    sender_ = [[FrameSender alloc] init];
                    sender_.clientFd   = clientFd_;
                    sender_.compressor = tjInitCompress();
                    sender_.quality    = quality_;
                    sender_.verbose    = verbose_;

                    stream_ = [[SCStream alloc] initWithFilter:filter
                                                 configuration:cfg
                                                      delegate:nil];

                    NSError *addErr = nil;
                    BOOL added = [stream_
                        addStreamOutput:sender_
                                   type:SCStreamOutputTypeScreen
                     sampleHandlerQueue:dispatch_get_global_queue(
                                          DISPATCH_QUEUE_PRIORITY_HIGH, 0)
                                  error:&addErr];
                    if (!added) {
                        printf("[MACOS] addStreamOutput failed: %s\n",
                               [[addErr localizedDescription] UTF8String]);
                        dispatch_semaphore_signal(sem);
                        return;
                    }

                    [stream_ startCaptureWithCompletionHandler:^(NSError *startErr) {
                        if (startErr) {
                            printf("[MACOS] startCapture failed: %s\n",
                                   [[startErr localizedDescription] UTF8String]);
                        } else {
                            success = true;
                            printf("[MACOS] Capture started: %dx%d @ %dfps\n",
                                   bWidth, bHeight, fps_);
                        }
                        dispatch_semaphore_signal(sem);
                    }];
                }];
        }

        dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 10LL * NSEC_PER_SEC));

        if (!success) {
            printf("[MACOS] Failed to start capture. Check Screen Recording permission in\n"
                   "        System Settings > Privacy & Security > Screen Recording\n");
            return false;
        }

        captureWidth_  = bWidth;
        captureHeight_ = bHeight;
        out_width  = bWidth;
        out_height = bHeight;
        initialized_ = true;
        return true;
    }

    // capture() is not used in the macOS backend — frames are pushed to the
    // socket directly from the SCStream callback. Return nullptr to signal
    // that main.cpp should use the push model for this backend.
    uint8_t* capture(int &out_size) override {
        // SCStream pushes frames asynchronously; nothing to poll here.
        // Sleep to avoid busy-spinning in main.cpp's capture loop.
        usleep(10000);  // 10ms
        return nullptr;
    }

    void shutdown() override {
        if (stream_) {
            [stream_ stopCaptureWithCompletionHandler:^(NSError *) {}];
            stream_ = nil;
        }
        if (sender_) {
            if (sender_.compressor) {
                tjDestroy(sender_.compressor);
                sender_.compressor = nullptr;
            }
            sender_ = nil;
        }
        if (clientFd_ >= 0) { close(clientFd_); clientFd_ = -1; }
        if (serverFd_ >= 0) { close(serverFd_); serverFd_ = -1; }
        unlink(UNIX_SOCKET_PATH);
        initialized_ = false;
    }

    // Called by main.cpp to set config before init()
    void set_config(int fps, int quality, int monitor, bool verbose) {
        fps_     = fps;
        quality_ = quality;
        monitor_ = monitor;
        verbose_ = verbose;
    }

private:
    int serverFd_      = -1;
    int clientFd_      = -1;
    int captureWidth_  = 0;
    int captureHeight_ = 0;
    int fps_           = 30;
    int quality_       = 75;
    int monitor_       = 0;
    bool verbose_      = false;
    bool initialized_  = false;

    SCStream    *stream_ = nil;
    FrameSender *sender_ = nil;
};

std::unique_ptr<CaptureBackend> create_macos_backend() {
    return std::make_unique<MacOSBackend>();
}

#endif // __APPLE__
