#include "Video.hpp"
#include "util/easylogging++.h"
#include <iomanip>

static enum AVPixelFormat getHwFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    return reinterpret_cast<Video*>(ctx->opaque)->internalGetHwFormat(ctx, pix_fmts);
}

enum AVPixelFormat Video::internalGetHwFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hwPixFormat_)
            return *p;
    }

    LOG(ERROR) << "Failed to get HW surface format.";
    return AV_PIX_FMT_NONE;
}

Video::Video(std::string filename)
:isLoaded_(false),
 filename_(filename),
 pFormatContext_(0),
 pVideoCodec_(0),
 pVideoStream_(0),
 pVideoCodecContext_(0),
 pAudioCodec_(0),
 pAudioStream_(0),
 pAudioCodecContext_(0),
 pHwDeviceContext_(0),
 runPreloaderThread_(true),
 cacheSize_(50),
 frameDeltaTime_s_(1.0f),
 debug_(false)
{
    int result;

    // TESTING
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;

    while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
        LOG(INFO) << "AV_HWDEVICE support: " << av_hwdevice_get_type_name(type);

    // TESTING END

    LOG(INFO) << "Trying to load video: " << filename;

    // Open container format
    pFormatContext_ = avformat_alloc_context();
    if(!pFormatContext_)
    {
        LOG(ERROR) << "No memory for AVFormatContext";
        return;
    }

    result = avformat_open_input(&pFormatContext_, filename.c_str(), NULL, NULL);
    if(result)
    {
        LOG(ERROR) << "Could not open video file: " << filename << ", result: " << err2str(result);
        return;
    }

    LOG(INFO) << "Video format " << pFormatContext_->iformat->name << ", duration " << pFormatContext_->duration << "us";

    // Find best video stream and setup codec
    int streamNumber = av_find_best_stream(pFormatContext_, AVMEDIA_TYPE_VIDEO, -1, -1, &pVideoCodec_, 0);
    if(streamNumber < 0)
    {
        LOG(ERROR) << "Could retrieve video stream. Result: " << err2str(streamNumber);
        return;
    }

    pVideoStream_ = pFormatContext_->streams[streamNumber];
    AVCodecParameters* pVideoCodecPars = pVideoStream_->codecpar;

    float frameRate = (float)pVideoStream_->r_frame_rate.num / pVideoStream_->r_frame_rate.den;
    float duration_s = (float)pVideoStream_->duration * pVideoStream_->time_base.num / pVideoStream_->time_base.den;
    frameDeltaTime_s_ = 1.0f/frameRate;

    LOG(INFO) << "Codec: " << pVideoCodec_->name << ", bitrate: " << pVideoCodecPars->bit_rate;
    LOG(INFO) << "Duration: " << std::setprecision(6) << duration_s << " (" << pVideoStream_->duration << "), startTime: " << pVideoStream_->start_time << "pts";
    LOG(INFO) << "Time base: " << pVideoStream_->time_base.num << "/" << pVideoStream_->time_base.den << ", framerate: " << pVideoStream_->r_frame_rate.num << "/" << pVideoStream_->r_frame_rate.den;
    LOG(INFO) << "Video resolution: " << pVideoCodecPars->width << "x" << pVideoCodecPars->height << " @ " << std::setprecision(4) << frameRate << "fps";

    videoPtsIncrement_ = pVideoStream_->time_base.den * pVideoStream_->r_frame_rate.den / (pVideoStream_->time_base.num * pVideoStream_->r_frame_rate.num);

    LOG(INFO) << "PTS inc: " << videoPtsIncrement_ << ", last frame: " << pVideoStream_->duration / videoPtsIncrement_;

    int dictEntries = av_dict_count(pVideoStream_->metadata);

    LOG(INFO) << "Dict entries: " << dictEntries;

    AVDictionaryEntry *pDictEntry = NULL;
    while((pDictEntry = av_dict_get(pVideoStream_->metadata, "", pDictEntry, AV_DICT_IGNORE_SUFFIX)) != 0)
    {
        LOG(INFO) << "    " << pDictEntry->key << " = " << pDictEntry->value;
    }

/*    const AVCodecHWConfig* pVideoHWConfig = 0;

    int index = 0;
    while(1)
    {
        const AVCodecHWConfig* pConfig = avcodec_get_hw_config(pVideoCodec_, index);
        if(!pConfig)
            break;

        if(pConfig->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
        {
            LOG(INFO) << "[" << index << "] Supported HW: " << av_hwdevice_get_type_name(pConfig->device_type) << ", pixfmt: " << pConfig->pix_fmt;
        }

        pVideoHWConfig = pConfig; // TODO: we will just use the last config for now

        index++;
    }

    if(!pVideoHWConfig)
    {
        LOG(ERROR) << "No hardware decoding support available";
        return;
    }

    LOG(INFO) << "Selected HW: " << av_hwdevice_get_type_name(pVideoHWConfig->device_type);
*/
    pVideoCodecContext_ = avcodec_alloc_context3(pVideoCodec_);
    if(!pVideoCodecContext_)
    {
        LOG(ERROR) << "Video: No memory for AVCodecContext";
        return;
    }

    result = avcodec_parameters_to_context(pVideoCodecContext_, pVideoCodecPars);
    if(result < 0)
    {
        LOG(ERROR) << "Video: Failed to copy codec parameters to codec context. Result: " << err2str(result);
        return;
    }
/*
    // TODO: new HW stuff
    hwPixFormat_ = pVideoHWConfig->pix_fmt;
    pVideoCodecContext_->opaque = this;
    pVideoCodecContext_->get_format = &getHwFormat;

    result = av_hwdevice_ctx_create(&pHwDeviceContext_, pVideoHWConfig->device_type, NULL, NULL, 0);
    if(result < 0)
    {
        LOG(ERROR) << "Failed to create specified HW device.";
        return;
    }

    pVideoCodecContext_->hw_device_ctx = av_buffer_ref(pHwDeviceContext_);
    // END new HW stuff
*/
    result = avcodec_open2(pVideoCodecContext_, pVideoCodec_, NULL);
    if(result < 0)
    {
        LOG(ERROR) << "Video: Failed to open codec through avcodec_open2. Result: " << err2str(result);
        return;
    }

    // Find best audio stream and setup codec
    streamNumber = av_find_best_stream(pFormatContext_, AVMEDIA_TYPE_AUDIO, -1, -1, &pAudioCodec_, 0);
    if(streamNumber < 0)
    {
        LOG(ERROR) << "Could retrieve audio stream. Result: " << err2str(streamNumber);
        return;
    }

    pAudioStream_ = pFormatContext_->streams[streamNumber];
    AVCodecParameters* pAudioCodecPars = pAudioStream_->codecpar;

    duration_s = (float)pAudioStream_->duration * pAudioStream_->time_base.num / pAudioStream_->time_base.den;

    LOG(INFO) << "Audio codec: " << pAudioCodec_->name << ", bitrate: " << pAudioCodecPars->bit_rate;
    LOG(INFO) << "Channels: " << pAudioCodecPars->channels << ", layout: 0x" << std::hex << pAudioCodecPars->channel_layout << std::dec << ", sample rate: " << pAudioCodecPars->sample_rate;
    LOG(INFO) << "Duration: " << std::setprecision(6) << duration_s << ", startTime: " << pAudioStream_->start_time << "pts";
    LOG(INFO) << "Time base: " << pAudioStream_->time_base.num << "/" << pAudioStream_->time_base.den;

    pAudioCodecContext_ = avcodec_alloc_context3(pAudioCodec_);
    if(!pAudioCodecContext_)
    {
        LOG(ERROR) << "Audio: No memory for AVCodecContext";
        return;
    }

    result = avcodec_parameters_to_context(pAudioCodecContext_, pAudioCodecPars);
    if(result < 0)
    {
        LOG(ERROR) << "Audio: Failed to copy codec parameters to codec context. Result: " << err2str(result);
        return;
    }

    result = avcodec_open2(pAudioCodecContext_, pAudioCodec_, NULL);
    if(result < 0)
    {
        LOG(ERROR) << "Audio: Failed to open codec through avcodec_open2. Result: " << err2str(result);
        return;
    }

//    runBenchmark();

    preloaderThread_ = std::thread(preloader, this);

    isLoaded_ = true;
}

