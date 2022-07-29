#pragma once

#include "util/ShaderProgram.hpp"
#include "imgui.h"
#include "Video.hpp"
#include <memory>

// Basically a render to texture framebuffer implementation
class ImageComposer
{
public:
    ImageComposer(ImVec2 renderSize);
    ~ImageComposer();

    void begin();
    void drawVideoFrame(AVFrame* pFrame, ImVec2 pos = ImVec2(0, 0), ImVec2 size = ImVec2(-1, -1));
    void drawVideoFrameRGB(AVFrame* pFrame, ImVec2 pos = ImVec2(0, 0), ImVec2 size = ImVec2(-1, -1));
    void end();

    ImVec2 getRenderSize() const { return renderSize_; }
    GLuint getTexture() const { return renderTexture_; }

private:
    ImVec2 renderSize_;

    GLint imguiFramebuffer_;
    GLuint framebuffer_;
    GLuint renderTexture_;

    std::unique_ptr<ShaderProgram> shaderYUV2Grey_;
    std::unique_ptr<ShaderProgram> shaderYUV2RGB_;

    GLuint VBO_;
    GLuint VAO_;
    GLuint EBO_;

    GLuint videoTexture_;
    GLuint videoTextureVU_;

    GLuint uniformLocationTex_;
    GLuint uniformLocationProjMtx_;
    GLuint attribLocationVtxPos_;
    GLuint attribLocationVtxUV_;
    GLuint attribLocationVtxColor_;

    GLuint rgbUniformLocationTex_;
    GLuint rgbUniformLocationTexVU_;
    GLuint rgbUniformLocationProjMtx_;
    GLuint rgbAttribLocationVtxPos_;
    GLuint rgbAttribLocationVtxUV_;
    GLuint rgbAttribLocationVtxColor_;
};
