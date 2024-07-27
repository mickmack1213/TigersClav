#include "Director.hpp"
#include "util/easylogging++.h"
#include <iomanip>

void Director::orchestrate(const std::vector<RefereeStateChange>& stateChanges, std::vector<int64_t> scoreTimes_ns, int64_t duration_ns)
{
    // The director will reduce the detailed state changes to simpler SceneStates first and
    // then figure out which parts are worth keeping for the final cut.

    // cleanup
    sceneChanges_.clear();
    sceneBlocks_.clear();
    finalCut_.clear();
    goalCut_.clear();

    if(stateChanges.empty())
        return;

    // Reduce state changes to a simpler set of scene changes
    sceneChanges_.reserve(stateChanges.size());

    SceneChange scene;
    scene.before_ = refStateToSceneState(stateChanges.front().pBefore_);

    for(const auto& state : stateChanges)
    {
        SceneState stateAfter = refStateToSceneState(state.pAfter_);

        if(scene.before_ != stateAfter)
        {
            scene.timestamp_ns_ = state.timestamp_ns_;
            scene.after_ = stateAfter;

            sceneChanges_.push_back(scene);

            scene.before_ = stateAfter;
        }
    }

    LOG(INFO) << stateChanges.size() << " state changes reduced to " << sceneChanges_.size() << " scene changes.";

    size_t runningScenes = std::count_if(sceneChanges_.begin(), sceneChanges_.end(), [](const auto& c){ return c.after_ == SceneState::RUNNING; });

    LOG(INFO) << "Running scenes: " << runningScenes;

    // Create blocks for each scene for simple visualization
    sceneBlocks_.reserve(sceneChanges_.size() + 2);

    SceneBlock block;
    block.tStart_ns_ = 0;
    block.state_ = SceneState::HALT;
    block.before_ = SceneState::HALT;
    block.after_ = SceneState::HALT;

    for(const auto& change : sceneChanges_)
    {
        block.tEnd_ns_ = change.timestamp_ns_-1;
        block.after_ = change.after_;

        sceneBlocks_.push_back(block);

        block.tStart_ns_ = change.timestamp_ns_;
        block.before_ = change.before_;
        block.state_ = change.after_;
    }

    block.tEnd_ns_ = duration_ns;
    sceneBlocks_.push_back(block);

    // Compute cuts worth keeping
    const int64_t timePrepare2Running_ms = 5000;
    const int64_t timeOther2Running_ms = 2000;
    const int64_t timeRunning2Halt_ms = 5000;
    const int64_t timeRunning2Other_ms = 2000;
    const int64_t timeMaxPlacement_ms = 15000;
    const int64_t timeAfterPlacement_ms = 1000;
    const int64_t timeBridgeGaps_ms = 2000;

    std::vector<Cut> rawCut;

    for(const auto& block : sceneBlocks_)
    {
        Cut cut;
        bool keep = false;

        if(block.state_ == SceneState::RUNNING)
        {
            if(block.before_ == SceneState::PREPARE)
            {
                cut.tStart_ns_ = block.tStart_ns_ - timePrepare2Running_ms * 1000000LL;
            }
            else
            {
                cut.tStart_ns_ = block.tStart_ns_ - timeOther2Running_ms * 1000000LL;
            }

            if(block.after_ == SceneState::HALT)
            {
                cut.tEnd_ns_ = block.tEnd_ns_ + timeRunning2Halt_ms * 1000000LL;
            }
            else
            {
                cut.tEnd_ns_ = block.tEnd_ns_ + timeRunning2Other_ms * 1000000LL;
            }

            keep = true;
        }
        else if(block.state_ == SceneState::BALL_PLACEMENT)
        {
            if(block.tEnd_ns_ - block.tStart_ns_ < timeMaxPlacement_ms * 1000000LL &&
               (block.after_ == SceneState::RUNNING || block.after_ == SceneState::STOP))
            {
                cut.tStart_ns_ = block.tStart_ns_;
                cut.tEnd_ns_ = block.tEnd_ns_ + timeAfterPlacement_ms * 1000000LL;

                keep = true;
            }
        }

        if(keep)
        {
            rawCut.push_back(cut);
        }
    }

    LOG(INFO) << "Created " << rawCut.size() << " raw cuts.";

    if(rawCut.empty())
        return;

    Cut cutTemp = rawCut.front();

    for(size_t i = 1; i < rawCut.size(); i++)
    {
        const auto& cutCur = rawCut[i];

        if(cutCur.tStart_ns_ - timeBridgeGaps_ms * 1000000LL <= cutTemp.tEnd_ns_)
        {
            cutTemp.tEnd_ns_ = cutCur.tEnd_ns_;
        }
        else
        {
            finalCut_.push_back(cutTemp);
            cutTemp = cutCur;
        }
    }

    cutTemp.tEnd_ns_ += 60'000'000'000LL;

    finalCut_.push_back(cutTemp);

    int64_t totalDuration_ns = 0;
    for(const auto& cut : finalCut_)
    {
        totalDuration_ns += cut.tEnd_ns_ - cut.tStart_ns_;
    }

    LOG(INFO) << "Final cut has " << finalCut_.size() << " elements, duration: " << totalDuration_ns * 1e-9 << "s";

    for(const auto& cut : finalCut_)
    {
        LOG(INFO) << std::fixed << std::setprecision(4) << "   " << cut.tStart_ns_ * 1e-9 << " => " << cut.tEnd_ns_ * 1e-9;
    }

    // Create goal cut
    const int64_t timeAfterGoal_ms = 2000;
    const int64_t timeMaxGoalScene_ms = 4000;

    for(auto scoreTime : scoreTimes_ns)
    {
        for(int i = 0; i < sceneBlocks_.size(); i++)
        {
            const auto& block = sceneBlocks_[i];

            if(block.tStart_ns_ < scoreTime && scoreTime < block.tEnd_ns_)
            {
                // the goal was given within this block
                int64_t duration_ns = scoreTime - block.tStart_ns_;
                if(duration_ns > timeMaxGoalScene_ms * 1000000LL)
                    duration_ns = timeMaxGoalScene_ms * 1000000LL;

                Cut cut;
                cut.tEnd_ns_ = scoreTime + timeAfterGoal_ms * 1000000LL;
                cut.tStart_ns_ = scoreTime - duration_ns;
                goalCut_.push_back(cut);

                LOG(INFO) << "Goal cut. tStart: " << cut.tStart_ns_ * 1e-9 << ", tEnd: " << cut.tEnd_ns_ * 1e-9;

                break;
            }
        }
    }
}

