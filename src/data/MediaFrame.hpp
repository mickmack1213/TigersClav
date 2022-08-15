#pragma once

#include "AVWrapper.hpp"
#include <memory>
#include <vector>

struct MediaFrame
{
    std::shared_ptr<AVFrameWrapper> pImage;
    AVRational videoTimeBase;
    AVRational videoFramerate;
    enum AVCodecID videoCodec;
    int64_t videoBitRate;

    std::shared_ptr<AVFrameWrapper> pSamples;
    AVRational audioTimeBase;
    enum AVCodecID audioCodec;
    int64_t audioBitRate;
};
