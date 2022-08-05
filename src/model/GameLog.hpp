#pragma once

#include "SyncMarker.hpp"
#include "data/SSLGameLog.hpp"

#include <memory>
#include <vector>
#include <optional>
#include <list>

class GameLog
{
public:
    struct Entry
    {
        int64_t timestamp_ns_;
        std::shared_ptr<Referee> pReferee_;
        std::shared_ptr<TrackerWrapperPacket> pTracker_;
    };

    GameLog(std::string filename);

    int64_t getTotalDuration_ns() const;

    void seekTo(int64_t timestamp_ns);
    void seekToNext();
    void seekToPrevious();
    std::optional<Entry> get();

    const std::map<std::string, std::string>& getTrackerSources() const { return trackerSources_; };
    std::string getPreferredTrackerSourceUUID() const { return preferredTracker_; }
    void setPreferredTrackerSourceUUID(std::string source) { preferredTracker_ = source; }

    std::shared_ptr<const SSL_GeometryData> getGeometry();

    std::list<std::string> getFileDetails() const;
    bool isLoaded() const { return pGameLog_->isLoaded(); }
    void abortLoading() { pGameLog_->abortLoading(); }

private:
    std::shared_ptr<SSLGameLog> pGameLog_;
    std::vector<SyncMarker> syncMarkers_; // TODO: add methods

    SSLGameLog::MsgMapIter refereeIter_;

    std::shared_ptr<SSL_GeometryData> pGeometry_;

    std::map<std::string, std::string> trackerSources_;
    std::string preferredTracker_;
};
