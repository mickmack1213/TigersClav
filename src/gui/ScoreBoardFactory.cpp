#include "ScoreBoardFactory.hpp"
#include "ProgrammerScoreBoard.hpp"
#include "FancyScoreBoard.hpp"

std::vector<std::string> ScoreBoardFactory::getTypeList()
{
    return std::vector<std::string>{ std::string(TYPE_PROGRAMMER), std::string(TYPE_FANCY) };
}

std::unique_ptr<AScoreBoard> ScoreBoardFactory::create(const std::string& type)
{
    if(type == TYPE_PROGRAMMER || type.empty())
        return std::make_unique<ProgrammerScoreBoard>();

    if(type == TYPE_FANCY)
        return std::make_unique<FancyScoreBoard>();

    return nullptr;
}
