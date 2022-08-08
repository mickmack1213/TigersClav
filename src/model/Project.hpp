#pragma once

#include "GameLog.hpp"
#include "Camera.hpp"

class Project
{
public:
    void load(std::string filename);
    void save(std::string filename);

    void sync();

    int64_t getTotalDuration() const;
    int64_t getMinTStart() const;

    void openGameLog(std::string filename);

    const std::string& getFilename() const { return filename_; }
    std::shared_ptr<GameLog> getGameLog() { return pGameLog_; }
    std::vector<std::shared_ptr<Camera>>& getCameras() { return pCameras_; }

private:
    std::string filename_;
    std::shared_ptr<GameLog> pGameLog_;
    std::vector<std::shared_ptr<Camera>> pCameras_;
};
