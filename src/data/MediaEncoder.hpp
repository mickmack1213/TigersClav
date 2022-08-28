#pragma once

extern "C" {
#include "libswresample/swresample.h"
}

#include "MediaFrame.hpp"

#include <string>

class MediaEncoder
{
public:
    MediaEncoder(std::string filename);
    ~MediaEncoder();

    int put(std::shared_ptr<const MediaFrame> pFrame);
    void close();

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
    AVFrame* pVideoEncFrame_;

    AVStream* pAudioStream_;
    AVCodec* pAudioCodec_;
    AVCodecContext* pAudioCodecContext_;

    SwrContext* pResampler_;
};
