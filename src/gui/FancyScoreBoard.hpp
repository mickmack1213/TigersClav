#pragma once

#include "AScoreBoard.hpp"

class FancyScoreBoard: public AScoreBoard
{
public:
    FancyScoreBoard();

    void update(const std::shared_ptr<Referee>& pRef) final;

private:
    void drawCard(CardColor color, unsigned int amount, BLPoint pos, int direction);

    void drawTeamNames(const std::string& team1, const std::string& team2, BLPoint pos1, BLPoint pos2, BLSize maxSize);
    void drawStage(const std::string& stageText, std::string& commandText, Referee::Command standard, Referee::Command command, const BLRgba32& textColor, int time_us);
    void drawTime(Referee::Stage stage, const BLRgba32& textColor, int time_us);

    void centeredText(const BLPoint &pos, const char *str, const BLFontFace &face, float size, const BLRgba32 &color);

    static void refereeStageToString(Referee::Stage stage, Referee::Command &standard, std::string &stageText);
    static void refereeCommandToTextAndColor(Referee::Command command, std::string& commandText, BLRgba32& bgColor, BLRgba32& textColor);

};