Video::~Video()
{
    runPreloaderThread_ = false;
    preloadCondition_.notify_one();

    if(preloaderThread_.joinable())
        preloaderThread_.join();

    if(pFormatContext_)
        avformat_close_input(&pFormatContext_);

    if(pVideoCodecContext_)
        avcodec_free_context(&pVideoCodecContext_);

    if(pAudioCodecContext_)
        avcodec_free_context(&pAudioCodecContext_);

    if(pHwDeviceContext_)
        av_buffer_unref(&pHwDeviceContext_);
}

std::list<std::string> Video::getFileDetails() const
{
    std::list<std::string> details;

    if(!isLoaded_)
        return details;

    std::stringstream ss;

    float frameRate = (float)pVideoStream_->r_frame_rate.num / pVideoStream_->r_frame_rate.den;
    float duration_s = (float)pVideoStream_->duration * pVideoStream_->time_base.num / pVideoStream_->time_base.den;

    ss << "Duration: " << std::setprecision(6) << duration_s << "s";
    details.push_back(ss.str());
    ss.str("");
    ss.clear();

    ss << "Video resolution: " << pVideoStream_->codecpar->width << "x" << pVideoStream_->codecpar->height << " @ " << std::fixed << std::setprecision(2) << frameRate << "fps";
    details.push_back(ss.str());
    ss.str("");
    ss.clear();

    ss << "Video Codec: " << pVideoCodec_->name << ", bitrate: " << std::fixed << std::setprecision(2) << pVideoStream_->codecpar->bit_rate * 1e-6 << "MBit/s";
    details.push_back(ss.str());
    ss.str("");
    ss.clear();

    return details;
}

