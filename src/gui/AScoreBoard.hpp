#pragma once

#include "blend2d.h"
#include "ssl_gc_referee_message.pb.h"
#include <memory>
#include <optional>

class AScoreBoard
{
public:
    AScoreBoard(const char* regularFont, const char* boldFont, int width, int height);

    virtual void update(const std::shared_ptr<Referee>& pRef) = 0;

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
};

