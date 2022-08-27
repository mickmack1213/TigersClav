#include "ScoreBoard.hpp"

ScoreBoard::ScoreBoard()
{
    BLResult blResult = regularFontFace_.createFromFile("fonts/NotoSansMono-Regular.ttf");
    if(blResult)
        throw std::runtime_error("Regular font not found");

    blResult = boldFontFace_.createFromFile("fonts/RobotoSlab-Bold.ttf");
    if(blResult)
        throw std::runtime_error("Bold font not found");

    gamestateImage_.create(1200, 110, BL_FORMAT_PRGB32);

    update(std::make_shared<Referee>());
}

void ScoreBoard::update(std::shared_ptr<Referee> pRef)
{
    BLRgba32 underlineYellow(0xFFFFD700);
    BLRgba32 underlineBlue(0xFF0000FF);

    int yellowCards[2] = { pRef->yellow().yellow_card_times().size(), pRef->blue().yellow_card_times().size() };
    unsigned int redCards[2] = { pRef->yellow().red_cards(), pRef->blue().red_cards() };

    ctx_.begin(gamestateImage_);

    // Clear the image.
    ctx_.setCompOp(BL_COMP_OP_SRC_COPY);
//    ctx_.setFillStyle(BLRgba32(0xFF330000));
//    ctx_.setFillStyle(BLRgba32(0xFFBBBBBB));
    ctx_.setFillStyle(BLRgba32(0xFF00FF00));
    ctx_.fillAll();

    drawCard(CardColor::RED, redCards[0], BLPoint(0, 0));
    drawCard(CardColor::YELLOW, yellowCards[0], BLPoint(50, 0));

    // Background for team names
    ctx_.setFillStyle(BLRgba32(0xFFDDDDDD));
    ctx_.fillRect(100, 0, 1000, 50);

    // Yellow team underline
    ctx_.setFillStyle(BLRgba32(0xFFFFD700));
    ctx_.fillRect(100, 50, 420, 10);

    // Blue team underline
    ctx_.setFillStyle(BLRgba32(0xFF0000FF));
    ctx_.fillRect(680, 50, 420, 10);

    // Team names
    drawTeamNames(pRef->yellow().name(), pRef->blue().name(), BLPoint(310, 25), BLPoint(890, 25), BLSize(400, 60));

    // Background for score
    ctx_.setFillStyle(BLRgba32(0xFF444444));
    ctx_.fillRect(520, 0, 160, 50);

    // Actual score
    drawScore(pRef->yellow().score(), pRef->blue().score(), BLPoint(600, 43));

    drawCard(CardColor::YELLOW, yellowCards[1], BLPoint(1114, 0));
    drawCard(CardColor::RED, redCards[1], BLPoint(1164, 0));

    std::optional<int> stageTimeLeft;
    if(hasStageTimeLeft(pRef->stage()))
        stageTimeLeft = pRef->stage_time_left();

    std::optional<int> actionTimeLeft;
    if(hasActionTimeLeft(pRef->command()))
        actionTimeLeft = pRef->current_action_time_remaining();

    drawStage(pRef->stage(), pRef->command(), stageTimeLeft, actionTimeLeft, BLPoint(100, 70));

    // Detach the rendering context from `img`.
    ctx_.end();
}

BLImageData ScoreBoard::getImageData()
{
    BLImageData imgData;
    gamestateImage_.getData(&imgData);

    return imgData;
}

