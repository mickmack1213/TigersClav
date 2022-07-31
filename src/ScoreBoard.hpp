#pragma once

#include "blend2d.h"
#include "ssl_gc_referee_message.pb.h"
#include <memory>
#include <optional>

class ScoreBoard
{
public:
    ScoreBoard();

    void update(std::shared_ptr<Referee> pRef);

    BLImageData getImageData();

private:
    enum class CardColor
    {
        YELLOW,
        RED
    };

    void drawCard(CardColor color, unsigned int amount, BLPoint pos, BLSize size = BLSize(36, 50));
    void drawScore(unsigned int score1, unsigned int score2, BLPoint pos);
    void drawTeamNames(std::string team1, std::string team2, BLPoint pos1, BLPoint pos2, BLSize maxSize);
    void drawStage(Referee::Stage stage, Referee::Command command, std::optional<int> stageTimeLeft_us, std::optional<int> actionTimeLeft_us, BLPoint pos);

    std::string refereeStageToString(Referee::Stage stage);
    void refereeCommandToTextAndColor(Referee::Command command, std::string& commandText, BLRgba32& color);

    bool hasStageTimeLeft(Referee::Stage stage);
    bool hasActionTimeLeft(Referee::Command command);

    BLFontFace regularFontFace_;
    BLFontFace boldFontFace_;

    BLImage gamestateImage_;
    BLContext ctx_;
};
