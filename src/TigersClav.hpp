#pragma once

#include "Application.hpp"
#include "blend2d.h"

#include <GL/gl.h>

#include "data/SSLGameLog.hpp"
#include "data/Video.hpp"
#include "util/ShaderProgram.hpp"
#include "gui/ImageComposer.hpp"
#include "gui/ScoreBoard.hpp"
#include "gui/FieldVisualizer.hpp"

class TigersClav : public Application
{
public:
    TigersClav();

    void render() override;

private:
    void createGamestateTextures();
    void drawGameLogPanel();
    void drawVideoPanel();

    std::unique_ptr<SSLGameLog> pGameLog_;
    std::unique_ptr<Video> pVideo_;
    std::unique_ptr<ImageComposer> pImageComposer_;
    std::unique_ptr<ScoreBoard> pScoreBoard_;
    std::unique_ptr<FieldVisualizer> pFieldVisualizer_;

    std::string lastFileOpenPath_;

    int gameLogRefPos_;
    bool gameLogRefPosHovered_;

    std::map<std::string, std::string> trackerSources_;
    std::string preferredTracker_;

    int64_t tPlayGamelog_ns_;

    int videoFramePos_;
    float videoDeltaRemainder_;
    bool videoFramePosHovered_;


    GLuint scoreBoardTexture_;
    GLuint fieldVisualizerTexture_;
};
