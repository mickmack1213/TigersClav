#include "VideoProducer.hpp"
#include "util/easylogging++.h"
#include "data/MediaEncoder.hpp"
#include "data/MediaSource.hpp"
#include <filesystem>

extern "C" {
#include <libavutil/imgutils.h>
}

VideoProducer::VideoProducer()
:workerDone_(false),
 shouldAbort_(false),
 totalDuration_s_(0.0),
 rendered_s_(0.0),
 pResizer_(0),
 perfTotalTime_(0.0f),
 perfDecodingTime_(0.0f),
 perfEncodingTime_(0.0f)
{
}

VideoProducer::~VideoProducer()
{
    shouldAbort_ = true;

    if(workThread_.joinable())
        workThread_.join();

    if(pResizer_)
        sws_freeContext(pResizer_);
}

void VideoProducer::addAllVideos(std::unique_ptr<Project>& pProject)
{
    const auto& finalCut = pProject->getGameLog()->getDirector().getFinalCut();

    if(finalCut.empty())
        return;

    addScoreBoardVideo(pProject->getGameLog());

    for(const auto& pCam : pProject->getCameras())
    {
        addCutVideo(pCam, finalCut);
        addArchiveCut(pCam, pProject->getGameLog());
    }
}

void VideoProducer::addCutVideo(std::shared_ptr<GameLog> pGameLog, std::shared_ptr<Camera> pCam)
{
    const auto& finalCut = pGameLog->getDirector().getFinalCut();

    if(finalCut.empty())
        return;

    addCutVideo(pCam, finalCut);
}

void VideoProducer::addArchiveVideo(std::shared_ptr<GameLog> pGameLog, std::shared_ptr<Camera> pCam)
{
    addArchiveCut(pCam, pGameLog);
}

void VideoProducer::addScoreBoardVideo(std::shared_ptr<GameLog> pGameLog)
{
    const auto& finalCut = pGameLog->getDirector().getFinalCut();

    if(finalCut.empty())
        return;

    scoreBoardVideo_.sourceFile = pGameLog->getFilename();
    scoreBoardVideo_.outFile = scoreBoardVideo_.sourceFile + ".mp4";
    scoreBoardVideo_.cut = finalCut;
}

void VideoProducer::start()
{
    workThread_ = std::thread(&VideoProducer::worker, this);
}

void VideoProducer::addArchiveCut(const std::shared_ptr<Camera>& pCam, std::shared_ptr<GameLog> pGameLog)
{
    if(pCam->getVideos().empty())
    {
        LOG(WARNING) << "Camera " << pCam->getName() << " has no videos, skipping.";
        return;
    }

    LOG(INFO) << "Preparing data for camera: " << pCam->getName();

    std::string outDir = std::filesystem::path(pCam->getVideos().front()->pVideo_->getFilename()).parent_path().string();

    CutVideo cutVideo;
    cutVideo.outFile = outDir + "/archive-" + pCam->getName() + ".mp4";

    Director::Cut dCut;
    dCut.tStart_ns_ = 0;
    dCut.tEnd_ns_ = pGameLog->getTotalDuration_ns();

    auto pieces = fillCut(dCut, pCam->getVideos());
    cutVideo.pieces.insert(cutVideo.pieces.end(), pieces.begin(), pieces.end());

    outVideos_.push_back(cutVideo);
}

void VideoProducer::addCutVideo(const std::shared_ptr<Camera>& pCam, const std::vector<Director::Cut>& finalCut)
{
    if(pCam->getVideos().empty())
    {
        LOG(WARNING) << "Camera " << pCam->getName() << " has no videos, skipping.";
        return;
    }

    LOG(INFO) << "Preparing data for camera: " << pCam->getName();

    std::string outDir = std::filesystem::path(pCam->getVideos().front()->pVideo_->getFilename()).parent_path().string();

    CutVideo cutVideo;
    cutVideo.outFile = outDir + "/cut-" + pCam->getName() + ".mp4";

    for(const auto& cut : finalCut)
    {
        auto pieces = fillCut(cut, pCam->getVideos());
        cutVideo.pieces.insert(cutVideo.pieces.end(), pieces.begin(), pieces.end());
    }

    outVideos_.push_back(cutVideo);

    cutVideo.outFile = outDir + "/archive-" + pCam->getName() + ".mp4";
    Director::Cut dCut;
    dCut.tStart_ns_ = 0;
}