Director::SceneState Director::refStateToSceneState(std::shared_ptr<Referee> pRef)
{
    if(!pRef)
        return SceneState::HALT;

    switch(pRef->stage())
    {
        case Referee_Stage_NORMAL_FIRST_HALF:
        case Referee_Stage_NORMAL_SECOND_HALF:
        case Referee_Stage_EXTRA_FIRST_HALF:
        case Referee_Stage_EXTRA_SECOND_HALF:
        case Referee_Stage_PENALTY_SHOOTOUT:
            break;

        case Referee_Stage_NORMAL_FIRST_HALF_PRE:
        case Referee_Stage_NORMAL_SECOND_HALF_PRE:
        case Referee_Stage_EXTRA_FIRST_HALF_PRE:
        case Referee_Stage_EXTRA_SECOND_HALF_PRE:
            break;

        case Referee_Stage_NORMAL_HALF_TIME:
        case Referee_Stage_EXTRA_TIME_BREAK:
        case Referee_Stage_EXTRA_HALF_TIME:
        case Referee_Stage_PENALTY_SHOOTOUT_BREAK:
        case Referee_Stage_POST_GAME:
        default:
            return SceneState::HALT;
    }

    switch(pRef->command())
    {
        case Referee_Command_STOP:
            return SceneState::STOP;

        case Referee_Command_NORMAL_START:
        case Referee_Command_FORCE_START:
        case Referee_Command_DIRECT_FREE_YELLOW:
        case Referee_Command_DIRECT_FREE_BLUE:
        case Referee_Command_INDIRECT_FREE_YELLOW:
        case Referee_Command_INDIRECT_FREE_BLUE:
            return SceneState::RUNNING;

        case Referee_Command_PREPARE_KICKOFF_YELLOW:
        case Referee_Command_PREPARE_KICKOFF_BLUE:
        case Referee_Command_PREPARE_PENALTY_YELLOW:
        case Referee_Command_PREPARE_PENALTY_BLUE:
            return SceneState::PREPARE;

        case Referee_Command_TIMEOUT_YELLOW:
        case Referee_Command_TIMEOUT_BLUE:
            return SceneState::TIMEOUT;

        case Referee_Command_BALL_PLACEMENT_YELLOW:
        case Referee_Command_BALL_PLACEMENT_BLUE:
            return SceneState::BALL_PLACEMENT;

        case Referee_Command_HALT:
        default:
            return SceneState::HALT;
    }
}
