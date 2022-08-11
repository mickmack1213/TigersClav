#include "MediaSource.hpp"
#include "util/easylogging++.h"
#include <iomanip>


MediaSource::MediaSource(std::string filename)
{
    debug_ = true;

    int result;

    LOG(INFO) << "Trying to load media container: " << filename;

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
//    frameDeltaTime_s_ = 1.0f/frameRate;

    LOG(INFO) << "Codec: " << pVideoCodec_->name << ", bitrate: " << pVideoCodecPars->bit_rate;
    LOG(INFO) << "Duration: " << std::setprecision(6) << duration_s << " (" << pVideoStream_->duration << "), startTime: " << pVideoStream_->start_time << "pts";
    LOG(INFO) << "Time base: " << pVideoStream_->time_base.num << "/" << pVideoStream_->time_base.den << ", framerate: " << pVideoStream_->r_frame_rate.num << "/" << pVideoStream_->r_frame_rate.den;
    LOG(INFO) << "Video resolution: " << pVideoCodecPars->width << "x" << pVideoCodecPars->height << " @ " << std::setprecision(4) << frameRate << "fps";

//    videoPtsIncrement_ = pVideoStream_->time_base.den * pVideoStream_->r_frame_rate.den / (pVideoStream_->time_base.num * pVideoStream_->r_frame_rate.num);

//    LOG(INFO) << "PTS inc: " << videoPtsIncrement_ << ", last frame: " << pVideoStream_->duration / videoPtsIncrement_;

    int dictEntries = av_dict_count(pVideoStream_->metadata);

    LOG(INFO) << "Dict entries: " << dictEntries;

    AVDictionaryEntry *pDictEntry = NULL;
    while((pDictEntry = av_dict_get(pVideoStream_->metadata, "", pDictEntry, AV_DICT_IGNORE_SUFFIX)) != 0)
    {
        LOG(INFO) << "    " << pDictEntry->key << " = " << pDictEntry->value;
    }

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

    // Find best audio stream and setup codec
    streamNumber = av_find_best_stream(pFormatContext_, AVMEDIA_TYPE_AUDIO, -1, -1, &pAudioCodec_, 0);
    if(streamNumber < 0)
    {
        LOG(ERROR) << "Could retrieve audio stream. Result: " << err2str(streamNumber);
        return;
    }

    pAudioStream_ = pFormatContext_->streams[streamNumber];
    AVCodecParameters* pAudioCodecPars = pAudioStream_->codecpar;

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

    duration_s = (float)pAudioStream_->duration * pAudioStream_->time_base.num / pAudioStream_->time_base.den;

    LOG(INFO) << "Context. Sample rate: " << pAudioCodecContext_->sample_rate << ", channels: " << pAudioCodecContext_->channels << ", bytes per sample: "
                    << av_get_bytes_per_sample(pAudioCodecContext_->sample_fmt) << ", planar: " << av_sample_fmt_is_planar(pAudioCodecContext_->sample_fmt);

    LOG(INFO) << "Codec pars. Channels: " << pAudioCodecPars->channels << ", layout: 0x" << std::hex << pAudioCodecPars->channel_layout << std::dec << ", sample rate: " << pAudioCodecPars->sample_rate;
    LOG(INFO) << "Audio codec: " << pAudioCodec_->name << ", bitrate: " << pAudioCodecPars->bit_rate << ", bits per raw sample: " << pAudioCodecPars->bits_per_raw_sample;
    LOG(INFO) << "Duration: " << std::setprecision(6) << duration_s << ", startTime: " << pAudioStream_->start_time << "pts";
    LOG(INFO) << "Time base: " << pAudioStream_->time_base.num << "/" << pAudioStream_->time_base.den;

    updateCache(); // TODO: testing
}

MediaSource::~MediaSource()
{
}

void MediaSource::updateCache()
{
    int result;
    AVPacketWrapper pPacket;

    while(lastCachedTime_ < lastRequestedTime_ + 1.0)
    {
        av_packet_unref(pPacket);

        result = av_read_frame(pFormatContext_, pPacket);
        if(result == AVERROR_EOF)
        {
            if(debug_)
                LOG(INFO) << "EOF. Flushing video codec.";

//            result = avcodec_send_packet(pVideoCodecContext_, 0);
        }
        else if(result < 0)
        {
            if(debug_)
                LOG(INFO) << "av_read_frame: " << err2str(result);

            return;
        }
        else
        {
            if(pPacket->stream_index == pVideoStream_->index)
            {
                if(debug_)
                    LOG(INFO) << "Video PTS: " << pPacket->pts << ", DTS: " << pPacket->dts << ", size: " << pPacket->size;

                result = avcodec_send_packet(pVideoCodecContext_, pPacket);
                if(result < 0)
                {
                    if(debug_)
                        LOG(ERROR) << "Error in avcodec_send_packet (video): " << err2str(result);

                    avcodec_flush_buffers(pVideoCodecContext_);
                    return;
                }

                AVFrameWrapper pFrame;
                result = avcodec_receive_frame(pVideoCodecContext_, pFrame);
                if(result >= 0)
                {
                    float pts_s = (float)pFrame->pts * pVideoStream_->time_base.num / pVideoStream_->time_base.den;

                    lastCachedTime_ = pts_s;

                    if(debug_)
                    {
                        LOG(INFO) << "Video Frame: " << pVideoCodecContext_->frame_number << ", type: " << av_get_picture_type_char(pFrame->pict_type)
                                  << ", PTS: " << pFrame->pts << "(" << std::setprecision(6) << pts_s << "), format: " << pFrame->format;
                    }

//                    return pWrapper;
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
                    return;
                }
            }

            if(pPacket->stream_index == pAudioStream_->index)
            {
                result = avcodec_send_packet(pAudioCodecContext_, pPacket);
                if(result < 0)
                {
                    if(debug_)
                        LOG(ERROR) << "Error in avcodec_send_packet (audio): " << err2str(result);

                    avcodec_flush_buffers(pAudioCodecContext_);
                    return;
                }

                AVFrameWrapper pFrame;
                result = avcodec_receive_frame(pAudioCodecContext_, pFrame);
                if(result >= 0)
                {
                    float pts_s = (float)pFrame->pts * pAudioStream_->time_base.num / pAudioStream_->time_base.den;

                    if(debug_)
                    {
                        LOG(INFO) << "Audio Frame: " << pAudioCodecContext_->frame_number// << ", type: " << av_get_picture_type_char(pFrame->pict_type)
                                  << ", PTS: " << pFrame->pts << "(" << std::setprecision(6) << pts_s << "), format: " << pFrame->format
                                  << ", linesize0: " << pFrame->linesize[0] << ", linesize1: " << pFrame->linesize[1]
                                  << ", data0: " << std::hex << (int64_t)pFrame->data[0] << ", data1: " << (int64_t)pFrame->data[1] << ", data2: " << (int64_t)pFrame->data[2] << std::dec
                                  << ", samples: " << pFrame->nb_samples;
                    }

        //                for(int i = 0; i < AV_NUM_DATA_POINTERS; i++)
        //                {
        //                    if(pFrame->data[i])
        //                        LOG(INFO) << "Index " << i << " linesize: " << pFrame->linesize[i];
        //                }

//                    return pWrapper;
                }
            }
        }
    }
}

std::string MediaSource::err2str(int errnum)
{
    std::string msg(AV_ERROR_MAX_STRING_SIZE, 0);

    av_strerror(errnum, msg.data(), msg.size());

    return msg;
}
