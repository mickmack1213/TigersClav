#include "MediaEncoder.hpp"
#include "util/easylogging++.h"
#include <chrono>

extern "C" {
#include <libavutil/opt.h>
}

MediaEncoder::MediaEncoder(std::string filename, bool useHwEncoder)
:debug_(false),
 filename_(filename),
 initialized_(false),
 pFormatContext_(0),
 pVideoStream_(0),
 pVideoCodec_(0),
 pVideoCodecContext_(0),
 pAudioStream_(0),
 pAudioCodec_(0),
 pAudioCodecContext_(0),
 pResampler_(0),
 curVideoPts_(0),
 curAudioPts_(0),
 useHwEncoder_(useHwEncoder)
{
}

MediaEncoder::~MediaEncoder()
{
    close();
}

bool MediaEncoder::initialize(std::shared_ptr<const MediaFrame> pFrame)
{
    int result;

    // Create container format
    LOG(INFO) << "Trying to open media for encoding: " << filename_;

    result = avformat_alloc_output_context2(&pFormatContext_, NULL, NULL, filename_.c_str());
    if(result < 0 || !pFormatContext_)
    {
        LOG(ERROR) << "Failed to allocate output format context: " << err2str(result);
        return false;
    }

    LOG(INFO) << "Container format: " << pFormatContext_->oformat->name;

    // Create video stream and encoder
    if(pFrame->pImage)
    {
        const AVFrame* pVideo = *(pFrame->pImage);

        pVideoStream_ = avformat_new_stream(pFormatContext_, NULL);
        if(!pVideoStream_)
        {
            LOG(ERROR) << "No memory for video stream";
            return false;
        }

        if(useHwEncoder_)
        {
            pVideoCodec_ = avcodec_find_encoder_by_name("h264_nvenc");
        }
        else
        {
            pVideoCodec_ = avcodec_find_encoder_by_name("libx264");
        }

        if(!pVideoCodec_)
        {
            LOG(ERROR) << "Could not find video codec";
            return false;
        }

        pVideoCodecContext_ = avcodec_alloc_context3(pVideoCodec_);
        if(!pVideoCodecContext_)
        {
            LOG(ERROR) << "No memory for video codec context";
            return false;
        }

        if(useHwEncoder_)
        {
            av_opt_set(pVideoCodecContext_->priv_data, "preset", "p3", 0);
            av_opt_set(pVideoCodecContext_->priv_data, "profile", "main", 0);
            av_opt_set(pVideoCodecContext_->priv_data, "tune", "ll", 0);
            av_opt_set(pVideoCodecContext_->priv_data, "rc", "vbr", 0);
            av_opt_set(pVideoCodecContext_->priv_data, "rc-lookahead", "10", 0);
        }
        else
        {
            av_opt_set(pVideoCodecContext_->priv_data, "preset", "faster", 0);
            av_opt_set(pVideoCodecContext_->priv_data, "movflags", "faststart", 0);
        }

        pVideoCodecContext_->width = pVideo->width;
        pVideoCodecContext_->height = pVideo->height;
        pVideoCodecContext_->sample_aspect_ratio = av_make_q(1, 1);
        pVideoCodecContext_->pix_fmt = (enum AVPixelFormat)pVideo->format;

        // TODO: make video codec parameters configurable?
        if(pVideo->height < 1200) // 1080p?
            pVideoCodecContext_->bit_rate = std::min(16 * 1000 * 1000LL, (long long) pFrame->videoBitRate);
        else // or larger (i.e. 4K)?
            pVideoCodecContext_->bit_rate = std::min(25 * 1000 * 1000LL, (long long) pFrame->videoBitRate);

        pVideoCodecContext_->time_base = pFrame->videoTimeBase;
        pVideoCodecContext_->framerate = pFrame->videoFramerate;
        pVideoStream_->time_base = pVideoCodecContext_->time_base;
        pVideoStream_->r_frame_rate = pVideoCodecContext_->framerate;

        result = avcodec_open2(pVideoCodecContext_, pVideoCodec_, NULL);
        if(result < 0)
        {
            LOG(ERROR) << "avcodec_open2 (video) failed: " << err2str(result);
            return false;
        }

        avcodec_parameters_from_context(pVideoStream_->codecpar, pVideoCodecContext_);

        LOG(INFO) << "Encoder video: " << pVideoCodec_->name << ", " << pVideoCodecContext_->width << "x" << pVideoCodecContext_->height << ", bit rate: " << pVideoCodecContext_->bit_rate;
    }

    // Create audio stream and encoder
    if(pFrame->pSamples)
    {
        const AVFrame* pAudio = *(pFrame->pSamples);

        if(pAudio->format != AV_SAMPLE_FMT_FLTP)
        {
			pResampler_ = swr_alloc();
            int result = swr_alloc_set_opts2(&pResampler_, &pAudio->ch_layout, AV_SAMPLE_FMT_FLTP, pAudio->sample_rate,
											 &pAudio->ch_layout, (enum AVSampleFormat)pAudio->format, pAudio->sample_rate, 0, NULL);
            if(!pResampler_ || result != 0)
            {
                LOG(ERROR) << "Failed to create audio resampler.";
                return false;
            }

            result = swr_init(pResampler_);
            if(result < 0)
            {
                LOG(ERROR) << "Initializing resampler failed: " << err2str(result);
                return false;
            }
        }

        pAudioStream_ = avformat_new_stream(pFormatContext_, NULL);
        if(!pAudioStream_)
        {
            LOG(ERROR) << "No memory for audio stream";
            return false;
        }

        pAudioCodec_ = avcodec_find_encoder_by_name("aac");
        if(!pAudioCodec_)
        {
            LOG(ERROR) << "Could not find audio codec";
            return false;
        }

        pAudioCodecContext_ = avcodec_alloc_context3(pAudioCodec_);
        if(!pAudioCodecContext_)
        {
            LOG(ERROR) << "No memory for audio codec context";
            return false;
        }

        pAudioCodecContext_->ch_layout = (*pFrame->pSamples)->ch_layout;
        pAudioCodecContext_->sample_rate = (*pFrame->pSamples)->sample_rate;
        pAudioCodecContext_->sample_fmt = AV_SAMPLE_FMT_FLTP;
        pAudioCodecContext_->bit_rate = pFrame->audioBitRate;
        pAudioCodecContext_->time_base = pFrame->audioTimeBase;

        pAudioStream_->time_base = pAudioCodecContext_->time_base;
        pAudioStream_->r_frame_rate = av_make_q(1, pAudioCodecContext_->sample_rate);

        result = avcodec_open2(pAudioCodecContext_, pAudioCodec_, NULL);
        if(result < 0)
        {
            LOG(ERROR) << "avcodec_open2 (audio) failed: " << err2str(result);
            return false;
        }

        avcodec_parameters_from_context(pAudioStream_->codecpar, pAudioCodecContext_);

        LOG(INFO) << "Encoder audio: " << pAudioCodec_->name << ", channels: " << pAudioCodecContext_->ch_layout.nb_channels << ", sample rate: " << pAudioCodecContext_->sample_rate
                  << ", bit rate: " << pAudioCodecContext_->bit_rate << ", frame_size: " << pAudioCodecContext_->frame_size;
    }

    // Open output file
    if(pFormatContext_->oformat->flags & AVFMT_GLOBALHEADER)
        pFormatContext_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if(!(pFormatContext_->oformat->flags & AVFMT_NOFILE))
    {
        result = avio_open(&pFormatContext_->pb, filename_.c_str(), AVIO_FLAG_WRITE);
        if(result < 0)
        {
            LOG(ERROR) << "Opening AVIOContext failed: " << err2str(result);
            return false;
        }
    }

    if(debug_)
        av_dump_format(pFormatContext_, 0, filename_.c_str(), 1);

    result = avformat_write_header(pFormatContext_, NULL);
    if(result < 0)
    {
        LOG(ERROR) << "Writing file header failed: " << err2str(result);
        return false;
    }

    LOG(INFO) << "File open for encoding.";

    initialized_ = true;

    return true;
}

