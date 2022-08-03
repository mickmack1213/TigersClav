#pragma once

#include "IMediaSource.hpp"
#include "IGameLogSource.hpp"
#include "SSLGameLog.hpp"
#include "Video.hpp"

#include <memory>
#include <list>

struct SyncMarker
{
    std::string name;
    int64_t timestamp;
};

struct GameLogNode
{
    std::shared_ptr<SSLGameLog> pGameLog_;
    std::vector<SyncMarker> syncMarkers_;
};

struct VideoNode
{
    std::shared_ptr<Video> pVideo_;
    SyncMarker syncMarker_;
};

struct VideoContainer
{
    std::string name;
    std::list<std::shared_ptr<Video>> pVideos_;
    SyncMarker syncMarker_;
};


class Project
{
public:

private:
    std::shared_ptr<GameLogNode> pGameLog_;
    std::list<std::shared_ptr<VideoNode>> pVideos_;
    std::list<std::shared_ptr<VideoContainer>> pVideoContainers_;
};
