#pragma once

#include "Application.hpp"

#include "model/Project.hpp"
#include "gui/ImageComposer.hpp"
#include "gui/ScoreBoard.hpp"
#include "gui/FieldVisualizer.hpp"
#include "util/ShaderProgram.hpp"

class TigersClav : public Application
{
public:
    TigersClav();

    void render() override;

private:
    void createGamestateTextures();
    void drawGameLogPanel();
    void drawVideoPanel();
    void drawProjectPanel();
    void drawSyncPanel();

    std::unique_ptr<Project> pProject_;
    std::unique_ptr<ImageComposer> pImageComposer_;
    std::unique_ptr<ScoreBoard> pScoreBoard_;
    std::unique_ptr<FieldVisualizer> pFieldVisualizer_;

    std::string lastFileOpenPath_;

    float gameLogTime_s_;
    bool gameLogAutoPlay_;
    bool gameLogSliderHovered_;

    int cameraIndex_;
    float cameraTime_s_;
    bool cameraAutoPlay_;
    bool cameraSliderHovered_;

    char camNameBuf_[128];

    std::deque<Video::CacheLevels> cacheLevelBuffer_;

    GLuint scoreBoardTexture_;
    GLuint fieldVisualizerTexture_;
};
