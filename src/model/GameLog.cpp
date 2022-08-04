#include "GameLog.hpp"

#include "util/easylogging++.h"

GameLog::GameLog(std::string filename)
:pGeometry_(nullptr)
{
    pGameLog_ = std::make_shared<SSLGameLog>(filename,
                    std::set<SSLMessageType>{ MESSAGE_SSL_REFBOX_2013, MESSAGE_SSL_VISION_TRACKER_2020, MESSAGE_SSL_VISION_2014 });

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
}

int64_t GameLog::getTotalDuration_ns() const
{
    if(!pGameLog_->isLoaded())
        return 0;

    return pGameLog_->getLastTimestamp_ns() - pGameLog_->getFirstTimestamp_ns();
}

std::optional<GameLog::Entry> GameLog::getEntry(int64_t timestamp_ns)
{
    if(!pGameLog_->isLoaded() ||
       pGameLog_->isEmpty(MESSAGE_SSL_VISION_TRACKER_2020) ||
       pGameLog_->isEmpty(MESSAGE_SSL_REFBOX_2013))
        return std::optional<GameLog::Entry>();

    const int64_t tGameLog_ns = pGameLog_->getFirstTimestamp_ns() + timestamp_ns;

    const auto& refIter = pGameLog_->findLastMsgBeforeTimestamp(MESSAGE_SSL_REFBOX_2013, tGameLog_ns);
    const auto& trackerIter = pGameLog_->findLastMsgBeforeTimestamp(MESSAGE_SSL_VISION_TRACKER_2020, tGameLog_ns);

    if(refIter == pGameLog_->end(MESSAGE_SSL_REFBOX_2013) || trackerIter == pGameLog_->end(MESSAGE_SSL_VISION_TRACKER_2020))
        return std::optional<GameLog::Entry>();

    GameLog::Entry entry;
    entry.pReferee_ = pGameLog_->convertTo<Referee>(refIter);
    entry.pTracker_ = pGameLog_->convertTo<TrackerWrapperPacket>(trackerIter);

    if(!entry.pReferee_ || !entry.pTracker_)
        return std::optional<GameLog::Entry>();

    return entry;
}

std::optional<int64_t> GameLog::getNextRefMsgTimestamp_ns(int64_t timestamp_ns)
{
    if(!pGameLog_->isLoaded() ||
       pGameLog_->isEmpty(MESSAGE_SSL_REFBOX_2013))
        return std::optional<int64_t>();

    const int64_t tGameLog_ns = pGameLog_->getFirstTimestamp_ns() + timestamp_ns;

    auto refIter = pGameLog_->findLastMsgBeforeTimestamp(MESSAGE_SSL_REFBOX_2013, tGameLog_ns);

    if(refIter == pGameLog_->end(MESSAGE_SSL_REFBOX_2013))
        return std::optional<int64_t>();

    refIter++;

    if(refIter == pGameLog_->end(MESSAGE_SSL_REFBOX_2013))
        return std::optional<int64_t>();

    return refIter->first - pGameLog_->getFirstTimestamp_ns();
}
