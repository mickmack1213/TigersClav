#pragma once

#include "blend2d.h"
#include "ssl_vision_wrapper.pb.h"
#include "ssl_vision_wrapper_tracked.pb.h"
#include <memory>
#include <optional>

class FieldVisualizer
{
public:
    FieldVisualizer();

    bool hasGeometry() const { return (bool)pGeometryPacket_; }
    void setGeometry(std::shared_ptr<const SSL_GeometryData> pVision);

    void update(std::shared_ptr<TrackerWrapperPacket> pTracker);

    BLImageData getImageData();

private:
    std::shared_ptr<const SSL_GeometryData> pGeometryPacket_;

    BLFontFace regularFontFace_;
    BLFontFace boldFontFace_;

    BLImage image_;
    BLContext ctx_;
    double scale_;
};