int32_t Video::getLastFrameId() const
{
    if(!isLoaded_)
        return 0;

    return pVideoStream_->duration / videoPtsIncrement_ - 1;
}

int64_t Video::getDuration_ns() const
{
    if(!isLoaded_)
        return 0;

    return (pVideoStream_->duration * 1000000000LL * pVideoStream_->time_base.num) / pVideoStream_->time_base.den;
}

AVFrame* Video::getFrameByTime(int64_t timestamp_ns)
{
    if(!isLoaded_)
        return 0;

    if(timestamp_ns < 0 || timestamp_ns >= getDuration_ns())
        return 0;

    const int64_t num = pVideoStream_->r_frame_rate.num;
    const int64_t den = pVideoStream_->r_frame_rate.den;

    int64_t frameId = (timestamp_ns * num) / (den * 1000000000LL);

    return getFrame(frameId);
}

AVFrame* Video::getFrame(int64_t frameId)
{
    int result;

    if(!isLoaded_)
        return 0;

    requestedPts_ = frameId * videoPtsIncrement_;
    preloadCondition_.notify_one();

    const auto& iterRequested = cachedFrames_.lower_bound(requestedPts_.load());

    if(iterRequested != cachedFrames_.end())
    {
        return iterRequested->second->pFrame;
    }

    return 0;
}

