#include "MediaSource.hpp"
#include "util/easylogging++.h"
#include <iomanip>


MediaSource::MediaSource(std::string filename)
:lastRequestTime_s_(0.0),
 debug_(false),
 isLoaded_(false),
 filename_(filename),
 runPreloaderThread_(true),
 reachedEndOfFile_(false)
{
    int result;

    LOG(INFO) << "Trying to load media source: " << filename;

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
    LOG(INFO) << "Duration: " << std::setprecision(6) << duration_s << " (" << pVideoStream_->duration << "), startTime: " << pVideoStream_->start_time << "pts";
    LOG(INFO) << "Time base: " << pVideoStream_->time_base.num << "/" << pVideoStream_->time_base.den << ", framerate: " << pVideoStream_->r_frame_rate.num << "/" << pVideoStream_->r_frame_rate.den;
    LOG(INFO) << "Video resolution: " << pVideoCodecPars->width << "x" << pVideoCodecPars->height << " @ " << std::setprecision(4) << frameRate << "fps";

    videoFrameDeltaTime_s_ = (double)pVideoStream_->r_frame_rate.den / (double)pVideoStream_->r_frame_rate.num;
    videoPtsInc_ = pVideoStream_->time_base.den * pVideoStream_->r_frame_rate.den / (pVideoStream_->time_base.num * pVideoStream_->r_frame_rate.num);

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

    audioPtsInc_ = pAudioStream_->time_base.den / (pAudioStream_->time_base.num * pAudioCodecContext_->sample_rate);

    preloaderThread_ = std::thread(preloader, this);

    isLoaded_ = true;
}

MediaSource::~MediaSource()
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
}

double MediaSource::getDuration_s() const
{
    return videoPtsToSeconds(pVideoStream_->duration);
}

void MediaSource::seekTo(double time_s)
{
    double tMax = getDuration_s() - videoFrameDeltaTime_s_;
    time_s = std::min(tMax, time_s);
    time_s = std::max(0.0, time_s);

    lastRequestTime_s_ = time_s;
    preloadCondition_.notify_one();
}

void MediaSource::seekToNext()
{
    seekTo(videoPtsToSeconds(videoSecondsToPts(lastRequestTime_s_) + videoPtsInc_));
}

void MediaSource::seekToPrevious()
{
    seekTo(videoPtsToSeconds(videoSecondsToPts(lastRequestTime_s_) - videoPtsInc_));
}

std::shared_ptr<MediaFrame> MediaSource::get()
{
    int result;

    preloadCondition_.notify_one();

    auto pMediaFrame = std::make_shared<MediaFrame>();

    pMediaFrame->videoTimeBase = pVideoStream_->time_base;
    pMediaFrame->videoFramerate = pVideoStream_->r_frame_rate;
    pMediaFrame->videoCodec = pVideoCodecContext_->codec_id;
    pMediaFrame->videoBitRate = pVideoCodecContext_->bit_rate;

    int64_t requestPts = videoSecondsToPts(lastRequestTime_s_);
    requestPts = ((requestPts + videoPtsInc_/2)/videoPtsInc_) * videoPtsInc_;

    {
        std::lock_guard<std::mutex> lock(videoSamplesMutex_);

        const auto& iterVideo = videoSamples_.lower_bound(requestPts);
        if(iterVideo == videoSamples_.end())
            return nullptr;

        pMediaFrame->pImage = iterVideo->second;
    }

    pMediaFrame->audioTimeBase = pAudioStream_->time_base;
    pMediaFrame->audioCodec = pAudioCodecContext_->codec_id;
    pMediaFrame->audioBitRate = pAudioCodecContext_->bit_rate;

    double tRequest_s = videoPtsToSeconds(requestPts);

    int64_t audioPtsMin = audioSecondsToPts(tRequest_s);
    int64_t audioPtsMax = audioSecondsToPts(tRequest_s + videoFrameDeltaTime_s_);

    {
        std::lock_guard<std::mutex> lock(audioSamplesMutex_);

        auto iterAudioMin = audioSamples_.upper_bound(audioPtsMin);
        if(iterAudioMin == audioSamples_.end())
            return nullptr;

        if(iterAudioMin != audioSamples_.begin())
            iterAudioMin--;

        const auto iterAudioMax = audioSamples_.lower_bound(audioPtsMax);

        const AVFrame* pFirstSrcFrame = *iterAudioMin->second;

        auto pWrapper = std::make_shared<AVFrameWrapper>();
        AVFrame* pFrame = *pWrapper;

        pFrame->nb_samples = (audioPtsMax - audioPtsMin) / audioPtsInc_;
        pFrame->format = pFirstSrcFrame->format;
        pFrame->sample_rate = pFirstSrcFrame->sample_rate;
        pFrame->pts = audioPtsMin;

        pFrame->channel_layout = pFirstSrcFrame->channel_layout;
        if(pFrame->channel_layout == 0)
            pFrame->channel_layout = av_get_default_channel_layout(pFirstSrcFrame->channels);

        result = av_frame_get_buffer(pFrame, 0);
        if(result < 0)
        {
            LOG(ERROR) << "No memory for audio frame data: " << err2str(result);
            return nullptr;
        }

        int samplesLeft = pFrame->nb_samples;
        int64_t startPts = audioPtsMin;
        int dstOffset = 0;

        for(auto iter = iterAudioMin; iter != iterAudioMax; iter++)
        {
            int srcOffset = (startPts - iter->first) / audioPtsInc_;
            int copySize = (*iter->second)->nb_samples - srcOffset;
            if(copySize > samplesLeft)
                copySize = samplesLeft;

            av_samples_copy(pFrame->data, (*iter->second)->data, dstOffset, srcOffset, copySize, (*iter->second)->channels, (enum AVSampleFormat)pFrame->format);

            samplesLeft -= copySize;
            dstOffset += copySize;
            startPts += copySize * audioPtsInc_;
        }

        pMediaFrame->pSamples = pWrapper;
    }

    return pMediaFrame;
}

