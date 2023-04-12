#pragma once

#include "AScoreBoard.hpp"

class FancyScoreBoard : public AScoreBoard
{
public:
    FancyScoreBoard();

    void update(const Referee& ref) override;

private:
    void drawCard(CardColor color, unsigned int amount, BLPoint pos, int direction);

    void drawTeamNames(const std::string& team1, const std::string& team2, BLPoint pos1, BLPoint pos2, BLSize maxSize);
    void drawTime(const BLRgba32& textColor, int time_us);

    void drawCenteredText(const BLPoint &pos, const std::string& text, const BLFontFace& face, float size, const BLRgba32& color);

    static void refereeStageToTextAndDefaultCommand(Referee::Stage stage, std::string& stageText, Referee::Command& defaultCommand);
    static void refereeCommandToTextAndColor(Referee::Command command, std::string& commandText, BLRgba32& bgColor, BLRgba32& textColor);
};
