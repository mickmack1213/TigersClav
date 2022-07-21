#include "TigersClav.hpp"
#include <cstdio>

TigersClav::TigersClav()
{
}

void TigersClav::render()
{
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(500, 1200), ImGuiCond_Once);

    ImGui::Begin("Setup", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    ImGui::Checkbox("Demo Window", &showDemoWindow_);

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::End();
}

void TigersClav::createGamestateOverlay()
{
    BLImage img(480, 480, BL_FORMAT_PRGB32);

    // Attach a rendering context into `img`.
    BLContext ctx(img);

    // Clear the image.
    ctx.setCompOp(BL_COMP_OP_SRC_COPY);
    ctx.fillAll();

    // Fill some path.
    BLPath path;
    path.moveTo(26, 31);
    path.cubicTo(642, 132, 587, -136, 25, 464);
    path.cubicTo(882, 404, 144, 267, 27, 31);

    ctx.setCompOp(BL_COMP_OP_SRC_OVER);
    ctx.setFillStyle(BLRgba32(0xFFFFFFFF));
    ctx.fillPath(path);

    BLFontFace face;
    BLResult err = face.createFromFile("fonts/NotoSans-Regular.ttf");

    // We must handle a possible error returned by the loader.
    if (err) {
      printf("Failed to load a font-face (err=%u)\n", err);
      return;
    }

    BLFont font;
    font.createFromFace(face, 50.0f);

    ctx.setFillStyle(BLRgba32(0xFF0000FF));
    ctx.fillUtf8Text(BLPoint(60, 80), font, "Hello Blend2D!");

    ctx.rotate(0.785398);
    ctx.fillUtf8Text(BLPoint(250, 80), font, "Rotated Text");

    // Detach the rendering context from `img`.
    ctx.end();

    BLImageData imgData;
    img.getData(&imgData);

    // Let's use some built-in codecs provided by Blend2D.
    BLImageCodec codec;
    codec.findByName("PNG");
    img.writeToFile("bl-getting-started-1.png", codec);
}
