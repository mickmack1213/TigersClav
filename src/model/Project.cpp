#include "Project.hpp"
#include "util/easylogging++.h"
#include "nlohmann/json.hpp"
#include <fstream>

using json = nlohmann::json;

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

void Project::load(std::string filename)
{
    std::ifstream in(filename);

    json data;

    in >> data;

    in.close();

    // TODO: maybe use absolute AND relative paths to make projects relative to sources

    try
    {
        json projectNode = data.at("Project");

        json gameLogNode = data.at("Gamelog");
        // TODO: load filename, sync markers, preferred tracker

        json cameraNode = data.at("Cameras");
        // TODO: load camera name, video recordings (name, sync marker)

        // TODO: parse data, we can put the full recreation logic here, no need to fill all files with json dependency
    }
    catch(json::exception& ex)
    {
        LOG(ERROR) << "Failed to load: " << filename << ", error: " << ex.what();
    }
}

void Project::save(std::string filename)
{
    std::ofstream out(filename);

    json data;
    // TODO: append all data

    out << std::setw(4) << data << std::endl;
}
