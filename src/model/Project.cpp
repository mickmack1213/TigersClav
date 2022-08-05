#include "Project.hpp"

int64_t Project::getTotalDuration() const
{
    int64_t gamelogDuration = pGameLog_ ? pGameLog_->getTotalDuration_ns() : 0;

    int64_t cameraDuration = 0;

    for(const auto& pCam : pCameras_)
    {
        cameraDuration = std::max(cameraDuration, pCam->getTotalDuration_ns());
    }

    return std::max(cameraDuration, gamelogDuration);
}

void Project::openGameLog(std::string filename)
{
    pGameLog_ = std::make_shared<GameLog>(filename);
}
