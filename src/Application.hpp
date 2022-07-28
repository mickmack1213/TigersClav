#pragma once

#include <GL/gl3w.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"

class Application
{
public:
    Application();
    virtual ~Application() {}

    int run();

protected:
    virtual void render() {}

    ImVec4 backgroundClearColor_;

private:
    bool initGui();
    void shutdownGui();

    GLFWwindow* window_;
};
