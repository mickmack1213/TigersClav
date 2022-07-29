#include "ImageComposer.hpp"
#include <stdexcept>

const std::string vsSimple_src =
    #include "shaders/imgui.vs"
;

const std::string fsYUV2Grey_src =
    #include "shaders/yuv_to_grey.fs"
;

const std::string fsYUV2RGB_src =
    #include "shaders/yuv_to_rgb.fs"
;

ImageComposer::ImageComposer(ImVec2 renderSize)
:renderSize_(renderSize)
{
    shaderYUV2Grey_ = std::make_unique<ShaderProgram>(vsSimple_src, fsYUV2Grey_src);
    if(!shaderYUV2Grey_)
        return;

    shaderYUV2RGB_ = std::make_unique<ShaderProgram>(vsSimple_src, fsYUV2RGB_src);
    if(!shaderYUV2RGB_)
        return;

    uniformLocationTex_ = shaderYUV2Grey_->getUniformLocation("Texture");
    uniformLocationProjMtx_ = shaderYUV2Grey_->getUniformLocation("ProjMtx");
    attribLocationVtxPos_ = shaderYUV2Grey_->getAttribLocation("Position");
    attribLocationVtxUV_ = shaderYUV2Grey_->getAttribLocation("UV");
    attribLocationVtxColor_ = shaderYUV2Grey_->getAttribLocation("Color");

    rgbUniformLocationTex_ = shaderYUV2RGB_->getUniformLocation("Texture");
    rgbUniformLocationTexVU_ = shaderYUV2RGB_->getUniformLocation("TextureVU");
    rgbUniformLocationProjMtx_ = shaderYUV2RGB_->getUniformLocation("ProjMtx");
    rgbAttribLocationVtxPos_ = shaderYUV2RGB_->getAttribLocation("Position");
    rgbAttribLocationVtxUV_ = shaderYUV2RGB_->getAttribLocation("UV");
    rgbAttribLocationVtxColor_ = shaderYUV2RGB_->getAttribLocation("Color");

    // Video rendering setup
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &imguiFramebuffer_);

    glGenFramebuffers(1, &framebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

    glGenTextures(1, &renderTexture_);

    glBindTexture(GL_TEXTURE_2D, renderTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, renderSize_.x, renderSize_.y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderTexture_, 0);

    GLenum drawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, drawBuffers);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Video framebuffer creation failed");

    glBindFramebuffer(GL_FRAMEBUFFER, imguiFramebuffer_);

    ImVec2 pos(0, 0);
    ImVec2 size = renderSize_;

    ImDrawVert vertices[] = {
        { { pos.x+size.x, pos.y        }, {1.0f, 0.0f}, 0xFFFFFFFF }, // top right
        { { pos.x+size.x, pos.y+size.y }, {1.0f, 1.0f}, 0xFFFFFFFF }, // bottom right
        { { pos.x,        pos.y+size.y }, {0.0f, 1.0f}, 0xFFFFFFFF }, // bottom left
        { { pos.x,        pos.y        }, {0.0f, 0.0f}, 0xFFFFFFFF }, // top left
    };

    unsigned int indices[] = { // for a quad
        0, 1, 3,   // first triangle
        1, 2, 3    // second triangle
    };

    glGenBuffers(1, &VBO_);
    glGenBuffers(1, &EBO_);
    glGenVertexArrays(1, &VAO_);

    // 1. bind Vertex Array Object
    glBindVertexArray(VAO_);
    // 2. copy our vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, VBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // 3. copy our index array in a element buffer for OpenGL to use
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    // 4. then set our vertex attributes pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // Texture for loading video data into it, TODO: single channel for now
    glGenTextures(1, &videoTexture_);
    glBindTexture(GL_TEXTURE_2D, videoTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 16, 16, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

    glGenTextures(1, &videoTextureVU_);
    glBindTexture(GL_TEXTURE_2D, videoTextureVU_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, 16, 8, 0, GL_RG, GL_UNSIGNED_BYTE, 0);
}

ImageComposer::~ImageComposer()
{
    glDeleteBuffers(1, &VBO_);
    glDeleteBuffers(1, &EBO_);
    glDeleteVertexArrays(1, &VAO_);
    glDeleteTextures(1, &renderTexture_);
    glDeleteTextures(1, &videoTexture_);
    glDeleteTextures(1, &videoTextureVU_);
    glDeleteFramebuffers(1, &framebuffer_);
}

