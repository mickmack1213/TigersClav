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

    void setScoreBoardType(const std::string& type) { scoreBoardType_ = type; }

    const std::string& getFilename() const { return filename_; }
    std::shared_ptr<GameLog> getGameLog() { return pGameLog_; }
    std::vector<std::shared_ptr<Camera>>& getCameras() { return pCameras_; }
    const std::string& getScoreBoardType() const { return scoreBoardType_; }

private:
    std::string filename_;
    std::shared_ptr<GameLog> pGameLog_;
    std::vector<std::shared_ptr<Camera>> pCameras_;
    std::string scoreBoardType_;
};
