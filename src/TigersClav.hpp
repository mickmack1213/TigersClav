#pragma once

#include "Application.hpp"
#include "blend2d.h"

#include <GL/gl.h>

#include "SSLGameLog.hpp"
#include "Video.hpp"
#include "util/ShaderProgram.hpp"
#include "ImageComposer.hpp"

class TigersClav : public Application
{
public:
    TigersClav();

    void render() override;

private:
    void createGamestateOverlay();

    std::unique_ptr<SSLGameLog> pGameLog_;
    std::unique_ptr<Video> pVideo_;
    std::unique_ptr<ImageComposer> pImageComposer_;

    std::string lastFileOpenPath_;

    BLFontFace regularFontFace_;
    BLFontFace symbolFontFace_;

    BLImage gamestateImage_;

    GLuint gamestateTexture_;
    ImVec2 gamestateSize_;

    bool drawVideoFrame_;
};