std::list<std::string> MediaSource::getFileDetails() const
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

void MediaSource::preloader()
{
    int result;

    while(runPreloaderThread_)
    {
        std::unique_lock<std::mutex> lock(preloadMutex_);
        preloadCondition_.wait(lock);

        if(!runPreloaderThread_)
            return;

        updateCache(lastRequestTime_s_);
    }
}

MediaCachedDuration MediaSource::getCachedDuration() const
{
    MediaCachedDuration cache { 0 };
    double requestTime_s = lastRequestTime_s_;

    if(!videoSamples_.empty())
    {
        cache.video_s[0] = std::max(0.0, requestTime_s - videoPtsToSeconds(videoSamples_.begin()->first));
        cache.video_s[1] = std::max(0.0, videoPtsToSeconds(videoSamples_.rbegin()->first) - requestTime_s);
    }

    if(!audioSamples_.empty())
    {
        cache.audio_s[0] = std::max(0.0, requestTime_s - audioPtsToSeconds(audioSamples_.begin()->first));
        cache.audio_s[1] = std::max(0.0, audioPtsToSeconds(audioSamples_.rbegin()->first) - requestTime_s);
    }

    return cache;
}

void MediaSource::updateCache(double requestTime_s)
{
    int result;
    AVPacketWrapper pPacket;

    std::shared_ptr<std::map<int64_t, std::vector<uint8_t>>> audioData = nullptr;
    std::shared_ptr<AVFrameWrapper> videoData = nullptr;

    const double bufferTime_s = 0.5;

    const bool invalidRequestTime = requestTime_s < 0.0 || requestTime_s >= videoPtsToSeconds(pVideoStream_->duration);
    if(invalidRequestTime)
        return;

    double cachedTimesVideo_s[2] = { 0.0, 0.0 };
    double cachedTimesAudio_s[2] = { 0.0, 0.0 };

    if(!videoSamples_.empty())
    {
        cachedTimesVideo_s[0] = videoPtsToSeconds(videoSamples_.begin()->first);
        cachedTimesVideo_s[1] = videoPtsToSeconds(videoSamples_.rbegin()->first);
    }

    if(!audioSamples_.empty())
    {
        cachedTimesAudio_s[0] = audioPtsToSeconds(audioSamples_.begin()->first);
        cachedTimesAudio_s[1] = audioPtsToSeconds(audioSamples_.rbegin()->first);
    }

    const bool requestOutsideVideoCache = requestTime_s < cachedTimesVideo_s[0] || requestTime_s > cachedTimesVideo_s[1];
    const bool requestOutsideAudioCache = requestTime_s < cachedTimesAudio_s[0] || requestTime_s > cachedTimesAudio_s[1] - videoFrameDeltaTime_s_;

    if(requestOutsideVideoCache || requestOutsideAudioCache)
    {
        // Seeking required
        LOG_IF(debug_, INFO) << "Seeking to: " << requestTime_s;

        reachedEndOfFile_ = false;

        {
            std::lock_guard<std::mutex> videoLock(videoSamplesMutex_);
            videoSamples_.clear();
        }

        {
            std::lock_guard<std::mutex> audioLock(audioSamplesMutex_);
            audioSamples_.clear();
        }

        while(processVideoFrame(0));
        while(processAudioFrame(0));

        double seekTime_s = std::max(0.0, requestTime_s - bufferTime_s*0.5);

        LOG_IF(debug_, INFO) << "Seek time: " << seekTime_s;

        av_seek_frame(pFormatContext_, pVideoStream_->index, videoSecondsToPts(seekTime_s), AVSEEK_FLAG_BACKWARD);

        fillCache(requestTime_s, requestTime_s + bufferTime_s);

        if(videoSamples_.empty() || audioSamples_.empty())
        {
            LOG_IF(debug_, INFO) << "Missing data, seeking via audio stream.";

            av_seek_frame(pFormatContext_, pAudioStream_->index, audioSecondsToPts(seekTime_s), AVSEEK_FLAG_BACKWARD);

            fillCache(requestTime_s, requestTime_s + bufferTime_s);
        }
    }
    else
    {
        // values are in cache
        fillCache(requestTime_s, requestTime_s + bufferTime_s);
        cleanCache(std::max(0.0, requestTime_s - bufferTime_s));
    }
}

