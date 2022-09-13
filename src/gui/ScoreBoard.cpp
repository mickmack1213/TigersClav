#include "ScoreBoard.hpp"

static double textWidth(const BLFont &font, BLGlyphBuffer& gb) {
    BLTextMetrics tm{};
    font.getTextMetrics(gb, tm);
    return tm.advance.x;
}

ScoreBoard::ScoreBoard()
{
    BLResult blResult = regularFontFace_.createFromFile("fonts/Palanquin-Regular.ttf");
    if(blResult)
        throw std::runtime_error("Regular font not found");

    blResult = boldFontFace_.createFromFile("fonts/NotoSans-Bold.ttf");
    if(blResult)
        throw std::runtime_error("Bold font not found");

    gamestateImage_.create(1200, 110, BL_FORMAT_PRGB32);

    update(std::make_shared<Referee>());
}

void ScoreBoard::update(const std::shared_ptr<Referee>& pRef)
{
    ctx_.begin(gamestateImage_);

    // Clear the image.
    ctx_.setCompOp(BL_COMP_OP_SRC_COPY);
    ctx_.setFillStyle(BLRgba32(0xFF00FF00));
    ctx_.fillAll();

    // Background
    ctx_.setFillStyle(BLRgba32(0xFF111111));
    ctx_.fillTriangle(0, 0, 20, 70, 20, 0);
    ctx_.fillTriangle(1200, 0, 1180, 70, 1180, 0);
    ctx_.fillRect(20, 0, 1160, 70);

    // Team names
    drawTeamNames(pRef->yellow().name(), pRef->blue().name(), BLPoint(300, 35), BLPoint(900, 35), BLSize(400, 70));

    // Score
    centeredText(BLPoint(525, 35), std::to_string(pRef->yellow().score()).c_str(), boldFontFace_, 70, BLRgba32(0xFFDDDDDD));
    centeredText(BLPoint(675, 35), std::to_string(pRef->blue().score()).c_str(), boldFontFace_, 70, BLRgba32(0xFFDDDDDD));

    // Cards
    int yellowCards[2] = { pRef->yellow().yellow_card_times().size(), pRef->blue().yellow_card_times().size() };
    unsigned int redCards[2] = { pRef->yellow().red_cards(), pRef->blue().red_cards() };
    drawCard(CardColor::RED, redCards[0], BLPoint(24, 10));
    drawCard(CardColor::YELLOW, yellowCards[0], BLPoint(64, 10));
    drawCard(CardColor::YELLOW, yellowCards[1], BLPoint(1100, 10));
    drawCard(CardColor::RED, redCards[1], BLPoint(1140, 10));

    // Stage time
    if(hasStageTimeLeft(pRef->stage()))
    {
        int time_us = pRef->stage_time_left();
        if(time_us < 0)
            time_us = 0;

        int time_s = time_us / 1000000;
        int time_min = time_s / 60;
        time_s %= 60;

        char buf[6];
        snprintf(buf, sizeof(buf), "%2d:%02d", time_min, time_s);
        centeredText(BLPoint(600, 35), buf, regularFontFace_, 30.0f, BLRgba32(0xFFDDDDDD));
    }

    // Stage
    std::optional<int> actionTimeLeft;
    if(hasActionTimeLeft(pRef->command()))
        actionTimeLeft = pRef->current_action_time_remaining();

    drawStage(pRef->stage(), pRef->command(), actionTimeLeft);

    // Detach the rendering context from `img`.
    ctx_.end();
}

BLImageData ScoreBoard::getImageData()
{
    BLImageData imgData{};
    gamestateImage_.getData(&imgData);

    return imgData;
}

