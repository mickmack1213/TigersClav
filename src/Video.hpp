#pragma once

extern "C" {
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <string>

class Video
{
public:
    Video(std::string filename);
    ~Video();

    bool isLoaded() const { return isLoaded_; }
    AVFrame* getFrame(int64_t timestamp);

private:
    std::string err2str(int errnum);

    bool isLoaded_;

    AVFormatContext* pFormatContext_;
    AVCodec* pCodec_;
    AVStream* pVideoStream_;
    AVCodecContext* pCodecContext_;
    AVFrame* pFrame_;
    AVPacket* pPacket_;

    int64_t lastGetFrameTimestamp_;
};