void MediaSource::fillCache(double tFirst_s, double tLast_s)
{
    int result;
    AVPacketWrapper pPacket;

    std::shared_ptr<AVFrameWrapper> audioData = nullptr;
    std::shared_ptr<AVFrameWrapper> videoData = nullptr;

    double tVideoCacheLast_s = 0.0;
    double tAudioCacheLast_s = 0.0;

    if(!videoSamples_.empty())
        tVideoCacheLast_s = videoPtsToSeconds(videoSamples_.rbegin()->first);

    if(!audioSamples_.empty())
        tAudioCacheLast_s = audioPtsToSeconds(audioSamples_.rbegin()->first);

    while((tVideoCacheLast_s < tLast_s || tAudioCacheLast_s < tLast_s) && !reachedEndOfFile_)
    {
        av_packet_unref(pPacket);

        result = av_read_frame(pFormatContext_, pPacket);
        if(result == AVERROR_EOF)
        {
            LOG_IF(debug_, INFO) << "EOF. Flushing codecs.";

            videoData = processVideoFrame(0);
            audioData = processAudioFrame(0);
            reachedEndOfFile_ = true;
        }
        else if(result < 0)
        {
            LOG_IF(debug_, INFO) << "av_read_frame: " << err2str(result);

            return;
        }
        else
        {
            if(pPacket->stream_index == pVideoStream_->index)
            {
                videoData = processVideoFrame(pPacket);
            }

            if(pPacket->stream_index == pAudioStream_->index)
            {
                audioData = processAudioFrame(pPacket);
            }
        }

        if(videoData)
        {
            const double tVideo_s = videoPtsToSeconds((*videoData)->pts);

            if(videoSamples_.empty() && tVideo_s > tFirst_s)
            {
                LOG_IF(debug_, INFO) << "First video sample (" << tVideo_s << ") after required time (" << tFirst_s << ")";

                return;
            }

            {
                std::lock_guard<std::mutex> videoLock(videoSamplesMutex_);
                videoSamples_[(*videoData)->pts] = videoData;
            }

            tVideoCacheLast_s = videoPtsToSeconds(videoSamples_.rbegin()->first);
        }

        if(audioData)
        {
            const double tFirstAudio_s = audioPtsToSeconds((*audioData)->pts);

            if(audioSamples_.empty() && tFirstAudio_s > tFirst_s)
            {
                LOG_IF(debug_, INFO) << "First audio sample (" << tFirstAudio_s << ") after required time (" << tFirst_s << ")";

                return;
            }

            {
                std::lock_guard<std::mutex> audioLock(audioSamplesMutex_);
                audioSamples_[(*audioData)->pts] = audioData;
            }

            tAudioCacheLast_s = audioPtsToSeconds(audioSamples_.rbegin()->first);
        }
    }
}

void MediaSource::cleanCache(double tOld_s)
{
    const int64_t tVideoPtsOld = videoSecondsToPts(tOld_s);
    const int64_t tAudioPtsOld = audioSecondsToPts(tOld_s);

    {
        std::lock_guard<std::mutex> videoLock(videoSamplesMutex_);
        const auto& iterVideoMinLimit = videoSamples_.lower_bound(tVideoPtsOld);
        if(iterVideoMinLimit != videoSamples_.end())
            videoSamples_.erase(videoSamples_.begin(), iterVideoMinLimit);
    }

    {
        std::lock_guard<std::mutex> audioLock(audioSamplesMutex_);
        const auto& iterAudioMinLimit = audioSamples_.lower_bound(tAudioPtsOld);
        if(iterAudioMinLimit != audioSamples_.end())
            audioSamples_.erase(audioSamples_.begin(), iterAudioMinLimit);
    }
}