void ScoreBoard::drawStage(Referee::Stage stage, Referee::Command command, std::optional<int> actionTimeLeft_us)
{
    if(command == Referee_Command_FORCE_START || command == Referee_Command_DIRECT_FREE_BLUE || command == Referee_Command_DIRECT_FREE_YELLOW || command == Referee_Command_INDIRECT_FREE_BLUE || command == Referee_Command_INDIRECT_FREE_YELLOW){
        command = Referee_Command_NORMAL_START;
    }

    std::string stageText;
    Referee::Command standard;
    refereeStageToString(stage, standard, stageText);

    std::string commandText;
    BLRgba32 bgColor{}, textColor{};
    refereeCommandToTextAndColor(command, commandText, bgColor, textColor);

    if(command == standard) {
        ctx_.setFillStyle(BLRgba32(0xFF111111));
    } else {
        ctx_.setFillStyle(bgColor);
    }

    ctx_.fillRect(400, 70, 400, 40);
    ctx_.fillTriangle(400, 110, 400, 70, 390, 70);
    ctx_.fillTriangle(800, 110, 800, 70, 810, 70);

    if(command == standard) {
        centeredText(BLPoint(600, 90), stageText.c_str(), regularFontFace_, 30.0f, BLRgba32(0xFFDDDDDD));
    } else {
        if(actionTimeLeft_us && *actionTimeLeft_us > 0)
        {
            int time_us = *actionTimeLeft_us;
            if(time_us < 0)
                time_us = 0;

            int time_s = time_us / 1000000;

            commandText += " (" + std::to_string(time_s) + "s)";
        }

        centeredText(BLPoint(600, 90), commandText.c_str(), regularFontFace_, 30.0f, textColor);
    }
}

void ScoreBoard::drawTeamNames(const std::string& team1, const std::string& team2, BLPoint pos1, BLPoint pos2, BLSize maxSize)
{
    BLFont regularFont;
    BLGlyphBuffer gb[2];

    double nameWidths[2] = {maxSize.w + 1, maxSize.w + 1};
    double fontHeight = maxSize.h + 1;
    float fontSize = 70.0f;
    float fontOffset = 0;

    gb[0].setUtf8Text(team1.c_str());
    gb[1].setUtf8Text(team2.c_str());

    while(nameWidths[0] > maxSize.w || nameWidths[1] > maxSize.w || fontHeight > maxSize.h)
    {
        regularFont.createFromFace(regularFontFace_, fontSize);
        BLFontMetrics fm = regularFont.metrics();
        nameWidths[0] = textWidth(regularFont, gb[0]);
        nameWidths[1] = textWidth(regularFont, gb[1]);
        fontHeight = fm.ascent + fm.descent - fm.lineGap;
        fontOffset = fm.descent;
        fontSize -= 1.0f;
    }

    ctx_.setFillStyle(BLRgba32(0xFFFFFF00));
    ctx_.fillGlyphRun(BLPoint(pos1.x-nameWidths[0]/2, pos1.y + fontHeight/2 - fontOffset), regularFont, gb[0].glyphRun());

    ctx_.setFillStyle(BLRgba32(0xFF7777FF));
    ctx_.fillGlyphRun(BLPoint(pos2.x-nameWidths[1]/2, pos2.y + fontHeight/2 - fontOffset), regularFont, gb[1].glyphRun());
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
        centeredText(BLPoint(pos.x+size.w/2, pos.y+size.h/2), std::to_string(amount).c_str(), boldFontFace_, (float)size.h, BLRgba32(0xFF111111));
    }
}