void MediaEncoder::close()
{
    int result;

    if(initialized_)
    {
        put(nullptr);

        result = av_write_trailer(pFormatContext_);
        if(result < 0)
        {
            LOG(ERROR) << "Writing trailer failed: " << err2str(result);
        }
        else
        {
            LOG(INFO) << "File trailer written.";

            if(!(pFormatContext_->oformat->flags & AVFMT_NOFILE))
            {
                avio_close(pFormatContext_->pb);
            }
        }
    }

    if(pResampler_)
        swr_free(&pResampler_);

    if(pFormatContext_)
        avformat_free_context(pFormatContext_);

    if(pVideoCodecContext_)
        avcodec_free_context(&pVideoCodecContext_);

    if(pAudioCodecContext_)
        avcodec_free_context(&pAudioCodecContext_);

    initialized_ = false;
    pFormatContext_ = 0;
    pVideoCodecContext_ = 0;
    pVideoStream_ = 0;
    pVideoCodec_ = 0;
    pAudioStream_ = 0;
    pAudioCodec_ = 0;
    pAudioCodecContext_ = 0;
}

int MediaEncoder::sendVideoFrame(const AVFrame* pVideo)
{
    int result;
    int64_t videoPtsInc = pVideoStream_->time_base.den * pVideoStream_->r_frame_rate.den / (pVideoStream_->time_base.num * pVideoStream_->r_frame_rate.num);

    auto tStart = std::chrono::high_resolution_clock::now();

    AVFrame* pEnc = av_frame_alloc();
    pEnc->width = pVideo->width;
    pEnc->height = pVideo->height;
    pEnc->format = pVideo->format;

    av_frame_get_buffer(pEnc, 0);
    av_frame_copy(pEnc, pVideo);
    av_frame_copy_props(pEnc, pVideo);

    auto tCopy = std::chrono::high_resolution_clock::now();
    videoTiming_.copy = std::chrono::duration_cast<std::chrono::microseconds>(tCopy - tStart).count() * 1e-6f;

    pEnc->pts = curVideoPts_;
    curVideoPts_ += videoPtsInc;

    LOG_IF(debug_, INFO) << "Encoding video frame. PTS: " << pEnc->pts;

    result = avcodec_send_frame(pVideoCodecContext_, pEnc);
    if(result < 0)
    {
        LOG(ERROR) << "avcodec_send_frame (video) error: " << err2str(result);
        return -1;
    }

    av_frame_free(&pEnc);

    auto tSend = std::chrono::high_resolution_clock::now();
    videoTiming_.send = std::chrono::duration_cast<std::chrono::microseconds>(tSend - tCopy).count() * 1e-6f;

    return 0;
}

