#pragma once

#include "util/ShaderProgram.hpp"
#include "data/Video.hpp"
#include "imgui.h"
#include <memory>

// Basically a render to texture framebuffer implementation
class ImageComposer
{
public:
    ImageComposer(ImVec2 renderSize);
    ~ImageComposer();

    void begin();
    void drawVideoFrameGrey(AVFrame* pFrame, ImVec2 pos = ImVec2(0, 0), ImVec2 size = ImVec2(-1, -1));
    void drawVideoFrameRGB(AVFrame* pFrame, ImVec2 pos = ImVec2(0, 0), ImVec2 size = ImVec2(-1, -1));
    void end();

    ImVec2 getRenderSize() const { return renderSize_; }
    GLuint getTexture() const { return renderTexture_; }

private:
    void updateRect(ImVec2 pos, ImVec2 size);
    void prepareShader(std::unique_ptr<ShaderProgram>& shader);

    ImVec2 renderSize_;

    GLint imguiFramebuffer_;
    GLuint framebuffer_;
    GLuint renderTexture_;

    std::unique_ptr<ShaderProgram> shaderYUV2Grey_;
    std::unique_ptr<ShaderProgram> shaderYUV2RGB_;

    GLuint VBO_;
    GLuint VAO_;
    GLuint EBO_;

    GLuint videoTextures_[3];
};
