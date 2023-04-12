#pragma once

#include "AScoreBoard.hpp"
#include <vector>
#include <string>
#include <memory>

class ScoreBoardFactory
{
public:
    static std::vector<std::string> getTypeList();
    static std::unique_ptr<AScoreBoard> create(const std::string& type = "");

    static constexpr std::string_view TYPE_PROGRAMMER { "Programmer" };
    static constexpr std::string_view TYPE_FANCY { "Fancy" };
};
