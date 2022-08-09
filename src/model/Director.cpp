#include "Director.hpp"
#include "util/easylogging++.h"

void Director::orchestrate(const std::vector<RefereeStateChange>& stateChanges, int64_t duration_ns)
{
    // The director will reduce the detailed state changes to simpler SceneStates first and
    // then figure out which parts are worth keeping for the final cut.

    sceneChanges_.clear();
    sceneBlocks_.clear();
    finalCut_.clear();

    if(stateChanges.empty())
        return;

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

    sceneBlocks_.reserve(sceneChanges_.size() + 2);

    SceneBlock block;
    block.tStart_ns_ = 0;
    block.state_ = SceneState::HALT;

    for(const auto& change : sceneChanges_)
    {
        block.tEnd_ns_ = change.timestamp_ns_-1;

        sceneBlocks_.push_back(block);

        block.tStart_ns_ = change.timestamp_ns_;
        block.state_ = change.after_;
    }

    block.tEnd_ns_ = duration_ns;
    sceneBlocks_.push_back(block);
}

Director::SceneState Director::refStateToSceneState(std::shared_ptr<Referee> pRef) const
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
