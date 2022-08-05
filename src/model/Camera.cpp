#include "Camera.hpp"
#include "util/easylogging++.h"
#include <filesystem>
#include <algorithm>

VideoRecording::VideoRecording(std::string videoFilename)
:offsetToGameLog_ns_(0),
 frontGap_ns_(0)
{
    pVideo_ = std::make_shared<Video>(videoFilename);
}

std::string VideoRecording::getName() const
{
    if(!pVideo_->isLoaded())
        return "Load Error";

    return std::filesystem::path(pVideo_->getFilename()).stem().string();
}

Camera::Camera(std::string name)
:name_(name)
{
}

int64_t Camera::getTotalDuration_ns() const
{
    int64_t duration_ns = 0;

    for(const auto& pVideo : pVideos_)
    {
        duration_ns += pVideo->pVideo_->getDuration_ns() + pVideo->frontGap_ns_;
    }

    return duration_ns;
}

AVFrame* Camera::getAVFrame(int64_t timestamp_ns)
{
    int64_t currentOffset_ns = 0;

    for(const auto& pVideo : pVideos_)
    {
        int64_t videoStart = currentOffset_ns + pVideo->frontGap_ns_;
        int64_t videoEnd = videoStart + pVideo->pVideo_->getDuration_ns();

        if(timestamp_ns >= videoStart && timestamp_ns < videoEnd)
        {
            return pVideo->pVideo_->getFrameByTime(timestamp_ns - videoStart);
        }

        currentOffset_ns = videoEnd;
    }

    return 0;
}

float Camera::getFrameDeltaTime() const
{
    if(pVideos_.empty())
        return 0.0f;

    return pVideos_.front()->pVideo_->getFrameDeltaTime();
}

void Camera::addVideo(std::string name)
{
    auto findIter = std::find_if(pVideos_.begin(), pVideos_.end(), [&](auto pVideo) { return pVideo->pVideo_->getFilename() == name; });
    if(findIter != pVideos_.end())
    {
        LOG(WARNING) << "Video: " << name << " already loaded.";
        return;
    }

    std::shared_ptr<VideoRecording> pRec = std::make_shared<VideoRecording>(name);

    if(pRec->pVideo_->isLoaded())
        pVideos_.emplace_back(pRec);
}
