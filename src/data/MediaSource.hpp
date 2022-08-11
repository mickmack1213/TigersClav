#pragma once

#include "AVWrapper.hpp"

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <list>
#include <map>
#include <optional>
#include <vector>

class MediaFrame
{
public:
    int64_t timestamp_ns_;

    std::shared_ptr<AVFrameWrapper> pVideoFrame_;

    std::vector<uint8_t> audioData_;
    AVSampleFormat audioFormat_;
    int audioNumSamples_;
    int audioNumChannels_;
};

// TODO: Idea: media source outputs video OR audio sample IN ORDER!

class MediaSource
{
public:
    MediaSource(std::string filename);
    ~MediaSource();

    int64_t getDuration_ns() const;

    void seekTo(int64_t timestamp_ns);
    void seekToNext();
    void seekToPrevious();
    std::optional<MediaFrame> get();

private:
    std::string err2str(int errnum);
    void updateCache();

    bool debug_;

    double lastRequestedTime_;
    double lastCachedTime_;

    AVFormatContext* pFormatContext_;

    AVCodec* pVideoCodec_;
    AVStream* pVideoStream_;
    AVCodecContext* pVideoCodecContext_;

    AVCodec* pAudioCodec_;
    AVStream* pAudioStream_;
    AVCodecContext* pAudioCodecContext_;
};
