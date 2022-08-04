#pragma once

#include "SyncMarker.hpp"
#include "data/SSLGameLog.hpp"

#include <memory>
#include <vector>
#include <optional>

class GameLog
{
public:
    struct Entry
    {
        std::shared_ptr<Referee> pReferee_;
        std::shared_ptr<TrackerWrapperPacket> pTracker_;
    };

    GameLog(std::string filename);

    int64_t getTotalDuration_ns() const;
    std::optional<Entry> getEntry(int64_t timestamp_ns);

    std::optional<int64_t> getNextRefMsgTimestamp_ns(int64_t timestamp_ns);

    std::shared_ptr<const SSL_GeometryData> getGeometry() const { return pGeometry_; }

private:
    std::shared_ptr<SSLGameLog> pGameLog_;
    std::vector<SyncMarker> syncMarkers_; // TODO: add methods

    std::shared_ptr<SSL_GeometryData> pGeometry_;
};
