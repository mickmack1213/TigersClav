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
 pFormatContext_(0),
 pVideoCodec_(0),
 pVideoStream_(0),
 pVideoCodecContext_(0),
 pFrame_(0),
 pPacket_(0),
 pAudioCodec_(0),
 pAudioStream_(0),
 pAudioCodecContext_(0),
 runPreloaderThread_(true),
 cacheSize_(50)
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

    LOG(INFO) << "Codec: " << pVideoCodec_->name << ", bitrate: " << pVideoCodecPars->bit_rate;
    LOG(INFO) << "Duration: " << std::setprecision(6) << duration_s << ", startTime: " << pVideoStream_->start_time << "pts";
    LOG(INFO) << "Time base: " << pVideoStream_->time_base.num << "/" << pVideoStream_->time_base.den;
    LOG(INFO) << "Video resolution: " << pVideoCodecPars->width << "x" << pVideoCodecPars->height << " @ " << std::setprecision(4) << frameRate << "fps";

    const AVCodecHWConfig* pVideoHWConfig = 0;

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

    result = avcodec_open2(pVideoCodecContext_, pVideoCodec_, NULL);
    if(result < 0)
    {
        LOG(ERROR) << "Video: Failed to open codec through avcodec_open2. Result: " << err2str(result);
        return;
    }

    pFrame_ = av_frame_alloc();
    if(!pFrame_)
    {
        LOG(ERROR) << "No memory for AVFrame";
        return;
    }

    pPacket_ = av_packet_alloc();
    if(!pPacket_)
    {
        LOG(ERROR) << "No memory for AVPacket";
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

    runBenchmark();

//    preloaderThread_ = std::thread(preloader, this);

    isLoaded_ = true;
}

Video::~Video()
{
    runPreloaderThread_ = false;
    preloadCondition_.notify_one();

    if(preloaderThread_.joinable())
        preloaderThread_.join();

    for(AVFrame* pFrame : cachedFrames_)
        av_frame_free(&pFrame);

    if(pFormatContext_)
        avformat_close_input(&pFormatContext_);

    if(pPacket_)
        av_packet_free(&pPacket_);

    if(pFrame_)
        av_frame_free(&pFrame_);

    if(pVideoCodecContext_)
        avcodec_free_context(&pVideoCodecContext_);

    if(pAudioCodecContext_)
        avcodec_free_context(&pAudioCodecContext_);

    if(pHwDeviceContext_)
        av_buffer_unref(&pHwDeviceContext_);
}

AVFrame* Video::getFrame(int64_t timestamp)
{
    int result;

    if(!isLoaded_)
        return 0;

    requestedPts_ = timestamp;
    preloadCondition_.notify_one();

    const auto& iterAfterRequested = std::find_if(cachedFrames_.begin(), cachedFrames_.end(), [&](AVFrame* pFrame){ return pFrame->pts > requestedPts_; });

    if(iterAfterRequested != cachedFrames_.end())
    {
        return *iterAfterRequested;
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

        if(cachedFrames_.empty() || requestedPts_ < cachedFrames_.front()->pts || requestedPts_ > cachedFrames_.back()->pts)
        {
            // requested frame is no where near to being cached, we need to seek
            for(AVFrame* pFrame : cachedFrames_)
                av_frame_free(&pFrame);

            cachedFrames_.clear();

            int32_t remainingFrames = cacheSize_;

            av_seek_frame(pFormatContext_, pVideoStream_->index, requestedPts_, AVSEEK_FLAG_BACKWARD);

            loadFrames(remainingFrames);
        }
        else
        {
            // requested frame is in the cached range, check how many more frames we should preload
            const auto& iterAfterRequested = std::find_if(cachedFrames_.begin(), cachedFrames_.end(), [&](AVFrame* pFrame){ return pFrame->pts > requestedPts_; });
            int32_t cachedAfterRequestd = std::distance(iterAfterRequested, cachedFrames_.end());
            int32_t remainingFrames = cacheSize_ - cachedAfterRequestd;

            if(remainingFrames > 0)
                LOG(INFO) << "CACHE: after: " << cachedAfterRequestd << ", before: " << cachedFrames_.size() - cachedAfterRequestd;

            loadFrames(remainingFrames);
        }
    }
}

void Video::runBenchmark()
{
    int result;
    int remainingPackets = 100;

    auto tStart = std::chrono::high_resolution_clock::now();

    while(av_read_frame(pFormatContext_, pPacket_) >= 0 && remainingPackets > 0)
    {
        if(pPacket_->stream_index == pVideoStream_->index)
        {
            LOG(INFO) << "Video PTS: " << pPacket_->pts << ", DTS: " << pPacket_->dts << ", size: " << pPacket_->size;

            result = avcodec_send_packet(pVideoCodecContext_, pPacket_);
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

        av_packet_unref(pPacket_);

        remainingPackets--;
    }

    auto tEnd = std::chrono::high_resolution_clock::now();

    LOG(INFO) << "Benchmark: " << std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count() << "ms";
}

void Video::loadFrames(int32_t remainingFrames)
{
    int result;

    if(remainingFrames <= 0)
        return;

    while(av_read_frame(pFormatContext_, pPacket_) >= 0)
    {
        if(pPacket_->stream_index == pVideoStream_->index)
        {
            LOG(INFO) << "Video PTS: " << pPacket_->pts << ", DTS: " << pPacket_->dts << ", size: " << pPacket_->size;

            result = avcodec_send_packet(pVideoCodecContext_, pPacket_);
            if(result < 0)
            {
                LOG(ERROR) << "Error in avcodec_send_packet: " << err2str(result);
                break;
            }

            result = avcodec_receive_frame(pVideoCodecContext_, pFrame_);
            if(result >= 0)
            {
                float pts_s = (float)pFrame_->pts * pVideoStream_->time_base.num / pVideoStream_->time_base.den;

                LOG(INFO) << "- Frame: " << pVideoCodecContext_->frame_number << ", type: " << av_get_picture_type_char(pFrame_->pict_type)
                          << ", PTS: " << pFrame_->pts << "(" << std::setprecision(6) << pts_s << "), format: " << pFrame_->format;

//                for(int i = 0; i < AV_NUM_DATA_POINTERS; i++)
//                {
//                    if(pFrame_->data[i])
//                        LOG(INFO) << "Index " << i << " linesize: " << pFrame_->linesize[i];
//                }

                if(pFrame_->pts > requestedPts_)
                    remainingFrames--;

                cachedFrames_.push_back(pFrame_);

                if(cachedFrames_.size() >= cacheSize_)
                {
                    pFrame_ = cachedFrames_.front();
                    cachedFrames_.pop_front();
                }
                else
                {
                    pFrame_ = av_frame_alloc();
                    LOG(INFO) << "Allocating frame";
                    if(!pFrame_)
                    {
                        LOG(ERROR) << "No more memory for AVFrame";
                        break; // TODO: handle this cleverly?
                    }
                }

                if(remainingFrames <= 0)
                    break;
            }
            else
            {
                LOG(WARNING) << "avcodec_receive_frame result: " << err2str(result);
            }
        }

        av_packet_unref(pPacket_);
    }
}

std::string Video::err2str(int errnum)
{
    std::string msg(AV_ERROR_MAX_STRING_SIZE, 0);

    av_strerror(errnum, msg.data(), msg.size());

    return msg;
}
