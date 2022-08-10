#pragma once

#include "RefereeStateChange.hpp"
#include <vector>
#include <memory>

class Director
{
public:
    enum class SceneState
    {
        HALT,
        STOP,
        PREPARE, // Kickoff or Penalty
        RUNNING,
        TIMEOUT,
        BALL_PLACEMENT,
    };

    struct SceneChange
    {
        int64_t timestamp_ns_;
        SceneState before_;
        SceneState after_;
    };

    struct SceneBlock
    {
        int64_t tStart_ns_;
        int64_t tEnd_ns_;
        SceneState state_;

        SceneState before_;
        SceneState after_;
    };

    struct Cut
    {
        int64_t tStart_ns_;
        int64_t tEnd_ns_;
    };

    void orchestrate(const std::vector<RefereeStateChange>& stateChanges, int64_t duration_ns);

    const std::vector<SceneBlock>& getSceneBlocks() const { return sceneBlocks_; }
    const std::vector<Cut>& getFinalCut() const { return finalCut_; }

private:
    SceneState refStateToSceneState(std::shared_ptr<Referee> pRef) const;

    std::vector<SceneChange> sceneChanges_;
    std::vector<SceneBlock> sceneBlocks_;
    std::vector<Cut> finalCut_;
};
