#pragma once

#include "Application.hpp"
#include "blend2d.h"

class TigersClav : public Application
{
public:
    TigersClav();

    void render() override;

private:
    void createGamestateOverlay();
};
