#pragma once

#include "data/SSLGameLog.hpp"

struct RefereeStateChange
{
    int64_t timestamp_ns_;
    std::shared_ptr<Referee> pBefore_;
    std::shared_ptr<Referee> pAfter_;
};
