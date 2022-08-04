#pragma once

#include "SyncMarker.hpp"
#include "data/Video.hpp"

struct VideoRecording
{
    std::shared_ptr<Video> pVideo_;
    SyncMarker syncMarker_;

    int64_t offsetToGameLog_ns_; // gamelog t=0 to video t=0

//    int64_t tStart_ns;
//    int64_t duration_ns;
};

class Camera
{
public:
    Camera(std::string name);

    std::string getName() const { return name_; }
    int64_t getTotalDuration() const;
    AVFrame* getAVFrame(int64_t timestamp_ns);

private:
    std::string name_;
    std::list<std::shared_ptr<VideoRecording>> pVideos_;
};
