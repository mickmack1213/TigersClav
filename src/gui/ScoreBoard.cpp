#include "ScoreBoard.hpp"

#define MAIN_HEIGHT 60
#define HALF_MAIN_HEIGHT (MAIN_HEIGHT >> 1)
#define STAGE_HEIGHT 40
#define HALF_STAGE_HEIGHT (STAGE_HEIGHT >> 1)

#define FULL_HEIGHT (MAIN_HEIGHT + STAGE_HEIGHT)
#define MAIN_CENTER (STAGE_HEIGHT + HALF_MAIN_HEIGHT)
#define SMALL_TEXT (STAGE_HEIGHT - 10)

#define CURVE_WIDTH 20
#define HALF_CURVE_WIDTH (CURVE_WIDTH / 2.0f)
#define TEAM_NAME_WIDTH 380
#define SCORE_WIDTH 200
#define FULL_WIDTH ((CURVE_WIDTH + TEAM_NAME_WIDTH) * 2 + SCORE_WIDTH)
#define HALF_WIDTH (FULL_WIDTH / 2.0f)
#define TEAM_NAME_CENTER (CURVE_WIDTH + TEAM_NAME_WIDTH/2.0f)

#define STAGE_WIDTH 300
#define HALF_STAGE_WIDTH (STAGE_WIDTH / 2)
#define CARD_OFFSET (HALF_STAGE_WIDTH + CURVE_WIDTH)
#define CARD_WIDTH 40

#define COLOR_DARK BLRgba32(0xFF333333)
#define COLOR_LIGHT BLRgba32(0xFFDDDDDD)


static double textWidth(const BLFont &font, BLGlyphBuffer& gb)
{
    BLTextMetrics tm{};
    font.getTextMetrics(gb, tm);
    return tm.advance.x;
}

ScoreBoard::ScoreBoard()
{
    BLResult blResult = regularFontFace_.createFromFile("fonts/Palanquin-Bold.ttf");
    if(blResult)
        throw std::runtime_error("Regular font not found");

    blResult = boldFontFace_.createFromFile("fonts/NotoSans-Bold.ttf");
    if(blResult)
        throw std::runtime_error("Bold font not found");

    gamestateImage_.create(FULL_WIDTH, FULL_HEIGHT, BL_FORMAT_PRGB32);

    update(std::make_shared<Referee>());
}

