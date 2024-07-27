#pragma once

#include "util/easylogging++.h"
#include "imgui.h"
#include <vector>
#include <string>
#include <mutex>

class LogViewer : public el::LogDispatchCallback
{
public:
    LogViewer();

    void render();

protected:
    void handle(const el::LogDispatchData* data) noexcept override;

private:
    struct LogEntry
    {
        el::Level level;
        std::string text;
    };

    std::vector<LogEntry> entries_;
    std::mutex entriesMutex_;

    bool autoScroll_;
    bool showDemoWindow_;
};
