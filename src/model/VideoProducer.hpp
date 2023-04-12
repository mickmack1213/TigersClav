#pragma once

#include "Project.hpp"
#include "gui/AScoreBoard.hpp"
#include "data/MediaEncoder.hpp"
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
    VideoProducer(std::string outputBaseName, std::string scoreBoardType = "");
    ~VideoProducer();

    void addCutVideo(std::shared_ptr<GameLog> pGameLog, std::shared_ptr<Camera> pCam);
    void addGoalVideo(std::shared_ptr<GameLog> pGameLog, std::shared_ptr<Camera> pCam);
    void addArchiveVideo(std::shared_ptr<GameLog> pGameLog, std::shared_ptr<Camera> pCam);
    void useHwDecoder(bool enable) { useHwDecoder_ = enable; }
    void useHwEncoder(bool enable) { useHwEncoder_ = enable; }
    void start();

    void abort() { shouldAbort_ = true; }
    float getProgress() const { return rendered_s_/totalDuration_s_; }
    bool isDone() const { return workerDone_; }

    float getPerfTotalTime() const { return perfTotalTime_; }
    float getPerfDecodingTime() const { return perfDecodingTime_; }
    float getPerfEncodingTime() const { return perfEncodingTime_; }

    float getElapsedTime() const;
    float getEstimatedTimeLeft() const;

    std::string getCurrentStep() const { return currentStep_; }

    MediaEncoder::Timing getLastVideoTiming() const { return lastVideoTiming_; }
    MediaEncoder::Timing getLastAudioTiming() const { return lastAudioTiming_; }

private:
    void addCutVideo(const std::shared_ptr<Camera>& pCam, const std::vector<Director::Cut>& directorsCut, std::string typeName);
    void addRenderedVideo(const std::shared_ptr<GameLog>& pGameLog, const std::vector<Director::Cut>& directorsCut, std::string typeName);
    std::vector<CutVideo::Piece> fillCut(const Director::Cut& cut, const std::vector<std::shared_ptr<VideoRecording>>& recordings);

    std::shared_ptr<MediaFrame> blImageToMediaFrame(const BLImageData& image);
    void wipeFrame(std::shared_ptr<MediaFrame> pFrame);

    void worker();

    std::string outputBaseName_;
    std::string scoreBoardType_;

    std::vector<CutVideo> outVideos_;
    std::vector<RenderedVideo> scoreBoardVideos_;

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

    std::string currentStep_;

    MediaEncoder::Timing lastVideoTiming_;
    MediaEncoder::Timing lastAudioTiming_;

    bool useHwDecoder_;
    bool useHwEncoder_;
};