std::vector<CutVideo::Piece> VideoProducer::fillCut(const Director::Cut& cut, const std::vector<std::shared_ptr<VideoRecording>>& recordings)
{
    // t_gamelog - tStart = t_rec
    // t_rec + tStart = t_gamelog

    std::vector<CutVideo::Piece> pieces;

    int64_t tCutWritePos_ns = cut.tStart_ns_;
    int64_t tCutDurationLeft_ns = cut.tEnd_ns_ - cut.tStart_ns_;

    for(const auto& rec : recordings)
    {
        int64_t tRecStart_ns = rec->tStart_ns_;
        int64_t tRecDuration_ns = rec->pVideo_->getDuration_s() * 1e9;
        int64_t tRecEnd_ns = tRecStart_ns + tRecDuration_ns;

        if(tRecEnd_ns < tCutWritePos_ns)
        {
            // this recording is completely before the cut, skip it
            continue;
        }

        if(tRecStart_ns > tCutWritePos_ns)
        {
            // this recording starts after our write position, so we are missing video data, fill with blank
            int64_t missingDuration_ns = tRecStart_ns - tCutWritePos_ns;
            if(missingDuration_ns > tCutDurationLeft_ns)
                missingDuration_ns = tCutDurationLeft_ns;

            CutVideo::Piece piece;
            piece.sourceFile.clear();
            piece.tStart_s = 0.0;
            piece.duration_s = missingDuration_ns * 1e-9;
            pieces.push_back(piece);

            tCutWritePos_ns = tRecStart_ns;
            tCutDurationLeft_ns -= missingDuration_ns;

            if(tCutDurationLeft_ns <= 0)
                break;
        }

        if(tRecStart_ns <= tCutWritePos_ns && tRecEnd_ns > tCutWritePos_ns)
        {
            // this recording fills a part or all of the cut
            int64_t useableRecDuration_ns = tRecEnd_ns - tCutWritePos_ns;
            if(useableRecDuration_ns > tCutDurationLeft_ns)
                useableRecDuration_ns = tCutDurationLeft_ns;

            CutVideo::Piece piece;
            piece.sourceFile = rec->pVideo_->getFilename();
            piece.tStart_s = (tCutWritePos_ns - tRecStart_ns) * 1e-9;
            piece.duration_s = useableRecDuration_ns * 1e-9;
            pieces.push_back(piece);

            tCutWritePos_ns += useableRecDuration_ns;
            tCutDurationLeft_ns -= useableRecDuration_ns;

            if(tCutDurationLeft_ns <= 0)
                break;
        }
    }

    if(tCutDurationLeft_ns > 0)
    {
        // missing data at the end with no recoding for that time, fill with blank
        CutVideo::Piece piece;
        piece.sourceFile.clear();
        piece.tStart_s = 0.0;
        piece.duration_s = tCutDurationLeft_ns * 1e-9;
        pieces.push_back(piece);
    }

    LOG(INFO) << "Listing pieces for cut from " << cut.tStart_ns_*1e-9 << " to " << cut.tEnd_ns_ * 1e-9;

    for(const auto& piece : pieces)
        LOG(INFO) << "  - tStart: " << piece.tStart_s << ", dur: " << piece.duration_s << " (" << piece.sourceFile << ")";

    return pieces;
}

std::shared_ptr<MediaFrame> VideoProducer::blImageToMediaFrame(const BLImageData& image)
{
    int result;

    auto pMediaFrame = std::make_shared<MediaFrame>();
    pMediaFrame->pSamples = nullptr;

    pMediaFrame->videoTimeBase = av_make_q(1, 50);
    pMediaFrame->videoFramerate = av_make_q(50, 1);
    pMediaFrame->videoCodec = AV_CODEC_ID_H264;
    pMediaFrame->videoBitRate = 10 * 1000 * 1000LL;

    pMediaFrame->pImage = std::make_shared<AVFrameWrapper>();
    AVFrame* pFrame = *(pMediaFrame->pImage);

    pFrame->format = AV_PIX_FMT_YUV420P;
    pFrame->width = image.size.w;
    pFrame->height = image.size.h;

    result = av_frame_get_buffer(pFrame, 0);
    if(result < 0)
    {
        LOG(ERROR) << "Not enough memory for YUV frame.";
        return nullptr;
    }

    auto pRGBFrameWrapper = std::make_shared<AVFrameWrapper>();
    AVFrame* pRGBFrame = *pRGBFrameWrapper;

    pRGBFrame->format = AV_PIX_FMT_BGRA;
    pRGBFrame->width = image.size.w;
    pRGBFrame->height = image.size.h;

    result = av_frame_get_buffer(pRGBFrame, 0);
    if(result < 0)
    {
        LOG(ERROR) << "Not enough memory for RGB frame.";
        return nullptr;
    }

    memcpy(pRGBFrame->data[0], image.pixelData, image.stride * image.size.h);

    result = sws_scale(pResizer_, (const uint8_t* const*)pRGBFrame->data, pRGBFrame->linesize, 0, pRGBFrame->height, pFrame->data, pFrame->linesize);
    if(result < 0)
    {
        LOG(ERROR) << "Format conversion failed: " << result;
        return nullptr;
    }

    return pMediaFrame;
}

