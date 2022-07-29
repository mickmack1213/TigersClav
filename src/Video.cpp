#include "Video.hpp"
#include "util/easylogging++.h"
#include <iomanip>

Video::Video(std::string filename)
:isLoaded_(false),
 pFormatContext_(0),
 pCodec_(0),
 pCodecContext_(0),
 pFrame_(0),
 pPacket_(0),
 lastGetFrameTimestamp_(-1)
{
    int result;

    LOG(INFO) << "Trying to load video: " << filename;

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

    int streamNumber = av_find_best_stream(pFormatContext_, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec_, 0);
    if(streamNumber < 0)
    {
        LOG(ERROR) << "Could retrieve video stream. Result: " << err2str(streamNumber);
        return;
    }

    pVideoStream_ = pFormatContext_->streams[streamNumber];
    AVCodecParameters* pCodecPars = pVideoStream_->codecpar;

    float frameRate = (float)pVideoStream_->r_frame_rate.num / pVideoStream_->r_frame_rate.den;
    float duration_s = (float)pVideoStream_->duration * pVideoStream_->time_base.num / pVideoStream_->time_base.den;

    LOG(INFO) << "Video resolution: " << pCodecPars->width << "x" << pCodecPars->height << " @ " << std::setprecision(4) << frameRate << "fps";
    LOG(INFO) << "Duration: " << std::setprecision(6) << duration_s << ", startTime: " << pVideoStream_->start_time << "pts";
    LOG(INFO) << "Time base: " << pVideoStream_->time_base.num << "/" << pVideoStream_->time_base.den;
    LOG(INFO) << "Codec: " << pCodec_->name << ", bitrate: " << pCodecPars->bit_rate;

    pCodecContext_ = avcodec_alloc_context3(pCodec_);
    if(!pCodecContext_)
    {
        LOG(ERROR) << "No memory for AVCodecContext";
        return;
    }

    result = avcodec_parameters_to_context(pCodecContext_, pCodecPars);
    if(result < 0)
    {
        LOG(ERROR) << "Failed to copy codec parameters to codec context. Result: " << err2str(result);
        return;
    }

    result = avcodec_open2(pCodecContext_, pCodec_, NULL);
    if(result < 0)
    {
        LOG(ERROR) << "Failed to open codec through avcodec_open2. Result: " << err2str(result);
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

    isLoaded_ = true;
}

Video::~Video()
{
    if(pFormatContext_)
        avformat_close_input(&pFormatContext_);

    if(pPacket_)
        av_packet_free(&pPacket_);

    if(pFrame_)
        av_frame_free(&pFrame_);

    if(pCodecContext_)
        avcodec_free_context(&pCodecContext_);
}

AVFrame* Video::getFrame(int64_t timestamp)
{
    int result;

    if(!isLoaded_)
        return 0;

    if(lastGetFrameTimestamp_ == timestamp)
        return pFrame_;

    av_seek_frame(pFormatContext_, pVideoStream_->index, timestamp, AVSEEK_FLAG_BACKWARD);

    int how_many_packets_to_process = 50;

    while(av_read_frame(pFormatContext_, pPacket_) >= 0)
    {
        if(pPacket_->stream_index == pVideoStream_->index)
        {
            LOG(INFO) << "PTS: " << pPacket_->pts << ", DTS: " << pPacket_->dts << ", size: " << pPacket_->size;

            result = avcodec_send_packet(pCodecContext_, pPacket_);
            if(result < 0)
            {
                LOG(ERROR) << "Error in avcodec_send_packet: " << err2str(result);
                break;
            }

            result = avcodec_receive_frame(pCodecContext_, pFrame_);
            if(result >= 0)
            {
                LOG(INFO) << "- Frame: " << pCodecContext_->frame_number << ", type: " << av_get_picture_type_char(pFrame_->pict_type)
                          << ", PTS: " << pFrame_->pts << ", format: " << pFrame_->format;

                for(int i = 0; i < AV_NUM_DATA_POINTERS; i++)
                {
                    if(pFrame_->data[i])
                        LOG(INFO) << "Index " << i << " linesize: " << pFrame_->linesize[i];
                }

                lastGetFrameTimestamp_ = timestamp;
                break;
            }
            else
            {
                LOG(WARNING) << "avcodec_receive_frame result: " << err2str(result);
            }

            if(--how_many_packets_to_process <= 0)
                break;
        }

        av_packet_unref(pPacket_);
    }

    return pFrame_;
}

std::string Video::err2str(int errnum)
{
    std::string msg(AV_ERROR_MAX_STRING_SIZE, 0);

    av_strerror(errnum, msg.data(), msg.size());

    return msg;
}
