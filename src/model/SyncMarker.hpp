#pragma once

#include <string>
#include <cstdint>

struct SyncMarker
{
    std::string name;
    int64_t timestamp_ns;
};
