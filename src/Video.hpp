#pragma once

extern "C" {
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <list>

class Video
{
public:
    Video(std::string filename);
    ~Video();

    bool isLoaded() const { return isLoaded_; }
    AVFrame* getFrame(int64_t timestamp);

    enum AVPixelFormat internalGetHwFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);

private:
    void preloader();
    void loadFrames(int32_t remainingFrames);
    std::string err2str(int errnum);

    void runBenchmark();

    std::thread preloaderThread_;
    std::atomic<bool> runPreloaderThread_;

    std::mutex preloadMutex_;
    std::condition_variable preloadCondition_;

    int32_t cacheSize_;
    std::list<AVFrame*> cachedFrames_;
    std::atomic<int64_t> requestedPts_;


    bool isLoaded_;

    AVFormatContext* pFormatContext_;
    AVCodec* pVideoCodec_;
    AVStream* pVideoStream_;
    AVCodecContext* pVideoCodecContext_;
    enum AVPixelFormat hwPixFormat_;
    AVBufferRef *pHwDeviceContext_;

    AVFrame* pFrame_;
    AVPacket* pPacket_;

    AVCodec* pAudioCodec_;
    AVStream* pAudioStream_;
    AVCodecContext* pAudioCodecContext_;
};
