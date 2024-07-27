#pragma once

extern "C" {
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

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

    operator AVPacket*() { return pPacket; }
    AVPacket* operator->() { return pPacket; }
    AVPacket& operator*() { return *pPacket; }

    operator bool() { return pPacket != 0; }

private:
    AVPacket* pPacket;
};

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

    operator AVFrame*() { return pFrame; }
    AVFrame* operator->() { return pFrame; }
    AVFrame& operator*() { return *pFrame; }

private:
    AVFrame* pFrame;
};