void VideoProducer::wipeFrame(std::shared_ptr<MediaFrame> pFrame)
{
    if(pFrame->pImage)
    {
        AVFrame* pImage = *pFrame->pImage;

        const AVPixelFormat format = static_cast<AVPixelFormat>(pImage->format);
        const int planes = av_pix_fmt_count_planes(format);

        ptrdiff_t linesizes[4];
        for (int i = 0; i < planes; ++i)
            linesizes[i] = pImage->linesize[i];

        av_image_fill_black(pImage->data, linesizes, format, pImage->color_range, pImage->width, pImage->height);
    }

    if(pFrame->pSamples)
    {
        AVFrame* pAudio = *pFrame->pSamples;
        av_samples_set_silence(pAudio->data, 0, pAudio->nb_samples, pAudio->channels, (enum AVSampleFormat)pAudio->format);
    }
}

void VideoProducer::worker()
{
    using namespace std::chrono_literals;

    totalDuration_s_ = 0.0;
    rendered_s_ = 0.0;

    for(const auto& outVideo : outVideos_)
    {
        for(const auto& piece : outVideo.pieces)
        {
            totalDuration_s_ += piece.duration_s;
        }
    }

    tWorkerStart_ = std::chrono::high_resolution_clock::now();

    if(!scoreBoardVideo_.sourceFile.empty())
    {
        for(const auto& cut : scoreBoardVideo_.cut)
        {
            totalDuration_s_ += (cut.tEnd_ns_ - cut.tStart_ns_) * 1e-9;
        }

        GameLog source(scoreBoardVideo_.sourceFile);

        while(!source.isLoaded())
            std::this_thread::sleep_for(10ms);

        MediaEncoder enc(scoreBoardVideo_.outFile);

        ScoreBoard board;

        auto imgData = board.getImageData();

        pResizer_ = sws_getContext(imgData.size.w, imgData.size.h, AV_PIX_FMT_BGRA, imgData.size.w, imgData.size.h,
                        AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
        if(!pResizer_)
        {
            LOG(ERROR) << "Could not create resizer.";
            return;
        }

        const int64_t tInc_ns = 20 * 1000 * 1000LL;

        for(const auto& cut : scoreBoardVideo_.cut)
        {
            for(int64_t t = cut.tStart_ns_; t < cut.tEnd_ns_; t += tInc_ns)
            {
                source.seekTo(t);
                auto optEntry = source.get();

                if(optEntry)
                {
                    board.update(optEntry->pReferee_);
                }
                else
                {
                    // TODO: not fatal? just use last image?
                    LOG(ERROR) << "Unable to get gamelog entry???";
                    return;
                }

                auto pFrame = blImageToMediaFrame(board.getImageData());

                if(enc.put(pFrame) < 0)
                {
                    LOG(ERROR) << "Encoding score board failed.";
                    return;
                }

                rendered_s_ += tInc_ns * 1e-9;

                if(shouldAbort_)
                {
                    workerDone_ = true;
                    return;
                }
            }
        }

        enc.close();
    }

    auto tScoreBoardComplete = std::chrono::high_resolution_clock::now();

    float scoreBoardTime = std::chrono::duration_cast<std::chrono::microseconds>(tScoreBoardComplete - tWorkerStart_).count() * 1e-6f;

    std::map<std::string, float> videoTimes;

    for(const auto& outVideo : outVideos_)
    {
        auto tVideoEncStart = std::chrono::high_resolution_clock::now();

        MediaEncoder enc(outVideo.outFile);

        auto firstPieceWithSource = std::find_if(outVideo.pieces.begin(), outVideo.pieces.end(), [](const CutVideo::Piece& p){ return !p.sourceFile.empty(); });
        if(firstPieceWithSource == outVideo.pieces.end())
        {
            LOG(ERROR) << "Video has no sources at all???";
            continue;
        }

        std::unique_ptr<MediaSource> pSrc = std::make_unique<MediaSource>(firstPieceWithSource->sourceFile, true);
        const double frameDelta_s = pSrc->getFrameDeltaTime();
        pSrc->seekTo(0.0);

        // generate an empty frame from this base data (black image, silent audio)
        std::shared_ptr<MediaFrame> pEmptyFrame;
        do
        {
            pEmptyFrame = pSrc->get();
            std::this_thread::sleep_for(1ms);
        }
        while(!pEmptyFrame);

        wipeFrame(pEmptyFrame);

        for(const auto& piece : outVideo.pieces)
        {
            if(piece.sourceFile.empty())
            {
                // no source, insert black
                for(double t = 0.0; t < piece.duration_s; t += frameDelta_s)
                {
                    enc.put(pEmptyFrame);
                    rendered_s_ += frameDelta_s;

                    if(shouldAbort_)
                    {
                        workerDone_ = true;
                        return;
                    }
                }
            }
            else
            {
                if(pSrc->getFilename() != piece.sourceFile)
                {
                    pSrc = std::make_unique<MediaSource>(piece.sourceFile, true);
                }

                pSrc->seekTo(piece.tStart_s);

                for(double t = 0.0; t < piece.duration_s; t += frameDelta_s)
                {
                    pSrc->seekTo(piece.tStart_s + t);

                    std::chrono::high_resolution_clock::time_point tDecStart = std::chrono::high_resolution_clock::now();

                    // wait until frame is available
                    std::shared_ptr<MediaFrame> pFrame;
                    do
                    {
                        std::this_thread::yield();
                        pFrame = pSrc->get();

                        if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tDecStart).count() > 10000)
                        {
                            LOG(ERROR) << "Timeout waiting for frame: " << t << ", file: " << pSrc->getFilename() << ", dur: " << pSrc->getDuration_s() << ", tell: " << pSrc->tell();
                            break;
                        }
                    }
                    while(!pFrame && !pSrc->hasReachedEndOfFile());

                    if(!pFrame)
                        break;

                    std::chrono::high_resolution_clock::time_point tDecEnd = std::chrono::high_resolution_clock::now();

                    if(enc.put(pFrame) < 0)
                    {
                        LOG(ERROR) << "Encoding video " << outVideo.outFile << " failed.";
                        break;
                    }

                    std::chrono::high_resolution_clock::time_point tEncEnd = std::chrono::high_resolution_clock::now();

                    float totalTime = std::chrono::duration_cast<std::chrono::microseconds>(tEncEnd - tDecStart).count() * 1e-6f;
                    float decodingTime = std::chrono::duration_cast<std::chrono::microseconds>(tDecEnd - tDecStart).count() * 1e-6f;
                    float encodingTime = std::chrono::duration_cast<std::chrono::microseconds>(tEncEnd - tDecEnd).count() * 1e-6f;

                    const float alpha = 0.95f;

                    perfTotalTime_ = alpha*perfTotalTime_ + (1.0f-alpha)*totalTime;
                    perfDecodingTime_ = alpha*perfDecodingTime_ + (1.0f-alpha)*decodingTime;
                    perfEncodingTime_ = alpha*perfEncodingTime_ + (1.0f-alpha)*encodingTime;

//                    pSrc->seekToNext();

                    rendered_s_ += frameDelta_s;

                    if(shouldAbort_)
                    {
                        workerDone_ = true;
                        return;
                    }
                }
            }
        }

        enc.close();

        auto tVideoEncEnd = std::chrono::high_resolution_clock::now();
        videoTimes[outVideo.outFile] = std::chrono::duration_cast<std::chrono::microseconds>(tVideoEncEnd - tVideoEncStart).count() * 1e-6f;
    }

    LOG(INFO) << "VideoProducer done. Timing: ";
    LOG(INFO) << "Score board: " << scoreBoardTime;
    for(const auto& t : videoTimes)
        LOG(INFO) << t.first << ": " << t.second;

    workerDone_ = true;
}

float VideoProducer::getElapsedTime() const
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - tWorkerStart_).count() * 1e-6f;
}

float VideoProducer::getEstimatedTimeLeft() const
{
    return getElapsedTime() * totalDuration_s_ / rendered_s_ - getElapsedTime();
}
