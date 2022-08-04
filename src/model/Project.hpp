#pragma once

#include "GameLog.hpp"
#include "Camera.hpp"

class Project
{
public:
    int64_t getTotalDuration();

    std::shared_ptr<GameLog> getGameLog();
    std::vector<std::shared_ptr<Camera>>& getCameras();

private:
    std::shared_ptr<GameLog> pGameLog_;
    std::vector<std::shared_ptr<Camera>> pCameras_;
};
