#include "GameLog.hpp"

#include "util/easylogging++.h"

GameLog::GameLog(std::string filename)
:pGeometry_(nullptr)
{
    pGameLog_ = std::make_shared<SSLGameLog>(filename,
                    std::set<SSLMessageType>{ MESSAGE_SSL_REFBOX_2013, MESSAGE_SSL_VISION_TRACKER_2020, MESSAGE_SSL_VISION_2014 });

    refereeIter_ = pGameLog_->end(MESSAGE_SSL_REFBOX_2013);
}

int64_t GameLog::getTotalDuration_ns() const
{
    if(!pGameLog_->isLoaded())
        return 0;

    return pGameLog_->getLastTimestamp_ns() - pGameLog_->getFirstTimestamp_ns();
}

void GameLog::seekTo(int64_t timestamp_ns)
{
    if(!pGameLog_->isLoaded() || pGameLog_->isEmpty(MESSAGE_SSL_REFBOX_2013))
        return;

    const int64_t tGameLog_ns = pGameLog_->getFirstTimestamp_ns() + timestamp_ns;

    refereeIter_ = pGameLog_->findLastMsgBeforeTimestamp(MESSAGE_SSL_REFBOX_2013, tGameLog_ns);
}

void GameLog::seekToNext()
{
    if(!pGameLog_->isLoaded() || pGameLog_->isEmpty(MESSAGE_SSL_REFBOX_2013))
        return;

    if(refereeIter_ != pGameLog_->end(MESSAGE_SSL_REFBOX_2013))
        refereeIter_++;
}

void GameLog::seekToPrevious()
{
    if(!pGameLog_->isLoaded() || pGameLog_->isEmpty(MESSAGE_SSL_REFBOX_2013))
        return;

    if(refereeIter_ != pGameLog_->begin(MESSAGE_SSL_REFBOX_2013))
        refereeIter_--;
}

std::optional<GameLog::Entry> GameLog::get()
{
    if(!pGameLog_->isLoaded() ||
       pGameLog_->isEmpty(MESSAGE_SSL_VISION_TRACKER_2020) ||
       pGameLog_->isEmpty(MESSAGE_SSL_REFBOX_2013))
        return std::optional<GameLog::Entry>();

    if(refereeIter_ == pGameLog_->end(MESSAGE_SSL_REFBOX_2013))
        refereeIter_ = pGameLog_->begin(MESSAGE_SSL_REFBOX_2013);

    const int64_t tGameLog_ns = refereeIter_->first;

    auto trackerIter = pGameLog_->findLastMsgBeforeTimestamp(MESSAGE_SSL_VISION_TRACKER_2020, tGameLog_ns);

    if(trackerIter == pGameLog_->end(MESSAGE_SSL_VISION_TRACKER_2020))
        return std::optional<GameLog::Entry>();

    GameLog::Entry entry;
    entry.timestamp_ns_ = refereeIter_->first - pGameLog_->getFirstTimestamp_ns();
    entry.pReferee_ = pGameLog_->convertTo<Referee>(refereeIter_);
    entry.pTracker_ = pGameLog_->convertTo<TrackerWrapperPacket>(trackerIter);

    if(!entry.pReferee_ || !entry.pTracker_)
        return std::optional<GameLog::Entry>();

    trackerSources_[entry.pTracker_->uuid()] = entry.pTracker_->has_source_name() ? entry.pTracker_->source_name() : "Unknown";

    while(trackerIter != pGameLog_->end(MESSAGE_SSL_VISION_TRACKER_2020) && !preferredTracker_.empty() && entry.pTracker_->uuid() != preferredTracker_)
    {
        entry.pTracker_ = pGameLog_->convertTo<TrackerWrapperPacket>(trackerIter);
        if(!entry.pTracker_)
            break;

        trackerIter++;
    }

    if(!entry.pTracker_)
        return std::optional<GameLog::Entry>();

    return entry;
}

std::shared_ptr<const SSL_GeometryData> GameLog::getGeometry()
{
    if(pGeometry_)
        return pGeometry_;

    if(pGameLog_->isLoaded())
    {
        auto visionIter = pGameLog_->begin(MESSAGE_SSL_VISION_2014);
        while(visionIter != pGameLog_->end(MESSAGE_SSL_VISION_2014))
        {
            auto optVision = pGameLog_->convertTo<SSL_WrapperPacket>(visionIter);
            if(optVision && optVision->has_geometry())
            {
                LOG(INFO) << "Found Geometry Frame. "
                          << optVision->geometry().field().field_length() << "x" << optVision->geometry().field().field_width();

                pGeometry_ = std::make_shared<SSL_GeometryData>(optVision->geometry());

                break;
            }

            visionIter++;
        }
    }

    return pGeometry_;
}

std::list<std::string> GameLog::getFileDetails() const
{
    char buf[128];

    std::list<std::string> details;

    SSLGameLogStats stats = pGameLog_->getStats();

    snprintf(buf, sizeof(buf), "File: %s", pGameLog_->getFilename().c_str());
    details.push_back(std::string(buf));
    memset(buf, 0, sizeof(buf));

    snprintf(buf, sizeof(buf), "Duration: %.3fs", stats.duration_s);
    details.push_back(std::string(buf));
    memset(buf, 0, sizeof(buf));

    snprintf(buf, sizeof(buf), "Size: %.1fMB", stats.totalSize/(1024.0f*1024.0f));
    details.push_back(std::string(buf));
    memset(buf, 0, sizeof(buf));

    snprintf(buf, sizeof(buf), "msgs: %u", stats.numMessages);
    details.push_back(std::string(buf));
    memset(buf, 0, sizeof(buf));

    snprintf(buf, sizeof(buf), "vision2010: %u", stats.numMessagesPerType[MESSAGE_SSL_VISION_2010]);
    details.push_back(std::string(buf));
    memset(buf, 0, sizeof(buf));

    snprintf(buf, sizeof(buf), "vision2014: %u", stats.numMessagesPerType[MESSAGE_SSL_VISION_2014]);
    details.push_back(std::string(buf));
    memset(buf, 0, sizeof(buf));

    snprintf(buf, sizeof(buf), "ref:        %u", stats.numMessagesPerType[MESSAGE_SSL_REFBOX_2013]);
    details.push_back(std::string(buf));
    memset(buf, 0, sizeof(buf));

    snprintf(buf, sizeof(buf), "tracker:    %u", stats.numMessagesPerType[MESSAGE_SSL_VISION_TRACKER_2020]);
    details.push_back(std::string(buf));
    memset(buf, 0, sizeof(buf));

    snprintf(buf, sizeof(buf), "Type: %s", stats.type.c_str());
    details.push_back(std::string(buf));
    memset(buf, 0, sizeof(buf));

    snprintf(buf, sizeof(buf), "Format: %d", stats.formatVersion);
    details.push_back(std::string(buf));
    memset(buf, 0, sizeof(buf));

    return details;
}
