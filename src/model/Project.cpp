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

    return std::max(cameraDuration-getMinTStart(), gamelogDuration);
}

int64_t Project::getMinTStart() const
{
    int64_t tStartMin = 0;

    for(const auto& pCam : pCameras_)
    {
        if(!pCam->getVideos().empty())
            tStartMin = std::min(tStartMin, pCam->getVideos()[0]->tStart_ns_);
    }

    return tStartMin;
}

void Project::openGameLog(std::string filename)
{
    pGameLog_ = std::make_shared<GameLog>(filename);
}

void Project::load(std::string filename)
{
    namespace fs = std::filesystem;

    try
    {
        // Load file content
        std::ifstream in(filename);

        json jFile;

        in >> jFile;

        in.close();

        // cleanup old data
        pGameLog_.reset();
        pCameras_.clear();

        // Read project data
        fs::path savePrjDir = fs::path(jFile["project"]["path"]).parent_path();
        fs::path openPrjDir = fs::path(filename).parent_path();

        LOG(INFO) << "Saved project dir: " << savePrjDir;
        LOG(INFO) << "Open project dir: " << openPrjDir;

        // Read gamelog data
        if(jFile.contains("gamelog"))
        {
            json jGameLog = jFile.at("gamelog");

            fs::path gamelogPath = openPrjDir / fs::relative(jGameLog["path"], savePrjDir);

            openGameLog(gamelogPath.string());

            std::string trackerId = jGameLog["tracker_uuid"];
            if(!trackerId.empty())
                pGameLog_->setPreferredTrackerSourceUUID(trackerId);

            for(auto& jMarker : jGameLog["markers"])
            {
                SyncMarker marker;
                marker.name = jMarker[0];
                marker.timestamp_ns = jMarker[1];

                pGameLog_->getSyncMarkers().push_back(marker);
            }
        }

        // Read camera data and video recordings
        for(auto& jCam : jFile["cameras"])
        {
            auto pCam = std::make_shared<Camera>(jCam["name"]);

            for(auto& jRec : jCam["recordings"])
            {
                fs::path recordingPath = openPrjDir / fs::relative(jRec["path"], savePrjDir);
                std::shared_ptr<VideoRecording> pRec = std::make_shared<VideoRecording>(recordingPath.string());

                if(pRec->pVideo_->isLoaded())
                {
                    if(jRec.contains("marker"))
                    {
                        SyncMarker marker;
                        marker.name = jRec["marker"][0];
                        marker.timestamp_ns = jRec["marker"][1];

                        pRec->syncMarker_ = marker;
                    }

                    pCam->getVideos().emplace_back(pRec);
                }
            }

            pCameras_.emplace_back(pCam);
        }

        filename_ = filename;
    }
    catch(json::exception& ex)
    {
        LOG(ERROR) << "Failed to load: " << filename << ", error: " << ex.what();
    }
}

void Project::save(std::string filename)
{
    if(filename.find(".clav_prj") == std::string::npos)
        filename.append(".clav_prj");

    std::ofstream out(filename);

    json jFile;

    // Project data
    jFile["project"]["path"] = filename;

    // Gamelog data
    if(pGameLog_)
    {
        json jGamelog;

        jGamelog["path"] = pGameLog_->getFilename();
        jGamelog["tracker_uuid"] = pGameLog_->getPreferredTrackerSourceUUID();

        jGamelog["markers"] = json::array();

        for(const auto& marker : pGameLog_->getSyncMarkers())
        {
            jGamelog["markers"].push_back({ marker.name, marker.timestamp_ns });
        }

        jFile["gamelog"] = jGamelog;
    }

    // Camera data
    for(const auto& pCam : pCameras_)
    {
        json jCam;

        jCam["name"] = pCam->getName();

        for(const auto& pRec : pCam->getVideos())
        {
            json jRec;

            jRec["path"] = pRec->pVideo_->getFilename();

            if(pRec->syncMarker_)
            {
                jRec["marker"] = { pRec->syncMarker_->name, pRec->syncMarker_->timestamp_ns };
            }

            jCam["recordings"].push_back(jRec);
        }

        jFile["cameras"].push_back(jCam);
    }

    out << std::setw(4) << jFile << std::endl;

    filename_ = filename;
}

