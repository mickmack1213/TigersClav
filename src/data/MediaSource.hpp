#pragma once

#include "MediaFrame.hpp"

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <list>
#include <map>
#include <vector>

struct MediaCachedDuration
{
    double audio_s[2];
    double video_s[2];
};

class MediaSource
{
public:
    MediaSource(std::string filename, bool useHwDecoder = false, std::string hwDecoder = "");
    ~MediaSource();

    bool isLoaded() const { return isLoaded_; }
    std::string getFilename() const { return filename_; }
    bool hasReachedEndOfFile() const { return reachedEndOfFile_; }

    double getDuration_s() const;
    double getFrameDeltaTime() const { return videoFrameDeltaTime_s_; }

    void seekTo(double time_s);
    void seekToNext();
    void seekToPrevious();
    std::shared_ptr<MediaFrame> get();
    double tell() const { return lastRequestTime_s_; }

    MediaCachedDuration getCachedDuration() const;

    std::list<std::string> getFileDetails() const;

    double videoPtsToSeconds(int64_t pts) const;
    int64_t videoSecondsToPts(double seconds) const;
    double audioPtsToSeconds(int64_t pts) const;
    int64_t audioSecondsToPts(double seconds) const;

    enum AVPixelFormat internalGetHwFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);

private:
    std::string err2str(int errnum);

    void preloader();
    void updateCache(double requestTime_s);
    void fillCache(double tFirst_s, double tLast_s);
    void cleanCache(double tOld_s);

    std::shared_ptr<AVFrameWrapper> processVideoFrame(AVPacket* pPacket);
    std::shared_ptr<AVFrameWrapper> processAudioFrame(AVPacket* pPacket);

    bool debug_;
    bool isLoaded_;
    std::string filename_;

    AVFormatContext* pFormatContext_;

    const AVCodec* pVideoCodec_;
    AVStream* pVideoStream_;
    AVCodecContext* pVideoCodecContext_;
    enum AVPixelFormat hwPixFormat_;
    AVBufferRef *pHwDeviceContext_;

    const AVCodec* pAudioCodec_;
    AVStream* pAudioStream_;
    AVCodecContext* pAudioCodecContext_;

    std::atomic<double> lastRequestTime_s_;
    bool reachedEndOfFile_;

    std::mutex videoSamplesMutex_;
    std::map<int64_t, std::shared_ptr<AVFrameWrapper>> videoSamples_;

    std::mutex audioSamplesMutex_;
    std::map<int64_t, std::shared_ptr<AVFrameWrapper>> audioSamples_;

    int64_t audioPtsInc_;
    int64_t videoPtsInc_;
    double videoFrameDeltaTime_s_;

    std::thread preloaderThread_;
    std::atomic<bool> runPreloaderThread_;
};
