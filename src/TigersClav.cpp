#include "TigersClav.hpp"
#include <cstdio>
#include <cstdint>
#include <stdexcept>
#include <fstream>
#include <memory>
#include "util/gzstream.h"
#include "ImGuiFileDialog.h"
#include "util/CustomFont.h"
#include <chrono>

static int utf8_encode(char* out, uint32_t utf)
{
    if(utf <= 0x7F)
    {
        // Plain ASCII
        out[0] = (char)utf;
        out[1] = 0;
        return 1;
    }
    else if(utf <= 0x07FF)
    {
        // 2-byte unicode
        out[0] = (char)(((utf >> 6) & 0x1F) | 0xC0);
        out[1] = (char)(((utf >> 0) & 0x3F) | 0x80);
        out[2] = 0;
        return 2;
    }
    else if(utf <= 0xFFFF)
    {
        // 3-byte unicode
        out[0] = (char)(((utf >> 12) & 0x0F) | 0xE0);
        out[1] = (char)(((utf >> 6) & 0x3F) | 0x80);
        out[2] = (char)(((utf >> 0) & 0x3F) | 0x80);
        out[3] = 0;
        return 3;
    }
    else if(utf <= 0x10FFFF)
    {
        // 4-byte unicode
        out[0] = (char)(((utf >> 18) & 0x07) | 0xF0);
        out[1] = (char)(((utf >> 12) & 0x3F) | 0x80);
        out[2] = (char)(((utf >> 6) & 0x3F) | 0x80);
        out[3] = (char)(((utf >> 0) & 0x3F) | 0x80);
        out[4] = 0;
        return 4;
    }
    else
    {
        // error - use replacement character
        out[0] = (char)0xEF;
        out[1] = (char)0xBF;
        out[2] = (char)0xBD;
        out[3] = 0;
        return 0;
    }
}

TigersClav::TigersClav()
:lastFileOpenPath_(".")
{
    BLResult blResult = regularFontFace_.createFromFile("fonts/NotoSans-Regular.ttf");
    if(blResult)
        throw std::runtime_error("Regular font not found");

    blResult = symbolFontFace_.createFromFile("fonts/NotoSansSymbols2-Regular.ttf");
    if(blResult)
        throw std::runtime_error("Symbol font not found");

    glGenTextures(1, &gamestateTexture_);

    gamestateImage_.create(800, 300, BL_FORMAT_PRGB32);
}

void TigersClav::render()
{
    createGamestateOverlay();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(1000, 1200), ImGuiCond_Once);

    ImGui::Begin("Setup"/*, 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize*/);

    ImGui::Checkbox("Demo Window", &showDemoWindow_);

    ImGui::Image((void*)(intptr_t)gamestateTexture_, ImVec2(gamestateImage_.width(), gamestateImage_.height()));

    if(ImGui::Button("Load Gamelog"))
    {
        ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", "Gamelogs {.log,.gz}", lastFileOpenPath_, 1, nullptr, ImGuiFileDialogFlags_Modal);
    }

    if(ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, ImVec2(500, 500)))
    {
        // action if OK
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            lastFileOpenPath_ = ImGuiFileDialog::Instance()->GetCurrentPath() + "/";

            pGameLog_ = std::make_unique<SSLGameLog>(ImGuiFileDialog::Instance()->GetFilePathName(), std::set<SSLMessageType>{ MESSAGE_SSL_REFBOX_2013, MESSAGE_SSL_VISION_TRACKER_2020 });
        }

        // close
        ImGuiFileDialog::Instance()->Close();
    }

    if(pGameLog_)
    {
        SSLGameLogStats stats = pGameLog_->getStats();

        ImGui::Text("Gamelog: %s", pGameLog_->getFilename().c_str());
        ImGui::Text("Type: %s", stats.type.c_str());
        ImGui::Text("Format: %d", stats.formatVersion);
        ImGui::Text("Size: %.1fMB", stats.totalSize/(1024.0f*1024.0f));
        ImGui::Text("msgs: %u", stats.numMessages);
        ImGui::Text("vision2010: %u", stats.numMessagesPerType[MESSAGE_SSL_VISION_2010]);
        ImGui::Text("vision2014: %u", stats.numMessagesPerType[MESSAGE_SSL_VISION_2014]);
        ImGui::Text("ref:        %u", stats.numMessagesPerType[MESSAGE_SSL_REFBOX_2013]);
        ImGui::Text("tracker:    %u", stats.numMessagesPerType[MESSAGE_SSL_VISION_TRACKER_2020]);
        ImGui::Text("Duration: %.1fs", stats.duration_s);

        if(pGameLog_->isLoaded() && !pGameLog_->isEmpty(MESSAGE_SSL_REFBOX_2013))
        {
            auto refIter = pGameLog_->begin(MESSAGE_SSL_REFBOX_2013);
            auto optRef = pGameLog_->convertTo<Referee>(refIter);
            if(optRef)
            {
                ImGui::Text("Y: %s, B: %s", optRef->yellow().name().c_str(), optRef->blue().name().c_str());
            }
        }
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::End();
}

