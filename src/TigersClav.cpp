#include "TigersClav.hpp"
#include "ImGuiFileDialog.h"
#include "LogViewer.hpp"

TigersClav::TigersClav()
:lastFileOpenPath_("."),
 gameLogTime_s_(0.0f),
 gameLogAutoPlay_(false),
 gameLogSliderHovered_(false),
 cameraTime_s_(0.0f),
 cameraAutoPlay_(false),
 cameraSliderHovered_(false),
 cameraIndex_(-1)
{
    glGenTextures(1, &scoreBoardTexture_);
    glGenTextures(1, &fieldVisualizerTexture_);

    pScoreBoard_ = std::make_unique<ScoreBoard>();
    pFieldVisualizer_ = std::make_unique<FieldVisualizer>();
    pImageComposer_ = std::make_unique<ImageComposer>(ImVec2(3840, 2160));

    pProject_ = std::make_unique<Project>();

    // TODO: move this to project
    std::ifstream openPath("lastFileOpenPath.txt");
    if(openPath)
    {
        lastFileOpenPath_.resize(256);
        openPath.getline(lastFileOpenPath_.data(), lastFileOpenPath_.size());
        openPath.close();
    }
}

void TigersClav::render()
{
    // Setup docking
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    const ImGuiViewport* viewport;

    if (viewport == NULL)
        viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags host_window_flags = 0;
    host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
    host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        host_window_flags |= ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", NULL, host_window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("DockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspace_flags, NULL);

    // Draw main menu
    if(ImGui::BeginMenuBar())
    {
        if(ImGui::BeginMenu("Project"))
        {
            // TODO: implement project save/load
            if(ImGui::MenuItem("Open...", "CTRL+O"))
            {
            }

            if(ImGui::MenuItem("Save"), "CTRL+S")
            {
            }

            if(ImGui::MenuItem("Save As..."))
            {
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::End();

    // Log Panel
    el::Helpers::logDispatchCallback<LogViewer>("LogViewer")->render();

    // Gamelog Panel
    ImGui::SetNextWindowDockID(dockspaceId, ImGuiCond_FirstUseEver);
    drawGameLogPanel();

    // Video Panel
    ImGui::SetNextWindowDockID(dockspaceId, ImGuiCond_FirstUseEver);
    drawVideoPanel();

    // Project Panel
    ImGui::SetNextWindowDockID(dockspaceId, ImGuiCond_FirstUseEver);
    drawProjectPanel();
}

void TigersClav::drawProjectPanel()
{
    ImGui::Begin("Project");

    if(ImGui::TreeNodeEx("Gamelog", ImGuiTreeNodeFlags_FramePadding))
    {
        if(pProject_->getGameLog())
        {
            for(const auto& detail : pProject_->getGameLog()->getFileDetails())
            {
                ImGui::BulletText(detail.c_str());
            }
        }

        if(ImGui::Button("Load Gamelog", ImVec2(100.0f, 0.0f)))
        {
            ImGuiFileDialog::Instance()->OpenDialog("LoadGamelogDialog", "Choose File", "Gamelogs {.log,.gz}", lastFileOpenPath_, 1, 0, ImGuiFileDialogFlags_Modal);
        }

        if(ImGuiFileDialog::Instance()->Display("LoadGamelogDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 500)))
        {
            if(ImGuiFileDialog::Instance()->IsOk())
            {
                lastFileOpenPath_ = ImGuiFileDialog::Instance()->GetCurrentPath() + "/";

                std::ofstream openPath("lastFileOpenPath.txt", std::ofstream::trunc);
                openPath << lastFileOpenPath_;
                openPath.close();

                pProject_->openGameLog(ImGuiFileDialog::Instance()->GetFilePathName());

                gameLogTime_s_ = 0.0f;
                gameLogAutoPlay_ = false;
            }

            ImGuiFileDialog::Instance()->Close();
        }

        ImGui::TreePop();
    }

    if(ImGui::TreeNodeEx("Cameras", ImGuiTreeNodeFlags_FramePadding))
    {
        std::vector<std::shared_ptr<Camera>>::iterator removeIter = pProject_->getCameras().end();

        for(const auto& pCam : pProject_->getCameras())
        {
            if(ImGui::TreeNodeEx(pCam->getName().c_str(), ImGuiTreeNodeFlags_FramePadding))
            {
                if(pCam->getTotalDuration_ns() > 0)
                {
                    ImGui::BulletText("Duration: %.3fs", pCam->getTotalDuration_ns()*1e-9);
                }

                // show videos
                std::list<std::shared_ptr<VideoRecording>>::iterator removeVideoIter = pCam->getVideos().end();

                for(const auto& pVideo : pCam->getVideos())
                {
                    if(ImGui::TreeNode(pVideo->getName().c_str()))
                    {
                        for(const auto& detail : pVideo->pVideo_->getFileDetails())
                            ImGui::BulletText(detail.c_str());

                        if(ImGui::Button("Delete Video", ImVec2(200.0f, 0.0f)))
                        {
                            removeVideoIter = std::find(pCam->getVideos().begin(), pCam->getVideos().end(), pVideo);
                        }

                        ImGui::TreePop();
                    }
                }

                if(removeVideoIter != pCam->getVideos().end())
                    pCam->getVideos().erase(removeVideoIter);

                // Add video logic
                if(ImGui::Button("Add Video", ImVec2(200.0f, 0.0f)))
                {
                    ImGuiFileDialog::Instance()->OpenDialog("AddVideoDialog", "Choose File", ".*", lastFileOpenPath_, 1, (void*)pCam.get(), ImGuiFileDialogFlags_Modal);
                }

                if(ImGuiFileDialog::Instance()->Display("AddVideoDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 500)))
                {
                    if(ImGuiFileDialog::Instance()->IsOk())
                    {
                        lastFileOpenPath_ = ImGuiFileDialog::Instance()->GetCurrentPath() + "/";

                        std::ofstream openPath("lastFileOpenPath.txt", std::ofstream::trunc);
                        openPath << lastFileOpenPath_;
                        openPath.close();

                        Camera* pCamAdd = reinterpret_cast<Camera*>(ImGuiFileDialog::Instance()->GetUserDatas());

                        pCamAdd->addVideo(ImGuiFileDialog::Instance()->GetFilePathName());
                    }

                    ImGuiFileDialog::Instance()->Close();
                }

                // Delete camera logic
                if(ImGui::Button("Delete Camera", ImVec2(200.0f, 0.0f)))
                    ImGui::OpenPopup("Confirm Camera Deletion");

                ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

                if(ImGui::BeginPopupModal("Confirm Camera Deletion", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text("This will delete the camera and all associated videos.");
                    ImGui::Text("Are you sure?");

                    if(ImGui::Button("Yes", ImVec2(120, 0)))
                    {
                        ImGui::CloseCurrentPopup();

                        removeIter = std::find(pProject_->getCameras().begin(), pProject_->getCameras().end(), pCam);
                    }

                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine();
                    if(ImGui::Button("Cancel", ImVec2(120, 0)))
                    {
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }

                ImGui::TreePop();
            }
        }

        if(removeIter != pProject_->getCameras().end())
        {
            pProject_->getCameras().erase(removeIter);
        }

        // Add camera logic
        if(ImGui::Button("Add Camera", ImVec2(100.0f, 0.0f)))
            ImGui::OpenPopup("Camera Name");

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if(ImGui::BeginPopupModal("Camera Name", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            static char camNameBuf[128];

            ImGui::InputText("##Camera_name", camNameBuf, sizeof(camNameBuf));

            auto existsIter = std::find_if(pProject_->getCameras().begin(), pProject_->getCameras().end(), [&](auto pCam){ return pCam->getName() == std::string(camNameBuf); });
            const bool invalidName = existsIter != pProject_->getCameras().end() || std::string(camNameBuf).empty();

            if(invalidName)
            {
                if(existsIter != pProject_->getCameras().end())
                    ImGui::Text("This camera name already exists!");

                ImGui::BeginDisabled();
            }

            if(ImGui::Button("Create", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();

                auto pCamera = std::make_shared<Camera>(std::string(camNameBuf));
                pProject_->getCameras().push_back(pCamera);

                camNameBuf[0] = 0;
            }

            if(invalidName)
            {
                ImGui::EndDisabled();
            }

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if(ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::TreePop();
    }

    ImGui::End();
}

void TigersClav::drawGameLogPanel()
{
    ImGui::Begin("Gamelog");

    ImVec2 regionAvail = ImGui::GetContentRegionAvail();

    const float aspectRatioScoreBoard = (float)pScoreBoard_->getImageData().size.h / pScoreBoard_->getImageData().size.w;
    const float aspectRatioField = (float)pFieldVisualizer_->getImageData().size.h / pFieldVisualizer_->getImageData().size.w;

    ImVec2 scoreBoardSize(regionAvail.x, regionAvail.x*aspectRatioScoreBoard);
    ImVec2 fieldSize(regionAvail.x, regionAvail.x*aspectRatioField);

    if(scoreBoardSize.y + fieldSize.y > regionAvail.y*0.8f)
    {
        float scale = regionAvail.y*0.8f / (scoreBoardSize.y + fieldSize.y);

        scoreBoardSize.x *= scale;
        scoreBoardSize.y *= scale;
        fieldSize.x *= scale;
        fieldSize.y *= scale;
    }

    createGamestateTextures();

    ImGui::Image((void*)(intptr_t)scoreBoardTexture_, scoreBoardSize);
    ImGui::Image((void*)(intptr_t)fieldVisualizerTexture_, fieldSize);

    if(pProject_->getGameLog() && pProject_->getGameLog()->isLoaded())
    {
        std::shared_ptr<GameLog> pGameLog = pProject_->getGameLog();

        pFieldVisualizer_->setGeometry(pGameLog->getGeometry());

        float tMax_s = pGameLog->getTotalDuration_ns() * 1e-9f;

        float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::PushButtonRepeat(true);
        if(ImGui::ArrowButton("##left", ImGuiDir_Left) || (gameLogSliderHovered_ && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)))
        {
            pGameLog->seekToPrevious();
            auto optEntry = pGameLog->get();
            if(optEntry)
            {
                gameLogTime_s_ = optEntry->timestamp_ns_ * 1e-9;
            }

            gameLogAutoPlay_ = false;
        }

        ImGui::SameLine(0.0f, spacing);
        ImGui::SetNextItemWidth(-78.0f);
        ImGui::SliderFloat("##GameLogTime", &gameLogTime_s_, 0.0f, tMax_s, "%.3fs", ImGuiSliderFlags_AlwaysClamp);
        gameLogSliderHovered_ = ImGui::IsItemHovered();
        if(ImGui::IsItemEdited())
        {
            pGameLog->seekTo(gameLogTime_s_ * 1e9);
            gameLogAutoPlay_ = false;
        }

        ImGui::SameLine(0.0f, spacing);
        if(ImGui::ArrowButton("##right", ImGuiDir_Right) || (gameLogSliderHovered_ && ImGui::IsKeyPressed(ImGuiKey_RightArrow)))
        {
            pGameLog->seekToNext();
            auto optEntry = pGameLog->get();
            if(optEntry)
            {
                gameLogTime_s_ = optEntry->timestamp_ns_ * 1e-9;
            }

            gameLogAutoPlay_ = false;
        }

        ImGui::PopButtonRepeat();

        ImGui::SameLine(0.0f, spacing);

        if(gameLogAutoPlay_)
        {
            gameLogTime_s_ += ImGui::GetIO().DeltaTime;
            pGameLog->seekTo(gameLogTime_s_ * 1e9);

            if(ImGui::Button("Pause", ImVec2(50, 0)) || gameLogTime_s_*1e9 > pGameLog->getTotalDuration_ns())
            {
                gameLogAutoPlay_ = false;
            }
        }
        else
        {
            if(ImGui::Button("Play", ImVec2(50, 0)))
            {
                gameLogAutoPlay_ = true;
            }
        }

        // Tracker source selection
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Tracker Source: ");
        ImGui::SameLine();

        for(const auto& source : pGameLog->getTrackerSources())
        {
            if(ImGui::RadioButton(source.second.c_str(), source.first == pGameLog->getPreferredTrackerSourceUUID()))
                pGameLog->setPreferredTrackerSourceUUID(source.first);

            ImGui::SameLine();
        }

        ImGui::NewLine();

        std::optional<GameLog::Entry> entry = pGameLog->get();
        if(entry)
        {
            pFieldVisualizer_->update(entry->pTracker_);
            pScoreBoard_->update(entry->pReferee_);
        }
    }

    ImGui::End();
}

void TigersClav::drawVideoPanel()
{
    ImGui::Begin("Video");

    ImVec2 regionAvail = ImGui::GetContentRegionAvail();

    const float aspectRatioVideo = pImageComposer_->getRenderSize().y / pImageComposer_->getRenderSize().x;

    ImVec2 videoSize(regionAvail.x, regionAvail.x*aspectRatioVideo);

    if(videoSize.y > regionAvail.y*0.8f)
    {
        float scale = regionAvail.y*0.8f / videoSize.y;

        videoSize.x *= scale;
        videoSize.y *= scale;
    }

    ImGui::Image((void*)(intptr_t)pImageComposer_->getTexture(), videoSize);

    pImageComposer_->begin();

    if(!pProject_->getCameras().empty())
    {
        if(cameraIndex_ < 0)
            cameraIndex_ = 0;

        if(ImGui::BeginCombo("Camera", pProject_->getCameras().at(cameraIndex_)->getName().c_str()))
        {
            for(size_t i = 0; i < pProject_->getCameras().size(); i++)
            {
                bool isSelected = i == cameraIndex_;
                if(ImGui::Selectable(pProject_->getCameras().at(i)->getName().c_str(), isSelected))
                    cameraIndex_ = i;

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    if(cameraIndex_ >= pProject_->getCameras().size())
    {
        cameraIndex_ = -1;
    }

    if(cameraIndex_ >= 0)
    {
        std::shared_ptr<Camera> pCamera = pProject_->getCameras().at(cameraIndex_);

        float tMax_s = pCamera->getTotalDuration_ns() * 1e-9f;
        float dt_s = pCamera->getFrameDeltaTime();

        float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::PushButtonRepeat(true);
        if(ImGui::ArrowButton("##left", ImGuiDir_Left) || (cameraSliderHovered_ && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)))
        {
            if(cameraTime_s_ > dt_s)
                cameraTime_s_ -= dt_s;

            cameraAutoPlay_ = false;
        }

        ImGui::SameLine(0.0f, spacing);
        ImGui::SetNextItemWidth(-78.0f);
        ImGui::SliderFloat("##CameraTime", &cameraTime_s_, 0.0f, tMax_s, "%.3fs", ImGuiSliderFlags_AlwaysClamp);
        cameraSliderHovered_ = ImGui::IsItemHovered();
        if(ImGui::IsItemEdited())
            cameraAutoPlay_ = false;

        ImGui::SameLine(0.0f, spacing);
        if(ImGui::ArrowButton("##right", ImGuiDir_Right) || (cameraSliderHovered_ && ImGui::IsKeyPressed(ImGuiKey_RightArrow)))
        {
            if(cameraTime_s_+dt_s < tMax_s)
                cameraTime_s_ += dt_s;

            cameraAutoPlay_ = false;
        }

        ImGui::PopButtonRepeat();

        ImGui::SameLine(0.0f, spacing);

        if(cameraAutoPlay_)
        {
            cameraTime_s_ += ImGui::GetIO().DeltaTime;

            if(ImGui::Button("Pause", ImVec2(50, 0)) || cameraTime_s_ * 1e9 > pCamera->getTotalDuration_ns())
            {
                cameraAutoPlay_ = false;
            }
        }
        else
        {
            if(ImGui::Button("Play", ImVec2(50, 0)))
            {
                cameraAutoPlay_ = true;
            }
        }

        AVFrame* pFrame = pCamera->getAVFrame(cameraTime_s_ * 1e9);
        if(pFrame)
            pImageComposer_->drawVideoFrameRGB(pFrame);
    }

    pImageComposer_->end();

    ImGui::End();
}

void TigersClav::createGamestateTextures()
{
    BLImageData imgData = pScoreBoard_->getImageData();

    glBindTexture(GL_TEXTURE_2D, scoreBoardTexture_);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgData.size.w, imgData.size.h, 0, GL_BGRA, GL_UNSIGNED_BYTE, imgData.pixelData);

    imgData = pFieldVisualizer_->getImageData();

    glBindTexture(GL_TEXTURE_2D, fieldVisualizerTexture_);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgData.size.w, imgData.size.h, 0, GL_BGRA, GL_UNSIGNED_BYTE, imgData.pixelData);
}
