#pragma once

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

class Application
{
public:
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
