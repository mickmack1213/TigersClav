#pragma once

#include "blend2d.h"
#include "ssl_gc_referee_message.pb.h"
#include <memory>
#include <string>

class AScoreBoard
{
public:
    AScoreBoard(const std::string& regularFont, const std::string& boldFont, int width, int height);
    virtual ~AScoreBoard() {}

    virtual void update(const Referee& ref) = 0;

    BLImageData getImageData();

protected:
    BLFontFace regularFontFace_;
    BLFontFace boldFontFace_;

    BLImage gamestateImage_;
    BLContext ctx_;

    enum class CardColor
    {
        YELLOW,
        RED
    };

    static bool hasStageTimeLeft(Referee::Stage stage);
    static bool hasActionTimeLeft(Referee::Command command);
    static double getTextWidth(const BLFont& font, BLGlyphBuffer& gb);
};