void ScoreBoard::drawStage(Referee::Stage stage, Referee::Command command, std::optional<int> stageTimeLeft_us, std::optional<int> actionTimeLeft_us, BLPoint pos)
{
    std::string stageText = refereeStageToString(stage);

    std::string commandText;
    BLRgba32 bgColor;
    refereeCommandToTextAndColor(command, commandText, bgColor);

    if(actionTimeLeft_us && *actionTimeLeft_us > 0)
    {
        int time_us = *actionTimeLeft_us;
        if(time_us < 0)
            time_us = 0;

        int time_s = time_us / 1000000;

        commandText += " (" + std::to_string(time_s) + "s)";
    }

    ctx_.setFillStyle(BLRgba32(0xFF444444));
    ctx_.fillRect(100, pos.y, 1000, 40);

    BLTextMetrics tm;
    BLGlyphBuffer gb;
    BLFont regularFont;

    regularFont.createFromFace(regularFontFace_, 30.0f);
    BLFontMetrics fm = regularFont.metrics();

    gb.setUtf8Text(stageText.c_str());
    regularFont.shape(gb);
    regularFont.getTextMetrics(gb, tm);

    ctx_.setFillStyle(BLRgba32(0xFFFFFFFF));
    ctx_.fillGlyphRun(BLPoint(525, pos.y+30), regularFont, gb.glyphRun());

    if(stageTimeLeft_us)
    {
        int time_us = *stageTimeLeft_us;
        if(time_us < 0)
            time_us = 0;

        int time_s = time_us / 1000000;
        int time_min = time_s / 60;
        time_s %= 60;

        char buf[32];
        snprintf(buf, sizeof(buf), "%2d:%02d", time_min, time_s);

        ctx_.setFillStyle(BLRgba32(0xFF000000));
        ctx_.fillRect(1000, pos.y, 100, 40);

        ctx_.setFillStyle(BLRgba32(0xFFFFFFFF));
        ctx_.fillUtf8Text(BLPoint(1000, pos.y+30), regularFont, buf);
    }

    ctx_.setFillStyle(BLRgba32(bgColor));
    ctx_.fillRect(100, pos.y, 420, 40);

    ctx_.setFillStyle(BLRgba32(0xFFFFFFFF));
    ctx_.fillUtf8Text(BLPoint(110, pos.y+30), regularFont, commandText.c_str());
}

void ScoreBoard::drawTeamNames(std::string team1, std::string team2, BLPoint pos1, BLPoint pos2, BLSize maxSize)
{
    BLFont regularFont;
    BLTextMetrics tm[2];
    BLGlyphBuffer gb[2];

    double nameWidths[2] = {0, 0};
    float fontHeight = 0;
    float fontSize = 8.0f;
    float fontOffset = 0;

    while(nameWidths[0] < maxSize.w && nameWidths[1] < maxSize.w && fontHeight < maxSize.h)
    {
        regularFont.createFromFace(regularFontFace_, fontSize);
        BLFontMetrics fm = regularFont.metrics();

        gb[0].setUtf8Text(team1.c_str());
        regularFont.shape(gb[0]);
        regularFont.getTextMetrics(gb[0], tm[0]);

        gb[1].setUtf8Text(team2.c_str());
        regularFont.shape(gb[1]);
        regularFont.getTextMetrics(gb[1], tm[1]);

        nameWidths[0] = tm[0].advance.x;
        nameWidths[1] = tm[1].advance.x;

        fontHeight = fm.ascent + fm.descent;

        fontOffset = fm.strikethroughPosition;

        fontSize += 1.0f;
    }

    ctx_.setFillStyle(BLRgba32(0xFF000000));

    ctx_.fillGlyphRun(BLPoint(pos1.x-nameWidths[0]/2, pos1.y-fontOffset), regularFont, gb[0].glyphRun());
    ctx_.fillGlyphRun(BLPoint(pos2.x-nameWidths[1]/2, pos2.y-fontOffset), regularFont, gb[1].glyphRun());
}

void ScoreBoard::drawCard(CardColor color, unsigned int amount, BLPoint pos, BLSize size)
{
    if(amount == 0)
        return;

    BLGradient gradient(BLLinearGradientValues(pos.x, pos.y, pos.x+size.w*0.8, pos.y+size.h));

    if(color == CardColor::YELLOW)
    {
        gradient.addStop(0.0, BLRgba32(0xFFFFFF80));
        gradient.addStop(1.0, BLRgba32(0xFFFFFF00));
    }
    else
    {
        gradient.addStop(0.0, BLRgba32(0xFFFF8080));
        gradient.addStop(1.0, BLRgba32(0xFFFF0000));
    }

    ctx_.setFillStyle(gradient);
    ctx_.fillRoundRect(pos.x, pos.y, size.w, size.h, size.h*0.1);

    ctx_.setStrokeStyle(BLRgba32(0xFF000000));
    ctx_.setStrokeWidth(1);
    ctx_.strokeRoundRect(pos.x, pos.y, size.w, size.h, size.h*0.1);

    if(amount > 1)
    {
        BLFont regularFont;
        regularFont.createFromFace(regularFontFace_, size.h*0.9);

        ctx_.setFillStyle(BLRgba32(0xFF444444));
        ctx_.fillUtf8Text(BLPoint(pos.x+size.w*0.12, pos.y+size.h*0.82), regularFont, std::to_string(amount).c_str());
    }
}