void ScoreBoard::refereeCommandToTextAndColor(Referee::Command command, std::string &commandText, BLRgba32 &bgColor, BLRgba32 &textColor) {
    bgColor = BLRgba32(0xFF444444);
    textColor = BLRgba32(0xFFDDDDDD);

    BLRgba32 yellow(0xFFFFFF00);
    BLRgba32 blue(0xFF0000FF);
    BLRgba32 darkText(0xFF111111);

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
            bgColor = BLRgba32(0xFF111111);
            break;
        case Referee_Command_PREPARE_KICKOFF_YELLOW:
            bgColor = yellow;
            commandText = "Kickoff";
            textColor = darkText;
            break;
        case Referee_Command_PREPARE_KICKOFF_BLUE:
            bgColor = blue;
            commandText = "Kickoff";
            break;
        case Referee_Command_PREPARE_PENALTY_YELLOW:
            bgColor = yellow;
            commandText = "Penalty";
            textColor = darkText;
            break;
        case Referee_Command_PREPARE_PENALTY_BLUE:
            bgColor = blue;
            commandText = "Penalty";
            break;
        case Referee_Command_TIMEOUT_YELLOW:
            bgColor = yellow;
            commandText = "Timeout";
            textColor = darkText;
            break;
        case Referee_Command_TIMEOUT_BLUE:
            bgColor = blue;
            commandText = "Timeout";
            break;
        case Referee_Command_GOAL_YELLOW:
            bgColor = yellow;
            commandText = "Goal";
            textColor = darkText;
            break;
        case Referee_Command_GOAL_BLUE:
            bgColor = blue;
            commandText = "Goal";
            break;
        case Referee_Command_BALL_PLACEMENT_YELLOW:
            bgColor = yellow;
            commandText = "Ball Placement";
            textColor = darkText;
            break;
        case Referee_Command_BALL_PLACEMENT_BLUE:
            bgColor = blue;
            commandText = "Ball Placement";
            break;
    }
}

void ScoreBoard::refereeStageToString(Referee::Stage stage, Referee::Command &standard, std::string &stageText) {
    switch(stage)
    {
        case Referee_Stage_NORMAL_FIRST_HALF_PRE:
        case Referee_Stage_NORMAL_FIRST_HALF:
            stageText = "1st Half";
            standard = Referee_Command_NORMAL_START;
            break;
        case Referee_Stage_NORMAL_HALF_TIME:
            stageText = "Break (Halftime)";
            standard = Referee_Command_HALT;
            break;
        case Referee_Stage_NORMAL_SECOND_HALF_PRE:
        case Referee_Stage_NORMAL_SECOND_HALF:
            stageText = "2nd Half";
            standard = Referee_Command_NORMAL_START;
            break;
        case Referee_Stage_EXTRA_TIME_BREAK:
            stageText = "Break (Ext.)";
            standard = Referee_Command_HALT;
            break;
        case Referee_Stage_EXTRA_FIRST_HALF_PRE:
        case Referee_Stage_EXTRA_FIRST_HALF:
            stageText = "1st Half (Ext.)";
            standard = Referee_Command_NORMAL_START;
            break;
        case Referee_Stage_EXTRA_HALF_TIME:
            stageText = "Break (Ext. Halftime)";
            standard = Referee_Command_HALT;
            break;
        case Referee_Stage_EXTRA_SECOND_HALF_PRE:
        case Referee_Stage_EXTRA_SECOND_HALF:
            stageText = "2nd Half (Ext.)";
            standard = Referee_Command_NORMAL_START;
            break;
        case Referee_Stage_PENALTY_SHOOTOUT_BREAK:
            stageText = "Break (Shootout)";
            standard = Referee_Command_HALT;
            break;
        case Referee_Stage_PENALTY_SHOOTOUT:
            stageText = "Penalty Shootout";
            standard = Referee_Command_NORMAL_START;
            break;
        case Referee_Stage_POST_GAME:
            stageText = "Finished";
            standard = Referee_Command_HALT;
            break;
    }
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

void ScoreBoard::centeredText(const BLPoint &pos, const char *str, const BLFontFace &face, const float size, const BLRgba32 &color) {
    BLFont regularFont;
    regularFont.createFromFace(face, size);
    BLFontMetrics fm = regularFont.metrics();

    BLGlyphBuffer gb;
    gb.setUtf8Text(str);

    double width = textWidth(regularFont, gb);
    double height = fm.ascent + fm.descent;

    ctx_.setFillStyle(color);
    ctx_.fillGlyphRun(BLPoint(pos.x-width/2, pos.y + height/2 - fm.descent), regularFont, gb.glyphRun());
}
