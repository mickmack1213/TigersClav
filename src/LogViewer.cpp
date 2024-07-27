#include "LogViewer.hpp"

LogViewer::LogViewer()
:autoScroll_(true),
 showDemoWindow_(false)
{
}

void LogViewer::handle(const el::LogDispatchData* data) noexcept
{
    LogEntry entry;
    entry.level = data->logMessage()->level();
    entry.text = data->logMessage()->logger()->logBuilder()->build(data->logMessage(),
                    data->dispatchAction() == el::base::DispatchAction::NormalLog);

    std::unique_lock<std::mutex> lock(entriesMutex_);
    entries_.emplace_back(std::move(entry));
}

void LogViewer::render()
{
    ImGui::Begin("Log");

    ImGui::Checkbox("Auto-scroll", &autoScroll_);

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -25), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

    {
        std::unique_lock<std::mutex> lock(entriesMutex_);
        for(const auto& entry : entries_)
        {
            ImGui::Text("%s", entry.text.c_str());
        }
    }

    if(autoScroll_)
        ImGui::SetScrollHereY(1.0f);

    ImGui::PopStyleVar();

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::SameLine();
    ImGui::Checkbox("Demo Window", &showDemoWindow_);

    ImGui::End();

    // Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (showDemoWindow_)
        ImGui::ShowDemoWindow(&showDemoWindow_);
}