void ScoreBoard::drawScore(unsigned int score1, unsigned int score2, BLPoint pos)
{
    BLTextMetrics tm;
    BLGlyphBuffer gb;
    BLFont fontLarge;

    fontLarge.createFromFace(boldFontFace_, 50.0f);
    BLFontMetrics fm = fontLarge.metrics();

    std::string scores[2] = { std::to_string(score1 % 100), std::to_string(score2 % 100) };
    std::string dash("-");

    ctx_.setFillStyle(BLRgba32(0xFFFFFFFF));

    gb.setUtf8Text(scores[0].c_str());
    fontLarge.shape(gb);
    fontLarge.getTextMetrics(gb, tm);
    ctx_.fillGlyphRun(BLPoint(pos.x - 40 - tm.advance.x/2, pos.y), fontLarge, gb.glyphRun());

    gb.setUtf8Text(dash.c_str());
    fontLarge.shape(gb);
    fontLarge.getTextMetrics(gb, tm);
    ctx_.fillGlyphRun(BLPoint(pos.x - 0 - tm.advance.x/2, pos.y), fontLarge, gb.glyphRun());

    gb.setUtf8Text(scores[1].c_str());
    fontLarge.shape(gb);
    fontLarge.getTextMetrics(gb, tm);
    ctx_.fillGlyphRun(BLPoint(pos.x + 40 - tm.advance.x/2, pos.y), fontLarge, gb.glyphRun());
}

bool ScoreBoard::hasStageTimeLeft(Referee::Stage stage)
{
    switch(stage)
    {
        case Referee_Stage_NORMAL_FIRST_HALF:
        case Referee_Stage_NORMAL_HALF_TIME:
        case Referee_Stage_NORMAL_SECOND_HALF:
        case Referee_Stage_EXTRA_TIME_BREAK:
        case Referee_Stage_EXTRA_FIRST_HALF:
        case Referee_Stage_EXTRA_HALF_TIME:
        case Referee_Stage_EXTRA_SECOND_HALF:
        case Referee_Stage_PENALTY_SHOOTOUT_BREAK:
            return true;
        default:
            return false;
    }
}

bool ScoreBoard::hasActionTimeLeft(Referee::Command command)
{
    switch(command)
    {
        // these stages NEVER have an action time
        case Referee_Command_HALT:
        case Referee_Command_STOP:
        case Referee_Command_TIMEOUT_YELLOW:
        case Referee_Command_TIMEOUT_BLUE:
        case Referee_Command_GOAL_YELLOW:
        case Referee_Command_GOAL_BLUE:
        case Referee_Command_FORCE_START:
        case Referee_Command_PREPARE_KICKOFF_YELLOW:
        case Referee_Command_PREPARE_KICKOFF_BLUE:
        case Referee_Command_PREPARE_PENALTY_YELLOW:
        case Referee_Command_PREPARE_PENALTY_BLUE:
            return false;

        // Usually, these stages have an action time, but we don't show it
        case Referee_Command_NORMAL_START:
        case Referee_Command_DIRECT_FREE_YELLOW:
        case Referee_Command_DIRECT_FREE_BLUE:
        case Referee_Command_INDIRECT_FREE_YELLOW:
        case Referee_Command_INDIRECT_FREE_BLUE:
            return false;

        case Referee_Command_BALL_PLACEMENT_YELLOW:
        case Referee_Command_BALL_PLACEMENT_BLUE:
            return true;

        default:
            return false;
    }
}