void ScoreBoard::update(const std::shared_ptr<Referee>& pRef)
{
    ctx_.begin(gamestateImage_);

    // Clear the image.
    ctx_.setCompOp(BL_COMP_OP_SRC_COPY);
    ctx_.setFillStyle(BLRgba32(0xFF00FF00));
    ctx_.fillAll();

    // Get information
    Referee::Command command = pRef->command();
    if(command == Referee_Command_FORCE_START || command == Referee_Command_DIRECT_FREE_BLUE || command == Referee_Command_DIRECT_FREE_YELLOW || command == Referee_Command_INDIRECT_FREE_BLUE || command == Referee_Command_INDIRECT_FREE_YELLOW)
    {
        command = Referee_Command_NORMAL_START;
    }
    std::string stageText;
    Referee::Command standard;
    refereeStageToString(pRef->stage(), standard, stageText);

    std::string commandText;
    BLRgba32 bgColor{}, textColor{};
    refereeCommandToTextAndColor(command, commandText, bgColor, textColor);

    // Cards
    int yellowCards[2] = { pRef->yellow().yellow_card_times().size(), pRef->blue().yellow_card_times().size() };
    unsigned int redCards[2] = { pRef->yellow().red_cards(), pRef->blue().red_cards() };
    drawCard(CardColor::RED, redCards[0], BLPoint(HALF_WIDTH - CARD_OFFSET - (yellowCards[0] > 0 ? 2 : 1) * CARD_WIDTH, HALF_STAGE_HEIGHT), 1);
    drawCard(CardColor::RED, redCards[1], BLPoint(HALF_WIDTH + CARD_OFFSET + (yellowCards[1] > 0 ? 2 : 1) * CARD_WIDTH, HALF_STAGE_HEIGHT), -1);
    drawCard(CardColor::YELLOW, yellowCards[0], BLPoint(HALF_WIDTH - CARD_OFFSET - CARD_WIDTH, HALF_STAGE_HEIGHT), 1);
    drawCard(CardColor::YELLOW, yellowCards[1], BLPoint(HALF_WIDTH + CARD_OFFSET + CARD_WIDTH, HALF_STAGE_HEIGHT), -1);

    // Background
    BLGradient gradient(BLLinearGradientValues(HALF_WIDTH, STAGE_HEIGHT, HALF_WIDTH, FULL_HEIGHT));
    gradient.addStop(0.0, COLOR_DARK);
    gradient.addStop(1.0, BLRgba32(0xFF111111));

    ctx_.setFillStyle(gradient);
    BLPath background;
    background.moveTo(0, FULL_HEIGHT);
    background.quadTo(HALF_CURVE_WIDTH, STAGE_HEIGHT, CURVE_WIDTH, STAGE_HEIGHT);
    background.lineTo(FULL_WIDTH - CURVE_WIDTH, STAGE_HEIGHT);
    background.quadTo(FULL_WIDTH - HALF_CURVE_WIDTH, STAGE_HEIGHT, FULL_WIDTH, FULL_HEIGHT);
    ctx_.fillPath(background);

    ctx_.setFillStyle(command == standard ? COLOR_DARK : bgColor);
    BLPath stateBackground;
    stateBackground.moveTo(HALF_WIDTH - CARD_OFFSET, STAGE_HEIGHT);
    stateBackground.cubicTo(HALF_WIDTH - HALF_STAGE_WIDTH - HALF_CURVE_WIDTH, STAGE_HEIGHT, HALF_WIDTH - HALF_STAGE_WIDTH - HALF_CURVE_WIDTH, 0, HALF_WIDTH - HALF_STAGE_WIDTH, 0);
    stateBackground.lineTo(HALF_WIDTH + HALF_STAGE_WIDTH, 0);
    stateBackground.cubicTo(HALF_WIDTH + HALF_STAGE_WIDTH + HALF_CURVE_WIDTH, 0, HALF_WIDTH + HALF_STAGE_WIDTH + HALF_CURVE_WIDTH, STAGE_HEIGHT, HALF_WIDTH + CARD_OFFSET, STAGE_HEIGHT);
    ctx_.fillPath(stateBackground);
    ctx_.setStrokeWidth(1.0);
    ctx_.setStrokeStyle(COLOR_DARK);
    ctx_.strokePath(stateBackground);

    // Team names
    drawTeamNames(pRef->yellow().name(), pRef->blue().name(), BLPoint(TEAM_NAME_CENTER, MAIN_CENTER), BLPoint(FULL_WIDTH - TEAM_NAME_CENTER, MAIN_CENTER), BLSize(TEAM_NAME_WIDTH, MAIN_HEIGHT));

    // Score
    centeredText(BLPoint(HALF_WIDTH - 75, MAIN_CENTER), std::to_string(pRef->yellow().score()).c_str(), boldFontFace_, MAIN_HEIGHT, COLOR_LIGHT);
    centeredText(BLPoint(HALF_WIDTH + 75, MAIN_CENTER), std::to_string(pRef->blue().score()).c_str(), boldFontFace_, MAIN_HEIGHT, COLOR_LIGHT);

    // Stage time
    drawTime(pRef->stage(), COLOR_LIGHT, pRef->stage_time_left());

    // Stage
    drawStage(stageText, commandText, standard, command, textColor, pRef->current_action_time_remaining());

    // Detach the rendering context from `img`.
    ctx_.end();
}

BLImageData ScoreBoard::getImageData()
{
    BLImageData imgData{};
    gamestateImage_.getData(&imgData);

    return imgData;
}

void ScoreBoard::drawStage(const std::string& stageText, std::string& commandText, Referee::Command standard, Referee::Command command, const BLRgba32& textColor, int time_us)
{
    if(command == standard)
    {
        centeredText(BLPoint(HALF_WIDTH, HALF_STAGE_HEIGHT), stageText.c_str(), regularFontFace_, SMALL_TEXT, COLOR_LIGHT);
    }
    else
    {
        if(hasActionTimeLeft(command) && time_us > 0)
            commandText += " (" + std::to_string(time_us / 1000000) + "s)";

        centeredText(BLPoint(HALF_WIDTH, HALF_STAGE_HEIGHT), commandText.c_str(), regularFontFace_, SMALL_TEXT, textColor);
    }
}

