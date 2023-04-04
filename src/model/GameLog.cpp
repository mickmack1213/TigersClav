#include "GameLog.hpp"

#include "util/easylogging++.h"

GameLog::GameLog(std::string filename)
:pGeometry_(nullptr),
 filename_(filename)
{
    pGameLog_ = std::make_shared<SSLGameLog>(filename,
                    std::set<SSLMessageType>{ MESSAGE_SSL_REFBOX_2013, MESSAGE_SSL_VISION_TRACKER_2020, MESSAGE_SSL_VISION_2014 },
                    std::bind(&GameLog::onGameLogLoaded, this));

    refereeIter_ = pGameLog_->end(MESSAGE_SSL_REFBOX_2013);
}

void GameLog::onGameLogLoaded()
{
    if(pGameLog_->isEmpty(MESSAGE_SSL_REFBOX_2013))
        return;

    const int64_t firstTimestamp_ns = pGameLog_->getFirstTimestamp_ns();

    auto front = pGameLog_->begin(MESSAGE_SSL_REFBOX_2013);

    RefereeStateChange change;
    change.timestamp_ns_ = front->first - firstTimestamp_ns;
    change.pBefore_ = pGameLog_->convertTo<Referee>(front);
    change.pAfter_ = nullptr;

    uint32_t lastNumGoals = 0;
    Director::SceneState lastSceneState = Director::SceneState::HALT;

    std::vector<Director::Cut> runningCuts;
    Director::Cut activeCut {};

    auto geometry = getGeometry();
    int32_t fieldLength = geometry->field().field_length();
    int32_t goalWidth = geometry->field().goal_width();
    int32_t goalDepth = geometry->field().goal_depth();

    for(auto iter = pGameLog_->begin(MESSAGE_SSL_REFBOX_2013); iter != pGameLog_->end(MESSAGE_SSL_REFBOX_2013); iter++)
    {
        const int64_t tNow_ns = iter->first - firstTimestamp_ns;

        auto pRef = pGameLog_->convertTo<Referee>(iter);

        // find and store running scenes for precise goal localisation
        auto sceneState = Director::refStateToSceneState(pRef);

        if(lastSceneState != Director::SceneState::RUNNING && sceneState == Director::SceneState::RUNNING)
        {
            activeCut.tStart_ns_ = tNow_ns;
        }

        if(lastSceneState == Director::SceneState::RUNNING && sceneState != Director::SceneState::RUNNING)
        {
            activeCut.tEnd_ns_ = tNow_ns;
            runningCuts.push_back(activeCut);

            LOG(INFO) << "Added active cut: " << activeCut.tStart_ns_ << " -> " << activeCut.tEnd_ns_;
        }

        lastSceneState = sceneState;

        // TODO: also store score changes (awarded goals) and possible ball in goal times (by going back the tracker packets)
        uint32_t goalSum = pRef->yellow().score() + pRef->blue().score();

        if(lastNumGoals != goalSum)
        {
            // a goal was just awarded
            lastNumGoals = goalSum;

            LOG(INFO) << "Goal awarded time: " << tNow_ns;
            LOG(INFO) << "Buffered running cuts: " << runningCuts.size();

            int64_t goalTime_ns = 0;

            for(auto cutIter = runningCuts.rbegin(); cutIter != runningCuts.rend(); cutIter++)
            {
                auto trackerIter = pGameLog_->findLastMsgBeforeTimestamp(MESSAGE_SSL_VISION_TRACKER_2020, cutIter->tStart_ns_ + firstTimestamp_ns);
                while(trackerIter->first < cutIter->tEnd_ns_ + firstTimestamp_ns && trackerIter != pGameLog_->end(MESSAGE_SSL_VISION_TRACKER_2020))
                {
                    auto pTracker = pGameLog_->convertTo<TrackerWrapperPacket>(trackerIter);

                    if(pTracker->tracked_frame().balls_size() > 0)
                    {
                        auto ballPos = pTracker->tracked_frame().balls(0).pos();

                        bool ballInGoal = std::abs(ballPos.x()*1e3f) > fieldLength/2 && std::abs(ballPos.y()*1e3f) < goalWidth/2;
                        bool ballInGoalSafe = std::abs(ballPos.x()*1e3f) > (fieldLength/2 + goalDepth*0.2f) && std::abs(ballPos.y()*1e3f) < goalWidth/2;

                        if(ballInGoal)
                            goalTime_ns = trackerIter->first - firstTimestamp_ns;

                        if(ballInGoalSafe)
                        {
                            goalTime_ns = trackerIter->first - firstTimestamp_ns;
                            LOG(INFO) << "Ball entered goal at " << ballPos.x() << ", " << ballPos.y() << " at time: " << trackerIter->first - firstTimestamp_ns;
                            break;
                        }
                    }

                    trackerIter++;
                }

                if(goalTime_ns)
                    break;
            }

            if(goalTime_ns)
            {
                scoreTimes_ns_.push_back(goalTime_ns);
            }
            else
            {
                if(runningCuts.empty())
                {
                    // no goal found, no running cuts??? just take the goal time
                    scoreTimes_ns_.push_back(tNow_ns);
                }
                else
                {
                    // no goal found, use end of last running scene before goal awarded
                    scoreTimes_ns_.push_back(runningCuts.back().tEnd_ns_ - 1000000);
                }
            }

            runningCuts.clear();
        }

        if(pRef->stage() != change.pBefore_->stage() || pRef->command() != change.pBefore_->command())
        {
            change.timestamp_ns_ = tNow_ns;
            change.pAfter_ = pRef;

            stateChanges_.push_back(change);

            change.pBefore_ = pRef;
        }
    }

    LOG(INFO) << "Found " << stateChanges_.size() << " state changes";

    director_.orchestrate(stateChanges_, scoreTimes_ns_, getTotalDuration_ns());
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
    bool hasTrackerOrDetectionMsgs = !pGameLog_->isEmpty(MESSAGE_SSL_VISION_TRACKER_2020) || !pGameLog_->isEmpty(MESSAGE_SSL_VISION_2014);

    if(!pGameLog_->isLoaded() || !hasTrackerOrDetectionMsgs)
        return std::optional<GameLog::Entry>();

    if(refereeIter_ == pGameLog_->end(MESSAGE_SSL_REFBOX_2013))
        refereeIter_ = pGameLog_->begin(MESSAGE_SSL_REFBOX_2013);

    const int64_t tGameLog_ns = refereeIter_->first;

    GameLog::Entry entry;
    entry.timestamp_ns_ = refereeIter_->first - pGameLog_->getFirstTimestamp_ns();
    entry.pReferee_ = pGameLog_->convertTo<Referee>(refereeIter_);

    auto trackerIter = pGameLog_->findLastMsgBeforeTimestamp(MESSAGE_SSL_VISION_TRACKER_2020, tGameLog_ns);
    auto detectionIter = pGameLog_->findLastMsgBeforeTimestamp(MESSAGE_SSL_VISION_2014, tGameLog_ns);

    if(trackerIter != pGameLog_->end(MESSAGE_SSL_VISION_TRACKER_2020))
    {
        entry.pTracker_ = pGameLog_->convertTo<TrackerWrapperPacket>(trackerIter);

        if(entry.pTracker_)
        {
            trackerSources_[entry.pTracker_->uuid()] = entry.pTracker_->has_source_name() ? entry.pTracker_->source_name() : "Unknown";

            while(trackerIter != pGameLog_->end(MESSAGE_SSL_VISION_TRACKER_2020) && !preferredTracker_.empty() && entry.pTracker_->uuid() != preferredTracker_)
            {
                entry.pTracker_ = pGameLog_->convertTo<TrackerWrapperPacket>(trackerIter);
                if(!entry.pTracker_)
                    break;

                trackerIter++;
            }
        }
    }

    if(detectionIter != pGameLog_->end(MESSAGE_SSL_VISION_2014))
    {
        entry.pDetection_ = std::make_shared<SSL_DetectionFrame>(pGameLog_->convertTo<SSL_WrapperPacket>(detectionIter)->detection());
    }

    if(!entry.pReferee_ || (!entry.pTracker_ && !entry.pDetection_))
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