std::string ScoreBoard::refereeStageToString(Referee::Stage stage)
{
    std::string stageText;

    switch(stage)
    {
        case Referee_Stage_NORMAL_FIRST_HALF_PRE:
        case Referee_Stage_NORMAL_FIRST_HALF:
            stageText = "1st Half";
            break;
        case Referee_Stage_NORMAL_HALF_TIME:
            stageText = "Break (Halftime)";
            break;
        case Referee_Stage_NORMAL_SECOND_HALF_PRE:
        case Referee_Stage_NORMAL_SECOND_HALF:
            stageText = "2nd Half";
            break;
        case Referee_Stage_EXTRA_TIME_BREAK:
            stageText = "Break (Ext.)";
            break;
        case Referee_Stage_EXTRA_FIRST_HALF_PRE:
        case Referee_Stage_EXTRA_FIRST_HALF:
            stageText = "1st Half (Ext.)";
            break;
        case Referee_Stage_EXTRA_HALF_TIME:
            stageText = "Break (Ext. Halftime)";
            break;
        case Referee_Stage_EXTRA_SECOND_HALF_PRE:
        case Referee_Stage_EXTRA_SECOND_HALF:
            stageText = "2nd Half (Ext.)";
            break;
        case Referee_Stage_PENALTY_SHOOTOUT_BREAK:
            stageText = "Break (Shootout)";
            break;
        case Referee_Stage_PENALTY_SHOOTOUT:
            stageText = "Penalty Shootout";
            break;
        case Referee_Stage_POST_GAME:
            stageText = "Finished";
            break;
    }

    return stageText;
}

void ScoreBoard::refereeCommandToTextAndColor(Referee::Command command, std::string& commandText, BLRgba32& bgColor)
{
    bgColor = BLRgba32(0xFF444444);

    BLRgba32 yellow(0xFF9A7D0A);
    BLRgba32 blue(0xFF154360);

    switch(command)
    {
        case Referee_Command_HALT:
            commandText = "Halt";
            bgColor = BLRgba32(0xFFB71C1C);
            break;
        case Referee_Command_STOP:
            commandText = "Stop";
            bgColor = BLRgba32(0xFFE65100);
            break;
        case Referee_Command_NORMAL_START:
        case Referee_Command_FORCE_START:
        case Referee_Command_DIRECT_FREE_YELLOW:
        case Referee_Command_DIRECT_FREE_BLUE:
        case Referee_Command_INDIRECT_FREE_YELLOW:
        case Referee_Command_INDIRECT_FREE_BLUE:
            commandText = "Running";
            bgColor = BLRgba32(0xFF1B5E20);
            break;
        case Referee_Command_PREPARE_KICKOFF_YELLOW:
            bgColor = yellow;
            commandText = "Kickoff";
            break;
        case Referee_Command_PREPARE_KICKOFF_BLUE:
            bgColor = blue;
            commandText = "Kickoff";
            break;
        case Referee_Command_PREPARE_PENALTY_YELLOW:
            bgColor = yellow;
            commandText = "Penalty";
            break;
        case Referee_Command_PREPARE_PENALTY_BLUE:
            bgColor = blue;
            commandText = "Penalty";
            break;
        case Referee_Command_TIMEOUT_YELLOW:
            bgColor = yellow;
            commandText = "Timeout";
            break;
        case Referee_Command_TIMEOUT_BLUE:
            bgColor = blue;
            commandText = "Timeout";
            break;
        case Referee_Command_GOAL_YELLOW:
            bgColor = yellow;
            commandText = "Goal";
            break;
        case Referee_Command_GOAL_BLUE:
            bgColor = blue;
            commandText = "Goal";
            break;
        case Referee_Command_BALL_PLACEMENT_YELLOW:
            bgColor = yellow;
            commandText = "Ball Placement";
            break;
        case Referee_Command_BALL_PLACEMENT_BLUE:
            bgColor = blue;
            commandText = "Ball Placement";
            break;
    }
}