void Video::preloader()
{
    int result;

    while(runPreloaderThread_)
    {
        std::unique_lock<std::mutex> lock(preloadMutex_);
        preloadCondition_.wait(lock);

        if(!runPreloaderThread_)
            return;

        int64_t requestedPts = requestedPts_;

        int64_t minPts = 0;
        int64_t maxPts = 0;

        if(!cachedFrames_.empty())
        {
            minPts = cachedFrames_.begin()->first;
            maxPts = cachedFrames_.rbegin()->first;
        }

        if(requestedPts < 0 || requestedPts >= pVideoStream_->duration)
        {
            // PTS outside valid range, do nothing
        }
        else if(cachedFrames_.empty() || requestedPts < minPts || requestedPts > maxPts)
        {
            if(debug_)
            {
                LOG(INFO) << "REQ: " << requestedPts/videoPtsIncrement_ << ", min: " << minPts/videoPtsIncrement_ << ", max: " << maxPts/videoPtsIncrement_;
                LOG(INFO) << "-> Seeking";
            }

            // requested frame is no where near to being cached, we need to seek
            av_seek_frame(pFormatContext_, pVideoStream_->index, requestedPts - cacheSize_/2*videoPtsIncrement_, AVSEEK_FLAG_BACKWARD);

            std::shared_ptr<AVFrameWrapper> pWrapper = nullptr;
            std::map<int64_t, std::shared_ptr<AVFrameWrapper>> newCache;
            int prerolledFrames = 0;

            // is the next packet the one we want?
            pWrapper = readNextFrame();
            if(!pWrapper || pWrapper->pFrame->pts != requestedPts)
            {
                do
                {
                    pWrapper = readNextFrame();
                    prerolledFrames++;

                    if(pWrapper)
                        newCache[pWrapper->pFrame->pts] = pWrapper;
                }
                while(pWrapper != nullptr && pWrapper->pFrame->pts < requestedPts);
            }
            else
            {
                newCache[pWrapper->pFrame->pts] = pWrapper;
                prerolledFrames++;
            }

            int remainingFrames = cacheSize_ - prerolledFrames;
            if(remainingFrames < 1)
                remainingFrames = 1;

            if(debug_)
                LOG(INFO) << "   Seek prerolled: " << prerolledFrames << ", remaining: " << remainingFrames;

            for(int i = 0; i < remainingFrames; i++)
            {
                pWrapper = readNextFrame();

                if(pWrapper)
                    newCache[pWrapper->pFrame->pts] = pWrapper;
                else
                    break;
            }

            cachedFrames_ = std::move(newCache);
        }
        else
        {
            // requested frame is in the cached range, check how many more frames we should preload

            const auto& iterRequested = cachedFrames_.lower_bound(requestedPts);
            int32_t cachedAfterRequestd = std::distance(iterRequested, cachedFrames_.end());
            int32_t remainingFrames = cacheSize_/2 - cachedAfterRequestd;

            if(remainingFrames > 0 && requestedPts + cacheSize_/2*videoPtsIncrement_ <= pVideoStream_->duration)
            {
                if(debug_)
                {
                    LOG(INFO) << "   loading: " << remainingFrames;
                    LOG(INFO) << "   CACHE: after: " << cachedAfterRequestd << ", before: " << cachedFrames_.size() - cachedAfterRequestd;
                }

                for(int i = 0; i < remainingFrames; i++)
                {
                    std::shared_ptr<AVFrameWrapper> pWrapper = readNextFrame();

                    if(pWrapper)
                        cachedFrames_[pWrapper->pFrame->pts] = pWrapper;
                    else
                        break;
                }
            }
        }

        // cache maintenance (remove too old entries)
        const auto& iterMinLimit = cachedFrames_.lower_bound(requestedPts - cacheSize_/2*videoPtsIncrement_);
        if(iterMinLimit != cachedFrames_.end())
            cachedFrames_.erase(cachedFrames_.begin(), iterMinLimit);

        cacheLevelBefore_ = (float)(requestedPts - minPts) / (cacheSize_ * videoPtsIncrement_);
        cacheLevelAfter_ = (float)(maxPts - requestedPts) / (cacheSize_ * videoPtsIncrement_);
    }
}

