#pragma once

#include "Application.hpp"
#include "blend2d.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include "SSLGameLog.hpp"

class TigersClav : public Application
{
public:
    TigersClav();

    void render() override;

private:
    void createGamestateOverlay();

    std::unique_ptr<SSLGameLogLoader> pLogLoader_;

    std::string gamelogFileName_;

    BLFontFace regularFontFace_;
    BLFontFace symbolFontFace_;

    BLImage gamestateImage_;

    GLuint gamestateTexture_;
    ImVec2 gamestateSize_;
};
