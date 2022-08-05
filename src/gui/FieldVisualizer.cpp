#include "FieldVisualizer.hpp"

FieldVisualizer::FieldVisualizer()
:scale_(10)
{
    BLResult blResult = regularFontFace_.createFromFile("fonts/NotoSansMono-Regular.ttf");
    if(blResult)
        throw std::runtime_error("Regular font not found");

    blResult = boldFontFace_.createFromFile("fonts/RobotoSlab-Bold.ttf");
    if(blResult)
        throw std::runtime_error("Bold font not found");

    image_.create(16, 4, BL_FORMAT_PRGB32);

    ctx_.begin(image_);

    // Clear the image.
    ctx_.setCompOp(BL_COMP_OP_SRC_COPY);
    ctx_.setFillStyle(BLRgba32(0xFF000000));
    ctx_.fillAll();

    ctx_.end();
}

void FieldVisualizer::setGeometry(std::shared_ptr<const SSL_GeometryData> pVision)
{
    pGeometryPacket_ = pVision;

    if(!pVision)
        return;

    const SSL_GeometryFieldSize& fieldSize = pVision->field();
    int32_t totalWidth = (fieldSize.field_length() + fieldSize.boundary_width()*2) / (int32_t)scale_;
    int32_t totalHeight = (fieldSize.field_width() + fieldSize.boundary_width()*2) / (int32_t)scale_;

    // is a resize required?
    if(image_.size().w != totalWidth || image_.size().h != totalHeight)
    {
        image_.create(totalWidth, totalHeight, BL_FORMAT_PRGB32);
    }
}

void FieldVisualizer::update(std::shared_ptr<TrackerWrapperPacket> pTracker)
{
    BLRgba32 underlineYellow(0xFFFFD700);
    BLRgba32 underlineBlue(0xFF0000FF);

    if(!pTracker->has_tracked_frame() || !pGeometryPacket_)
        return;

    // TODO: distinguish tracker sources?

    ctx_.begin(image_);

    // Clear the image.
    ctx_.setCompOp(BL_COMP_OP_SRC_COPY);
    ctx_.setFillStyle(BLRgba32(0xFF145A32));
    ctx_.fillAll();

    const double offX = image_.size().w/2;
    const double offY = image_.size().h/2;

    // Field Lines
    ctx_.setStrokeStyle(BLRgba32(0xFFFFFFFF));
    ctx_.setStrokeWidth(2);

    const SSL_GeometryFieldSize& fieldSize = pGeometryPacket_->field();

    for(const auto& line : fieldSize.field_lines())
    {
        ctx_.strokeLine(line.p1().x()/scale_ + offX, line.p1().y()/scale_ + offY, line.p2().x()/scale_ + offX, line.p2().y()/scale_ + offY);
    }

    // Robots
    for(const auto& robot : pTracker->tracked_frame().robots())
    {
        ctx_.setStrokeStyle(BLRgba32(0xFF000000));
        ctx_.setStrokeWidth(10/scale_);

        ctx_.setFillStyle(BLRgba32(0xFF000000));
        ctx_.fillChord(robot.pos().x()*1000.0/scale_ + offX, robot.pos().y()*1000.0/scale_ + offY, fieldSize.max_robot_radius()/scale_, fieldSize.max_robot_radius()/scale_,
                        robot.orientation()+0.5f, M_PI*2.0-1.0);

        if(robot.robot_id().team() == Team::YELLOW)
            ctx_.setFillStyle(BLRgba32(0xFFFFEB3B));
        else
            ctx_.setFillStyle(BLRgba32(0xFF1565C0));

        ctx_.fillPie(robot.pos().x()*1000.0/scale_ + offX, robot.pos().y()*1000.0/scale_ + offY, fieldSize.max_robot_radius()/scale_, fieldSize.max_robot_radius()/scale_,
                        robot.orientation()+0.5f, M_PI*2.0-1.0);

        ctx_.strokeChord(robot.pos().x()*1000.0/scale_ + offX, robot.pos().y()*1000.0/scale_ + offY, fieldSize.max_robot_radius()/scale_, fieldSize.max_robot_radius()/scale_,
                        robot.orientation()+0.5f, M_PI*2.0-1.0);
    }

    // Ball
    if(pTracker->tracked_frame().balls_size() > 0)
    {
        const TrackedBall& ball = pTracker->tracked_frame().balls(0);

        ctx_.setFillStyle(BLRgba32(0xFFF39C12));
        ctx_.setStrokeStyle(BLRgba32(0xFFF39C12));
        ctx_.setStrokeWidth(15/scale_);

        ctx_.fillCircle(ball.pos().x()*1000.0/scale_ + offX, ball.pos().y()*1000.0/scale_ + offY, fieldSize.ball_radius()/scale_);
        ctx_.strokeCircle(ball.pos().x()*1000.0/scale_ + offX, ball.pos().y()*1000.0/scale_ + offY, fieldSize.ball_radius()*10/scale_);
    }

    // Tracker source name
    BLFont regularFont;
    regularFont.createFromFace(regularFontFace_, fieldSize.boundary_width()/scale_*0.8);

    ctx_.setFillStyle(BLRgba32(0xFFFFFFFF));
    ctx_.fillUtf8Text(BLPoint(0, fieldSize.boundary_width()/scale_*0.8), regularFont, pTracker->source_name().c_str());

    // Detach the rendering context from `img`.
    ctx_.end();
}

BLImageData FieldVisualizer::getImageData()
{
    BLImageData imgData;
    image_.getData(&imgData);

    return imgData;
}