void ScoreBoard::drawTeamNames(const std::string& team1, const std::string& team2, BLPoint pos1, BLPoint pos2, BLSize maxSize)
{
    BLFont regularFont;
    BLGlyphBuffer gb[2];

    double nameWidths[2] = {maxSize.w + 1, maxSize.w + 1};
    double fontHeight = maxSize.h + 1;
    float fontSize = MAIN_HEIGHT;
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

void ScoreBoard::drawCard(CardColor color, unsigned int amount, BLPoint pos, int direction)
{
    if(amount == 0)
        return;

    ctx_.setFillStyle(BLRgba32(color == CardColor::YELLOW ? 0xFFFFFF00 : 0xFFFF0000));
    BLPath path;
    path.moveTo(pos.x, STAGE_HEIGHT);
    path.cubicTo(pos.x + direction*10, STAGE_HEIGHT, pos.x + direction*10, 0, pos.x + direction*20, 0);
    path.lineTo(pos.x + direction*60, 0);
    path.lineTo(pos.x + direction*60, STAGE_HEIGHT);
    ctx_.fillPath(path);
    ctx_.setStrokeStyle(COLOR_DARK);
    ctx_.setStrokeWidth(1.0);
    ctx_.strokePath(path);

    if(amount > 1)
    {
        centeredText(BLPoint(pos.x + direction*30, pos.y), std::to_string(amount).c_str(), boldFontFace_, SMALL_TEXT, COLOR_DARK);
    }
}

void ScoreBoard::refereeCommandToTextAndColor(Referee::Command command, std::string &commandText, BLRgba32 &bgColor, BLRgba32 &textColor) {
    bgColor = COLOR_DARK;
    textColor = COLOR_LIGHT;

    BLRgba32 yellow(0xFFFFFF00);
    BLRgba32 blue(0xFF0000FF);

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
            break;
        case Referee_Command_PREPARE_KICKOFF_YELLOW:
            bgColor = yellow;
            commandText = "Kickoff";
            textColor = COLOR_DARK;
            break;
        case Referee_Command_PREPARE_KICKOFF_BLUE:
            bgColor = blue;
            commandText = "Kickoff";
            break;
        case Referee_Command_PREPARE_PENALTY_YELLOW:
            bgColor = yellow;
            commandText = "Penalty";
            textColor = COLOR_DARK;
            break;
        case Referee_Command_PREPARE_PENALTY_BLUE:
            bgColor = blue;
            commandText = "Penalty";
            break;
        case Referee_Command_TIMEOUT_YELLOW:
            bgColor = yellow;
            commandText = "Timeout";
            textColor = COLOR_DARK;
            break;
        case Referee_Command_TIMEOUT_BLUE:
            bgColor = blue;
            commandText = "Timeout";
            break;
        case Referee_Command_GOAL_YELLOW:
            bgColor = yellow;
            commandText = "Goal";
            textColor = COLOR_DARK;
            break;
        case Referee_Command_GOAL_BLUE:
            bgColor = blue;
            commandText = "Goal";
            break;
        case Referee_Command_BALL_PLACEMENT_YELLOW:
            bgColor = yellow;
            commandText = "Ball Placement";
            textColor = COLOR_DARK;
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

void ScoreBoard::drawTime(Referee::Stage stage, const BLRgba32& textColor, int time_us) {
    if(hasStageTimeLeft(stage))
    {
        if(time_us < 0)
            time_us = 0;

        int time_s = time_us / 1000000;
        int time_min = time_s / 60;
        time_s %= 60;

        char buf[6];
        snprintf(buf, sizeof(buf), "%2d:%02d", time_min, time_s);
        centeredText(BLPoint(HALF_WIDTH, MAIN_CENTER), buf, regularFontFace_, SMALL_TEXT, textColor);
    }
}
