#include "Video.hpp"
#include "util/easylogging++.h"
#include <iomanip>

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
 lastGetFrameTimestamp_(-1)
{
    int result;

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

    if(pVideoCodecContext_)
        avcodec_free_context(&pVideoCodecContext_);

    if(pAudioCodecContext_)
        avcodec_free_context(&pAudioCodecContext_);
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

        if(pPacket_->stream_index == pAudioStream_->index)
        {
            LOG(INFO) << "Audio PTS: " << pPacket_->pts << ", DTS: " << pPacket_->dts << ", size: " << pPacket_->size;

            result = avcodec_send_packet(pAudioCodecContext_, pPacket_);
            if(result < 0)
            {
                LOG(ERROR) << "Error in avcodec_send_packet: " << err2str(result);
                break;
            }

            result = avcodec_receive_frame(pAudioCodecContext_, pFrame_);
            if(result >= 0)
            {
                float pts_s = (float)pFrame_->pts * pAudioStream_->time_base.num / pAudioStream_->time_base.den;

                LOG(INFO) << "- Frame: " << pAudioCodecContext_->frame_number << ", sample rate: " << pFrame_->sample_rate
                          << ", PTS: " << pFrame_->pts << "(" << std::setprecision(6) << pts_s << "), format: " << pFrame_->format << ", num samples: "
                          << pFrame_->nb_samples << ", bytesPerSample: " << av_get_bytes_per_sample((AVSampleFormat)pFrame_->format);


                for(int i = 0; i < AV_NUM_DATA_POINTERS; i++)
                {
                    if(pFrame_->data[i])
                        LOG(INFO) << "Index " << i << " linesize: " << pFrame_->linesize[i];
                }
            }
            else
            {
                LOG(WARNING) << "avcodec_receive_frame result: " << err2str(result);
            }
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