int MediaEncoder::receiveVideoPackets()
{
    int result;
    int64_t videoPtsInc = pVideoStream_->time_base.den * pVideoStream_->r_frame_rate.den / (pVideoStream_->time_base.num * pVideoStream_->r_frame_rate.num);

    while(1)
    {
        AVPacketWrapper packet;
        auto tStart = std::chrono::high_resolution_clock::now();

        result = avcodec_receive_packet(pVideoCodecContext_, packet);
        if(result == AVERROR(EAGAIN) || result == AVERROR_EOF)
        {
            break;
        }
        else if(result < 0)
        {
            LOG(ERROR) << "Error while receiving packet from encoder (video): " << err2str(result);
            return -1;
        }

        auto tReceive = std::chrono::high_resolution_clock::now();
        videoTiming_.receive = std::chrono::duration_cast<std::chrono::microseconds>(tReceive - tStart).count() * 1e-6f;

        packet->stream_index = pVideoStream_->index;
        packet->duration = videoPtsInc;

        if(packet->dts > packet->pts)
            packet->dts = packet->pts - 1000;

        LOG_IF(debug_, INFO) << "   Encoded video frame. PTS: " << packet->pts << ", DTS: " << packet->dts << ", dur: " << packet->duration << ", stream: " << packet->stream_index;

        result = av_interleaved_write_frame(pFormatContext_, packet);
        if(result < 0)
        {
            LOG(ERROR) << "Error while writing frame data: " << err2str(result);
            return -1;
        }

        av_packet_unref(packet);

        auto tWrite = std::chrono::high_resolution_clock::now();
        videoTiming_.write = std::chrono::duration_cast<std::chrono::microseconds>(tWrite - tReceive).count() * 1e-6f;
    }

    return 0;
}