std::shared_ptr<AVFrameWrapper> MediaSource::processVideoFrame(AVPacket* pPacket)
{
    int result;

    result = avcodec_send_packet(pVideoCodecContext_, pPacket);
    if(result < 0)
    {
        LOG_IF(debug_, ERROR) << "Error in avcodec_send_packet (video): " << err2str(result);

        avcodec_flush_buffers(pVideoCodecContext_);
        return nullptr;
    }

    auto pWrapper = std::make_shared<AVFrameWrapper>();
    AVFrame* pFrame = *pWrapper;
    result = avcodec_receive_frame(pVideoCodecContext_, pFrame);
    if(result >= 0)
    {
        float pts_s = (float)pFrame->pts * pVideoStream_->time_base.num / pVideoStream_->time_base.den;

        LOG_IF(debug_, INFO) << "Video Frame: " << pVideoCodecContext_->frame_number << ", type: " << av_get_picture_type_char(pFrame->pict_type)
                             << ", PTS: " << pFrame->pts << "(" << std::setprecision(6) << pts_s << "), format: " << pFrame->format;

        return pWrapper;
    }
    else if(result == AVERROR(EAGAIN))
    {
        return nullptr;
    }
    else
    {
        LOG_IF(debug_, ERROR) << "avcodec_receive_frame (video) result: " << err2str(result);

        avcodec_flush_buffers(pVideoCodecContext_);
        return nullptr;
    }
}

std::shared_ptr<AVFrameWrapper> MediaSource::processAudioFrame(AVPacket* pPacket)
{
    int result;

    result = avcodec_send_packet(pAudioCodecContext_, pPacket);
    if(result < 0)
    {
        LOG_IF(debug_, ERROR) << "Error in avcodec_send_packet (audio): " << err2str(result);

        avcodec_flush_buffers(pAudioCodecContext_);
        return nullptr;
    }

    auto pWrapper = std::make_shared<AVFrameWrapper>();
    AVFrame* pFrame = *pWrapper;
    result = avcodec_receive_frame(pAudioCodecContext_, pFrame);
    if(result >= 0)
    {
        float pts_s = (float)pFrame->pts * pAudioStream_->time_base.num / pAudioStream_->time_base.den;

        LOG_IF(debug_, INFO) << "Audio Frame: " << pAudioCodecContext_->frame_number// << ", type: " << av_get_picture_type_char(pFrame->pict_type)
                             << ", PTS: " << pFrame->pts << "(" << std::setprecision(6) << pts_s << "), format: " << pFrame->format
                             << ", linesize0: " << pFrame->linesize[0] << ", linesize1: " << pFrame->linesize[1]
                             << ", data0: " << std::hex << (int64_t)pFrame->data[0] << ", data1: " << (int64_t)pFrame->data[1] << ", data2: " << (int64_t)pFrame->data[2] << std::dec
                             << ", samples: " << pFrame->nb_samples;

        return pWrapper;
    }
    else if(result == AVERROR(EAGAIN))
    {
        return nullptr;
    }
    else
    {
        LOG_IF(debug_, ERROR) << "avcodec_receive_frame (audio) result: " << err2str(result);

        avcodec_flush_buffers(pAudioCodecContext_);
        return nullptr;
    }
}


double MediaSource::videoPtsToSeconds(int64_t pts) const
{
    return (double)pts * pVideoStream_->time_base.num / pVideoStream_->time_base.den;
}

int64_t MediaSource::videoSecondsToPts(double seconds) const
{
    return (int64_t)(seconds * pVideoStream_->time_base.den / pVideoStream_->time_base.num);
}

double MediaSource::audioPtsToSeconds(int64_t pts) const
{
    return (double)pts * pAudioStream_->time_base.num / pAudioStream_->time_base.den;
}

int64_t MediaSource::audioSecondsToPts(double seconds) const
{
    return (int64_t)(seconds * pAudioStream_->time_base.den / pAudioStream_->time_base.num);
}

std::string MediaSource::err2str(int errnum)
{
    std::string msg(AV_ERROR_MAX_STRING_SIZE, 0);

    av_strerror(errnum, msg.data(), msg.size());

    return msg;
}
