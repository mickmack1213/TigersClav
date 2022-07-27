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

private:
    std::string err2str(int errnum);

    bool isLoaded_;

    AVFormatContext* pFormatContext_;
    AVCodec* pCodec_;
    AVCodecContext* pCodecContext_;
    AVFrame* pFrame_;
    AVPacket* pPacket_;
};
