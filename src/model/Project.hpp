#pragma once

#include "GameLog.hpp"
#include "Camera.hpp"

class Project
{
public:
    int64_t getTotalDuration() const;

    void openGameLog(std::string filename);

    std::shared_ptr<GameLog> getGameLog() { return pGameLog_; }
    std::vector<std::shared_ptr<Camera>>& getCameras() { return pCameras_; }

private:
    std::shared_ptr<GameLog> pGameLog_;
    std::vector<std::shared_ptr<Camera>> pCameras_;
};
