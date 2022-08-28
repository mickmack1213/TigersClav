#pragma once

#include "Project.hpp"
#include "gui/ScoreBoard.hpp"
#include <queue>

extern "C" {
#include <libswscale/swscale.h>
}

struct CutVideo
{
    struct Piece
    {
        std::string sourceFile;
        double tStart_s;
        double duration_s;
    };

    std::string outFile;
    std::vector<Piece> pieces;
};

struct RenderedVideo
{
    std::string sourceFile;
    std::string outFile;
    std::vector<Director::Cut> cut;
};

class VideoProducer
{
public:
    VideoProducer();
    ~VideoProducer();

    void addAllVideos(std::unique_ptr<Project>& pProject);
    void addCutVideo(std::shared_ptr<GameLog> pGameLog, std::shared_ptr<Camera> pCam);
    void addArchiveVideo(std::shared_ptr<GameLog> pGameLog, std::shared_ptr<Camera> pCam);
    void addScoreBoardVideo(std::shared_ptr<GameLog> pGameLog);
    void start();

    void abort() { shouldAbort_ = true; }
    float getProgress() const { return rendered_s_/totalDuration_s_; }
    bool isDone() const { return workerDone_; }

    float getPerfTotalTime() const { return perfTotalTime_; }
    float getPerfDecodingTime() const { return perfDecodingTime_; }
    float getPerfEncodingTime() const { return perfEncodingTime_; }

    float getElapsedTime() const;
    float getEstimatedTimeLeft() const;

private:
    void addCutVideo(const std::shared_ptr<Camera>& pCam, const std::vector<Director::Cut>& finalCut);
    void addArchiveCut(const std::shared_ptr<Camera>& pCam, std::shared_ptr<GameLog> pGameLog);
    std::vector<CutVideo::Piece> fillCut(const Director::Cut& cut, const std::vector<std::shared_ptr<VideoRecording>>& recordings);

    std::shared_ptr<MediaFrame> blImageToMediaFrame(const BLImageData& image);
    void wipeFrame(std::shared_ptr<MediaFrame> pFrame);

    void worker();

    std::vector<CutVideo> outVideos_;
    RenderedVideo scoreBoardVideo_;

    std::thread workThread_;
    std::atomic<bool> workerDone_;
    std::atomic<bool> shouldAbort_;

    float perfTotalTime_;
    float perfDecodingTime_;
    float perfEncodingTime_;

    double totalDuration_s_;
    double rendered_s_;

    struct SwsContext* pResizer_;

    std::chrono::high_resolution_clock::time_point tWorkerStart_;
};