void TigersClav::createGamestateOverlay()
{
    // Attach a rendering context into `img`.
    BLContext ctx(gamestateImage_);

    // Clear the image.
    ctx.setCompOp(BL_COMP_OP_SRC_COPY);

    ctx.setFillStyle(BLRgba32(0xFF000000));
    ctx.fillAll();

    ctx.setFillStyle(BLRgba32(0xFFFFFFFF));

    BLFont symbolFontLarge;
    symbolFontLarge.createFromFace(symbolFontFace_, 50.0f);

    char digitalZero[5];
    utf8_encode(digitalZero, 0x1FBF0 + 0);

    ctx.fillUtf8Text(BLPoint(0, 50), symbolFontLarge, digitalZero);

    BLFont regularFont;
    regularFont.createFromFace(regularFontFace_, 24.0f);

    BLFontMetrics fm = regularFont.metrics();
    BLTextMetrics tm;
    BLGlyphBuffer gb;

    const char* pTeam1 = "TIGERs Mannheim";

    ctx.setFillStyle(BLRgba32(0xFF0000FF));

    gb.setUtf8Text(pTeam1, strlen(pTeam1));
    regularFont.shape(gb);
    regularFont.getTextMetrics(gb, tm);

    double team1Width = tm.boundingBox.x1 - tm.boundingBox.x0;

    ctx.fillGlyphRun(BLPoint(60, 80), regularFont, gb.glyphRun());
//    ctx.fillUtf8Text(BLPoint(60, 80), regularFont, pTeam1);

    ctx.setFillStyle(BLRgba32(0xFFFFFF00));
    ctx.fillUtf8Text(BLPoint(60 + team1Width, 80), regularFont, "ER-Force");

    ctx.setStrokeStyle(BLRgba32(0xFFFFFF00));
    ctx.setStrokeWidth(15);
    ctx.setStrokeStartCap(BL_STROKE_CAP_ROUND);
    ctx.setStrokeEndCap(BL_STROKE_CAP_TRIANGLE_REV);
    ctx.strokeLine(10, 200, 200, 200);

    // Yellow Card
    BLGradient linearY(BLLinearGradientValues(100, 0, 130, 50));
    linearY.addStop(0.0, BLRgba32(0xFFFFFF80));
    linearY.addStop(1.0, BLRgba32(0xFFFFFF00));

    ctx.setFillStyle(linearY);
    ctx.fillRoundRect(100, 0, 36, 50, 5);

    // Red Card
    BLGradient linearR(BLLinearGradientValues(150, 0, 180, 50));
    linearR.addStop(0.0, BLRgba32(0xFFFF8080));
    linearR.addStop(1.0, BLRgba32(0xFFFF0000));

    ctx.setFillStyle(linearR);
    ctx.fillRoundRect(150, 0, 36, 50, 5);

    // Detach the rendering context from `img`.
    ctx.end();

    BLImageData imgData;
    gamestateImage_.getData(&imgData);

    // Create a OpenGL texture identifier
    glBindTexture(GL_TEXTURE_2D, gamestateTexture_);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgData.size.w, imgData.size.h, 0, GL_BGRA, GL_UNSIGNED_BYTE, imgData.pixelData);
}