void ImageComposer::begin()
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

    glViewport(0, 0, renderSize_.x, renderSize_.y);

    ImVec4 clearColor = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);

    glClearColor(clearColor.x * clearColor.w, clearColor.y * clearColor.w,
                 clearColor.z * clearColor.w, clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ImageComposer::drawVideoFrame(AVFrame* pFrame, ImVec2 pos, ImVec2 size)
{
    if(size.x < 0)
        size.x = renderSize_.x;

    if(size.y < 0)
        size.y = renderSize_.y;

    glBindTexture(GL_TEXTURE_2D, videoTexture_);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pFrame->width, pFrame->height, 0, GL_RED, GL_UNSIGNED_BYTE, pFrame->data[0]);

    // TODO: Consider using glTexSubImage2D for updates

    shaderYUV2Grey_->use();

    // projection matrix maps pixel coordinates to (-1,-1) to (1,1) UV coordinates on render texture
    const float w = renderSize_.x;
    const float h = renderSize_.y;
    const float ortho_projection[4][4] = {
        { 2.0f/w,   0.0f,  0.0f,  0.0f },
        {   0.0f, 2.0f/h,  0.0f,  0.0f },
        {   0.0f,   0.0f, -1.0f,  0.0f },
        {  -1.0f,  -1.0f,  0.0f,  1.0f },
    };
    glUniform1i(uniformLocationTex_, 0);
    glUniformMatrix4fv(uniformLocationProjMtx_, 1, GL_FALSE, &ortho_projection[0][0]);

    ImDrawVert vertices[] = {
        { { pos.x+size.x, pos.y        }, {1.0f, 0.0f}, 0xFFFFFFFF }, // top right
        { { pos.x+size.x, pos.y+size.y }, {1.0f, 1.0f}, 0xFFFFFFFF }, // bottom right
        { { pos.x,        pos.y+size.y }, {0.0f, 1.0f}, 0xFFFFFFFF }, // bottom left
        { { pos.x,        pos.y        }, {0.0f, 0.0f}, 0xFFFFFFFF }, // top left
    };

    glBindVertexArray(VAO_);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(attribLocationVtxPos_);
    glEnableVertexAttribArray(attribLocationVtxUV_);
    glEnableVertexAttribArray(attribLocationVtxColor_);
    glVertexAttribPointer(attribLocationVtxPos_,   2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(attribLocationVtxUV_,    2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(attribLocationVtxColor_, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void ImageComposer::drawVideoFrameRGB(AVFrame* pFrame, ImVec2 pos, ImVec2 size)
{
    if(size.x < 0)
        size.x = renderSize_.x;

    if(size.y < 0)
        size.y = renderSize_.y;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, videoTexture_);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pFrame->width, pFrame->height, 0, GL_RED, GL_UNSIGNED_BYTE, pFrame->data[0]);

//    glActiveTexture(GL_TEXTURE1);
//    glBindTexture(GL_TEXTURE_2D, videoTextureVU_);
//    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, pFrame->width, pFrame->height/2, 0, GL_RG, GL_UNSIGNED_BYTE, pFrame->data[1]);
//    glTexSubImage2D(GL_TEXTURE_2D, 0, )

    // TODO: Consider using glTexSubImage2D for updates

    shaderYUV2RGB_->use();

    // projection matrix maps pixel coordinates to (-1,-1) to (1,1) UV coordinates on render texture
    const float w = renderSize_.x;
    const float h = renderSize_.y;
    const float ortho_projection[4][4] = {
        { 2.0f/w,   0.0f,  0.0f,  0.0f },
        {   0.0f, 2.0f/h,  0.0f,  0.0f },
        {   0.0f,   0.0f, -1.0f,  0.0f },
        {  -1.0f,  -1.0f,  0.0f,  1.0f },
    };
    glUniform1i(rgbUniformLocationTex_, 0);
    glUniform1i(rgbUniformLocationTexVU_, 1);
    glUniformMatrix4fv(rgbUniformLocationProjMtx_, 1, GL_FALSE, &ortho_projection[0][0]);

    ImDrawVert vertices[] = {
        { { pos.x+size.x, pos.y        }, {1.0f, 0.0f}, 0xFFFFFFFF }, // top right
        { { pos.x+size.x, pos.y+size.y }, {1.0f, 1.0f}, 0xFFFFFFFF }, // bottom right
        { { pos.x,        pos.y+size.y }, {0.0f, 1.0f}, 0xFFFFFFFF }, // bottom left
        { { pos.x,        pos.y        }, {0.0f, 0.0f}, 0xFFFFFFFF }, // top left
    };

    glBindVertexArray(VAO_);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(rgbAttribLocationVtxPos_);
    glEnableVertexAttribArray(rgbAttribLocationVtxUV_);
    glEnableVertexAttribArray(rgbAttribLocationVtxColor_);
    glVertexAttribPointer(rgbAttribLocationVtxPos_,   2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(rgbAttribLocationVtxUV_,    2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(rgbAttribLocationVtxColor_, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void ImageComposer::end()
{
    glBindFramebuffer(GL_FRAMEBUFFER, imguiFramebuffer_);
}