int MediaEncoder::sendAudioFrameFromBuffer(bool flush)
{
    int result;
    int audioPtsInc = pAudioStream_->time_base.den / (pAudioStream_->time_base.num * pAudioCodecContext_->sample_rate);

    auto tStart = std::chrono::high_resolution_clock::now();

    int bufferedSamples = 0;
    for(auto buf : bufferedAudio_)
    {
        bufferedSamples += (*buf.pSamples)->nb_samples - buf.firstSample;
    }

    if(bufferedSamples < pAudioCodecContext_->frame_size && !flush)
    {
        // not enough audio samples for a full frame
        return 0;
    }

    int encSamples = bufferedSamples;
    if(encSamples > pAudioCodecContext_->frame_size)
        encSamples = pAudioCodecContext_->frame_size;

    // we have enough samples to fill an audio frame for encoding
    const AVFrame* pAudio = *(bufferedAudio_.front().pSamples);

    AVFrame* pAudioBuf = av_frame_alloc();
    pAudioBuf->nb_samples = encSamples;
    pAudioBuf->format = pAudio->format;
    pAudioBuf->sample_rate = pAudio->sample_rate;
    pAudioBuf->ch_layout = pAudio->ch_layout;
    pAudioBuf->pts = curAudioPts_;
    curAudioPts_ += audioPtsInc * encSamples;

    LOG_IF(debug_, INFO) << "Filling audio frame. PTS: " << pAudioBuf->pts << ", buffered: " << bufferedSamples << ", enc: " << encSamples;

    av_frame_get_buffer(pAudioBuf, 0);

    int samplesLeft = encSamples;
    int dstOffset = 0;
    for(auto iter = bufferedAudio_.begin(); iter != bufferedAudio_.end(); )
    {
        int copySize = (*iter->pSamples)->nb_samples - iter->firstSample;
        if(copySize > samplesLeft)
            copySize = samplesLeft;

        av_samples_copy(pAudioBuf->data, (*iter->pSamples)->data, dstOffset, iter->firstSample, copySize, pAudioBuf->ch_layout.nb_channels, (enum AVSampleFormat)pAudioBuf->format);

        samplesLeft -= copySize;
        dstOffset += copySize;
        iter->firstSample += copySize;

        bool remove = iter->firstSample >= (*iter->pSamples)->nb_samples;

        iter++;

        if(remove)
            bufferedAudio_.pop_front();

        if(samplesLeft <= 0)
            break;
    }

    AVFrame* pAudioEnc = av_frame_alloc();
    pAudioEnc->nb_samples = pAudioBuf->nb_samples;
    pAudioEnc->format = AV_SAMPLE_FMT_FLTP;
    pAudioEnc->sample_rate = pAudioBuf->sample_rate;
    pAudioEnc->ch_layout = pAudioBuf->ch_layout;
    pAudioEnc->pts = pAudioBuf->pts;

    LOG_IF(debug_, INFO) << "Encoding audio frame. PTS: " << pAudioEnc->pts << ", channel_layout: " << pAudioEnc->ch_layout.u.mask;

    av_frame_get_buffer(pAudioEnc, 0);

    if(pResampler_)
    {
        result = swr_convert_frame(pResampler_, pAudioEnc, pAudioBuf);
        if(result < 0)
        {
            LOG(ERROR) << "Audio conversion failed: " << err2str(result);
            return -1;
        }
    }
    else
    {
        av_frame_copy(pAudioEnc, pAudioBuf);
    }

    auto tCopy = std::chrono::high_resolution_clock::now();
    audioTiming_.copy = std::chrono::duration_cast<std::chrono::microseconds>(tCopy - tStart).count() * 1e-6f;

    result = avcodec_send_frame(pAudioCodecContext_, pAudioEnc);
    if(result < 0)
    {
        LOG(ERROR) << "avcodec_send_frame (audio) error: " << err2str(result);
        return -1;
    }

    av_frame_free(&pAudioBuf);
    av_frame_free(&pAudioEnc);

    auto tSend = std::chrono::high_resolution_clock::now();
    audioTiming_.send = std::chrono::duration_cast<std::chrono::microseconds>(tSend - tCopy).count() * 1e-6f;

    return 1;
}

