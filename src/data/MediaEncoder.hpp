#pragma once

extern "C" {
#include "libswresample/swresample.h"
}

#include "MediaFrame.hpp"

#include <string>

class MediaEncoder
{
public:
    struct Timing
    {
        float copy;
        float send;
        float receive;
        float write;
    };

    MediaEncoder(std::string filename, bool useHwEncoder = false);
    ~MediaEncoder();

    int put(std::shared_ptr<const MediaFrame> pFrame);
    void close();

    Timing getVideoTiming() const { return videoTiming_; }
    Timing getAudioTiming() const { return audioTiming_; }

private:
    bool initialize(std::shared_ptr<const MediaFrame> pFrame);
    std::string err2str(int errnum);

    bool debug_;
    std::string filename_;
    bool initialized_;

    int64_t curVideoPts_;
    int64_t curAudioPts_;

    AVFormatContext* pFormatContext_;

    AVStream* pVideoStream_;
    AVCodec* pVideoCodec_;
    AVCodecContext* pVideoCodecContext_;

    AVStream* pAudioStream_;
    AVCodec* pAudioCodec_;
    AVCodecContext* pAudioCodecContext_;

    SwrContext* pResampler_;

    bool useHwEncoder_;

    Timing videoTiming_;
    Timing audioTiming_;
};
