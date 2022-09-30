#pragma once

#include "blend2d.h"
#include "ssl_gc_referee_message.pb.h"
#include <memory>
#include <optional>

class ScoreBoard
{
public:
    ScoreBoard(const char* regularFont, const char* boldFont, int width, int height);

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

class FancyScoreBoard: public ScoreBoard
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

class ProgrammerScoreBoard: public ScoreBoard
{
public:
    ProgrammerScoreBoard();

    void update(const std::shared_ptr<Referee>& pRef) final;

private:
    void drawCard(CardColor color, unsigned int amount, BLPoint pos, BLSize size = BLSize(36, 50));
    void drawScore(unsigned int score1, unsigned int score2, BLPoint pos);
    void drawTeamNames(std::string team1, std::string team2, BLPoint pos1, BLPoint pos2, BLSize maxSize);
    void drawStage(Referee::Stage stage, Referee::Command command, std::optional<int> stageTimeLeft_us, std::optional<int> actionTimeLeft_us, BLPoint pos);

    std::string refereeStageToString(Referee::Stage stage);
    void refereeCommandToTextAndColor(Referee::Command command, std::string& commandText, BLRgba32& color);
};
