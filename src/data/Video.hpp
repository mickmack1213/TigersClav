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
#include <map>

class Video
{
public:
    struct CacheLevels
    {
        float before;
        float after;
    };

    Video(std::string filename);
    ~Video();

    bool isLoaded() const { return isLoaded_; }
    AVFrame* getFrame(int64_t frameId);
    int32_t getLastFrameId() const;
    float getFrameDeltaTime() const { return frameDeltaTime_s_; }

    int64_t getDuration_ns() const;
    AVFrame* getFrameByTime(int64_t timestamp_ns);

    std::string getFilename() const { return filename_; }

    std::list<std::string> getFileDetails() const;

    CacheLevels getCacheLevels() const { return CacheLevels{ cacheLevelBefore_, cacheLevelAfter_}; }

    enum AVPixelFormat internalGetHwFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);

private:
    class AVFrameWrapper
    {
    public:
        AVFrameWrapper()
        {
            pFrame = av_frame_alloc();
        }

        ~AVFrameWrapper()
        {
            av_frame_free(&pFrame);
        }

        AVFrameWrapper(const AVFrameWrapper&) = delete;
        AVFrameWrapper& operator=(const AVFrameWrapper&) = delete;

        AVFrame* pFrame;
    };

    class AVPacketWrapper
    {
    public:
        AVPacketWrapper()
        {
            pPacket = av_packet_alloc();
        }

        ~AVPacketWrapper()
        {
            av_packet_free(&pPacket);
        }

        AVPacketWrapper(const AVPacketWrapper&) = delete;
        AVPacketWrapper& operator=(const AVPacketWrapper&) = delete;

        AVPacket* pPacket;
    };

    void preloader();
    std::shared_ptr<AVFrameWrapper> readNextFrame();
    std::string err2str(int errnum);

    void runBenchmark();

    std::thread preloaderThread_;
    std::atomic<bool> runPreloaderThread_;

    std::mutex preloadMutex_;
    std::condition_variable preloadCondition_;

    int32_t cacheSize_;
    std::map<int64_t, std::shared_ptr<AVFrameWrapper>> cachedFrames_;
    std::atomic<int64_t> requestedPts_;

    std::atomic<float> cacheLevelBefore_;
    std::atomic<float> cacheLevelAfter_;

    int64_t videoPtsIncrement_;
    float frameDeltaTime_s_;

    bool isLoaded_;
    std::string filename_;

    bool debug_;

    AVFormatContext* pFormatContext_;
    AVCodec* pVideoCodec_;
    AVStream* pVideoStream_;
    AVCodecContext* pVideoCodecContext_;
    enum AVPixelFormat hwPixFormat_;
    AVBufferRef *pHwDeviceContext_;

    AVCodec* pAudioCodec_;
    AVStream* pAudioStream_;
    AVCodecContext* pAudioCodecContext_;
};
