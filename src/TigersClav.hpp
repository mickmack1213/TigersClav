#pragma once

#include "Application.hpp"
#include "blend2d.h"

#include <GL/gl.h>
#include <GL/glext.h>

class TigersClav : public Application
{
public:
    TigersClav();

    void render() override;

private:
    void createGamestateOverlay();

    BLFontFace regularFontFace_;
    BLFontFace symbolFontFace_;

    BLImage gamestateImage_;

    GLuint gamestateTexture_;
    ImVec2 gamestateSize_;
};