struct Rec
{
    bool synced;
    int64_t tStart_ns;
    int64_t duration_ns;
};

void Project::sync()
{
    if(!pGameLog_ || !pGameLog_->isLoaded() || pGameLog_->getSyncMarkers().empty())
        return;

    int64_t tStartMin_ns = 0;

    for(const auto& pCam : pCameras_)
    {
        std::vector<Rec> recs;

        for(auto recIter = pCam->getVideos().begin(); recIter != pCam->getVideos().end(); recIter++)
        {
            const auto pRec = *recIter;

            Rec rec;
            rec.synced = false;
            rec.tStart_ns = 0;
            rec.duration_ns = pRec->pVideo_->getDuration_ns();

            if(pRec->syncMarker_.has_value())
            {
                auto gameLogMarkerIter = std::find_if(pGameLog_->getSyncMarkers().begin(), pGameLog_->getSyncMarkers().end(),
                             [&](SyncMarker& marker) { return pRec->syncMarker_->name == marker.name; });

                if(gameLogMarkerIter != pGameLog_->getSyncMarkers().end())
                {
                    int64_t tMarkerGameLog_ns = gameLogMarkerIter->timestamp_ns;
                    int64_t tMarkerVideo_ns = pRec->syncMarker_->timestamp_ns;

                    int64_t tVideoStart_ns = tMarkerGameLog_ns - tMarkerVideo_ns; // video start time in "global gamelog" time

                    rec.tStart_ns = tVideoStart_ns;
                    rec.synced = true;
                }
            }

            recs.push_back(rec);
        }

        LOG(INFO) << "Camera: " << pCam->getName();
        for(auto rec : recs)
            LOG(INFO) << "  sync: " << rec.synced << ", tStart: " << rec.tStart_ns << ", dur: " << rec.duration_ns;

        // forward tStart update
        bool syncFound = false;
        int64_t tStartNext_ns = 0;

        for(size_t i = 0; i < recs.size(); i++)
        {
            if(recs[i].synced)
            {
                tStartMin_ns = std::min(tStartMin_ns, recs[i].tStart_ns);

                tStartNext_ns = recs[i].tStart_ns + recs[i].duration_ns;
                syncFound = true;
            }
            else
            {
                if(!syncFound)
                    continue;

                recs[i].synced = true;
                recs[i].tStart_ns = tStartNext_ns;
                tStartNext_ns += recs[i].duration_ns;
            }
        }

        // reverse tStart update
        syncFound = false;
        int64_t tStartLast_ns = 0;

        for(int64_t i = recs.size()-1; i >= 0; i--)
        {
            if(recs[i].synced)
            {
                tStartLast_ns = recs[i].tStart_ns;
                syncFound = true;
            }
            else
            {
                if(!syncFound)
                    continue;

                recs[i].synced = true;
                recs[i].tStart_ns = tStartLast_ns - recs[i].duration_ns;
                tStartLast_ns = recs[i].tStart_ns;

                tStartMin_ns = std::min(tStartMin_ns, recs[i].tStart_ns);
            }
        }

        LOG(INFO) << "Updated Camera: " << pCam->getName();
        for(auto rec : recs)
            LOG(INFO) << "  sync: " << rec.synced << ", tStart: " << rec.tStart_ns << ", dur: " << rec.duration_ns;

        for(size_t i = 0; i < pCam->getVideos().size(); i++)
        {
            auto pCur = pCam->getVideos()[i];
            pCur->tStart_ns_ = recs[i].tStart_ns;

            if(i > 0)
            {
                pCur->frontGap_ns_ = pCur->tStart_ns_ - (recs[i-1].tStart_ns + recs[i-1].duration_ns);
                if(pCur->frontGap_ns_ < 0)
                    pCur->frontGap_ns_ = 0;
            }
        }
    }

    for(const auto& pCam : pCameras_)
    {
        if(pCam->getVideos().empty())
            continue;

        auto pFirst = pCam->getVideos().front();

        pFirst->frontGap_ns_ = pFirst->tStart_ns_ - tStartMin_ns;
    }
}