int MediaEncoder::receiveAudioPackets()
{
    int result;

    while(1)
    {
        AVPacketWrapper packet;
        auto tStart = std::chrono::high_resolution_clock::now();

        result = avcodec_receive_packet(pAudioCodecContext_, packet);
        if(result == AVERROR(EAGAIN) || result == AVERROR_EOF)
        {
            break;
        }
        else if(result < 0)
        {
            LOG(ERROR) << "   Error while receiving packet from encoder (audio): " << err2str(result);
            return -1;
        }

        auto tReceive = std::chrono::high_resolution_clock::now();
        audioTiming_.receive = std::chrono::duration_cast<std::chrono::microseconds>(tReceive - tStart).count() * 1e-6f;

        packet->stream_index = pAudioStream_->index;

        LOG_IF(debug_, INFO) << "   Encoded audio frame. PTS: " << packet->pts << ", DTS: " << packet->dts << ", dur: " << packet->duration << ", stream: " << packet->stream_index;

        result = av_interleaved_write_frame(pFormatContext_, packet);
        if(result < 0)
        {
            LOG(ERROR) << "Error while writing frame data: " << err2str(result);
            return -1;
        }

        av_packet_unref(packet);

        auto tWrite = std::chrono::high_resolution_clock::now();
        audioTiming_.write = std::chrono::duration_cast<std::chrono::microseconds>(tWrite - tReceive).count() * 1e-6f;
    }

    return 0;
}

int MediaEncoder::put(std::shared_ptr<const MediaFrame> pFrame)
{
    int result;

    if(!initialized_)
    {
        if(!initialize(pFrame))
        {
            return -1;
        }
    }

    if(pVideoCodecContext_)
    {
        int64_t videoPtsInc = pVideoStream_->time_base.den * pVideoStream_->r_frame_rate.den / (pVideoStream_->time_base.num * pVideoStream_->r_frame_rate.num);

        const AVFrame* pVideo = 0;

        if(pFrame && pFrame->pImage)
            pVideo = *(pFrame->pImage);

        if(pVideo)
        {
            result = sendVideoFrame(pVideo);
            if(result < 0)
                return result;
        }
        else if(!pFrame)
        {
            LOG_IF(debug_, INFO) << "Flushing video encoder.";

            avcodec_send_frame(pVideoCodecContext_, 0);
        }

        result = receiveVideoPackets();
        if(result < 0)
            return result;
    }

    if(pAudioCodecContext_)
    {
        int audioPtsInc = pAudioStream_->time_base.den / (pAudioStream_->time_base.num * pAudioCodecContext_->sample_rate);

        if(pFrame && pFrame->pSamples)
        {
            bufferedAudio_.push_back((BufferedAudioFrame){pFrame->pSamples, 0});
        }

        int sendFrameResult = 0;

        do
        {
            sendFrameResult = sendAudioFrameFromBuffer(false);
            if(sendFrameResult < 0)
            {
                return sendFrameResult;
            }
            else
            {
                result = receiveAudioPackets();
                if(result < 0)
                    return result;
            }
        }
        while(sendFrameResult > 0);

        if(!pFrame)
        {
            LOG_IF(debug_, INFO) << "Flushing audio encoder.";

            result = sendAudioFrameFromBuffer(true);
            if(result < 0)
                return result;

            result = receiveAudioPackets();
            if(result < 0)
                return result;

            avcodec_send_frame(pAudioCodecContext_, 0);

            result = receiveAudioPackets();
            if(result < 0)
                return result;
        }
    }

    return 0;
}

std::string MediaEncoder::err2str(int errnum)
{
    std::string msg(AV_ERROR_MAX_STRING_SIZE, 0);

    av_strerror(errnum, msg.data(), msg.size());

    return msg;
}