void Video::runBenchmark()
{
    int result;
    int remainingPackets = 100;

    AVPacketWrapper packetWrapper;
    AVPacket* pPacket = packetWrapper.pPacket;

    auto tStart = std::chrono::high_resolution_clock::now();

    while(av_read_frame(pFormatContext_, pPacket) >= 0 && remainingPackets > 0)
    {
        if(pPacket->stream_index == pVideoStream_->index)
        {
            LOG(INFO) << "Video PTS: " << pPacket->pts << ", DTS: " << pPacket->dts << ", size: " << pPacket->size;

            result = avcodec_send_packet(pVideoCodecContext_, pPacket);
            if(result < 0)
            {
                LOG(ERROR) << "Error in avcodec_send_packet: " << err2str(result);
                break;
            }

            AVFrame* pReceivedFrame = av_frame_alloc();
            AVFrame* pSwFrame = av_frame_alloc();
            AVFrame* pFrame;

            result = avcodec_receive_frame(pVideoCodecContext_, pReceivedFrame);
            if(result >= 0)
            {
                if(pReceivedFrame->format == hwPixFormat_)
                {
                    result = av_hwframe_transfer_data(pSwFrame, pReceivedFrame, 0); // transfer from GPU to CPU
                    if(result < 0)
                    {
                        LOG(ERROR) << "Failed to transfer data to system memory";
                    }

                    pFrame = pSwFrame;
                }
                else
                {
                    pFrame = pReceivedFrame;
                }

                float pts_s = (float)pReceivedFrame->pts * pVideoStream_->time_base.num / pVideoStream_->time_base.den;

                LOG(INFO) << "- Frame: " << pVideoCodecContext_->frame_number << ", type: " << av_get_picture_type_char(pReceivedFrame->pict_type)
                          << ", PTS: " << pReceivedFrame->pts << "(" << std::setprecision(6) << pts_s << "), format: " << pFrame->format;

                for(int i = 0; i < AV_NUM_DATA_POINTERS; i++)
                {
                    if(pFrame->data[i])
                        LOG(INFO) << "Index " << i << " linesize: " << pFrame->linesize[i];
                }
            }
            else
            {
                LOG(WARNING) << "avcodec_receive_frame result: " << err2str(result);
            }

            av_frame_free(&pReceivedFrame);
            av_frame_free(&pSwFrame);
        }

        av_packet_unref(pPacket);

        remainingPackets--;
    }

    auto tEnd = std::chrono::high_resolution_clock::now();

    LOG(INFO) << "Benchmark: " << std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count() << "ms";
}

std::shared_ptr<Video::AVFrameWrapper> Video::readNextFrame()
{
    int result;
    std::shared_ptr<Video::AVFrameWrapper> pWrapper = nullptr;

    AVPacketWrapper packetWrapper;
    AVPacket* pPacket = packetWrapper.pPacket;

    while(1)
    {
        av_packet_unref(pPacket);

        result = av_read_frame(pFormatContext_, pPacket);
        if(result == AVERROR_EOF)
        {
            if(debug_)
                LOG(INFO) << "EOF. Flushing video codec.";

            result = avcodec_send_packet(pVideoCodecContext_, 0);
        }
        else if(result < 0)
        {
            if(debug_)
                LOG(INFO) << "av_read_frame: " << err2str(result);

            return nullptr;
        }
        else
        {
            if(pPacket->stream_index == pVideoStream_->index)
            {
                if(debug_)
                    LOG(INFO) << "Video PTS: " << pPacket->pts << ", DTS: " << pPacket->dts << ", size: " << pPacket->size;

                result = avcodec_send_packet(pVideoCodecContext_, pPacket);
            }
        }

        if(result < 0)
        {
            if(debug_)
                LOG(ERROR) << "Error in avcodec_send_packet: " << err2str(result);

            avcodec_flush_buffers(pVideoCodecContext_);
            return nullptr;
        }

        pWrapper = std::make_shared<Video::AVFrameWrapper>();
        AVFrame* pFrame = pWrapper->pFrame;

        result = avcodec_receive_frame(pVideoCodecContext_, pFrame);
        if(result >= 0)
        {
            float pts_s = (float)pFrame->pts * pVideoStream_->time_base.num / pVideoStream_->time_base.den;

            if(debug_)
            {
                LOG(INFO) << "- Frame: " << pVideoCodecContext_->frame_number << ", type: " << av_get_picture_type_char(pFrame->pict_type)
                          << ", PTS: " << pFrame->pts << "(" << std::setprecision(6) << pts_s << "), format: " << pFrame->format;
            }

//                for(int i = 0; i < AV_NUM_DATA_POINTERS; i++)
//                {
//                    if(pFrame->data[i])
//                        LOG(INFO) << "Index " << i << " linesize: " << pFrame->linesize[i];
//                }

            return pWrapper;
        }
        else if(result == AVERROR(EAGAIN))
        {
            continue;
        }
        else
        {
            if(debug_)
                LOG(ERROR) << "avcodec_receive_frame result: " << err2str(result);

            avcodec_flush_buffers(pVideoCodecContext_);
            return nullptr;
        }

        break;
    }

    return pWrapper;
}

std::string Video::err2str(int errnum)
{
    std::string msg(AV_ERROR_MAX_STRING_SIZE, 0);

    av_strerror(errnum, msg.data(), msg.size());

    return msg;
}
