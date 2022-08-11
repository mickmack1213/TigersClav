#include "TigersClav.hpp"
#include "ImGuiFileDialog.h"
#include "LogViewer.hpp"
#include <filesystem>

TigersClav::TigersClav()
:lastFileOpenPath_("."),
 gameLogTime_s_(0.0f),
 gameLogAutoPlay_(false),
 gameLogSliderHovered_(false),
 recordingTime_s_(0.0f),
 recordingAutoPlay_(false),
 recordingSliderHovered_(false),
 recordingIndex_(-1)
{
    glGenTextures(1, &scoreBoardTexture_);
    glGenTextures(1, &fieldVisualizerTexture_);

    pScoreBoard_ = std::make_unique<ScoreBoard>();
    pFieldVisualizer_ = std::make_unique<FieldVisualizer>();
    pImageComposer_ = std::make_unique<ImageComposer>(ImVec2(3840, 2160));

    pProject_ = std::make_unique<Project>();

    snprintf(camNameBuf_, sizeof(camNameBuf_), "Camera 1");
    markerNameBuf_[0] = 0;

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
            if(ImGui::MenuItem("Open...", "CTRL+O"))
            {
                ImGuiFileDialog::Instance()->OpenDialog("OpenProjectDialog", ICON_IGFD_FOLDER_OPEN "Choose File", "Clav Project {.clav_prj}", lastFileOpenPath_, 1, 0, ImGuiFileDialogFlags_Modal);
            }

            if(ImGui::MenuItem("Save"))
            {
                if(pProject_->getFilename().empty())
                    ImGuiFileDialog::Instance()->OpenDialog("SaveProjectDialog", ICON_IGFD_SAVE "Choose File", "Clav Project {.clav_prj}", lastFileOpenPath_, 1, 0, ImGuiFileDialogFlags_Modal);
                else
                    pProject_->save(pProject_->getFilename());
            }

            if(ImGui::MenuItem("Save As..."))
            {
                ImGuiFileDialog::Instance()->OpenDialog("SaveProjectDialog", ICON_IGFD_SAVE "Choose File", "Clav Project {.clav_prj}", lastFileOpenPath_, 1, 0, ImGuiFileDialogFlags_Modal);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::End();

    if(ImGuiFileDialog::Instance()->Display("OpenProjectDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 500)))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            lastFileOpenPath_ = ImGuiFileDialog::Instance()->GetCurrentPath() + "/";

            std::ofstream openPath("lastFileOpenPath.txt", std::ofstream::trunc);
            openPath << lastFileOpenPath_;
            openPath.close();

            pProject_->load(ImGuiFileDialog::Instance()->GetFilePathName());
        }

        ImGuiFileDialog::Instance()->Close();
    }

    if(ImGuiFileDialog::Instance()->Display("SaveProjectDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 500)))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            lastFileOpenPath_ = ImGuiFileDialog::Instance()->GetCurrentPath() + "/";

            std::ofstream openPath("lastFileOpenPath.txt", std::ofstream::trunc);
            openPath << lastFileOpenPath_;
            openPath.close();

            pProject_->save(ImGuiFileDialog::Instance()->GetFilePathName());
        }

        ImGuiFileDialog::Instance()->Close();
    }

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

    // Sync (Video+GameLog) Panel
    ImGui::SetNextWindowDockID(dockspaceId, ImGuiCond_FirstUseEver);
    drawSyncPanel();
}

void TigersClav::drawProjectPanel()
{
    if(pProject_->getFilename().empty())
        ImGui::Begin("Project");
    else
        ImGui::Begin((std::string("Project - ") + std::filesystem::path(pProject_->getFilename()).stem().string()).c_str());

    if(ImGui::TreeNodeEx("Gamelog", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DefaultOpen))
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
                std::vector<std::shared_ptr<VideoRecording>>::iterator removeVideoIter = pCam->getVideos().end();

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

                        MediaSource x(ImGuiFileDialog::Instance()->GetFilePathName());

//                        Camera* pCamAdd = reinterpret_cast<Camera*>(ImGuiFileDialog::Instance()->GetUserDatas());
//
//                        pCamAdd->addVideo(ImGuiFileDialog::Instance()->GetFilePathName());
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
            ImGui::InputText("##Camera_name", camNameBuf_, sizeof(camNameBuf_));

            auto existsIter = std::find_if(pProject_->getCameras().begin(), pProject_->getCameras().end(), [&](auto pCam){ return pCam->getName() == std::string(camNameBuf_); });
            const bool invalidName = existsIter != pProject_->getCameras().end() || std::string(camNameBuf_).empty();

            if(invalidName)
            {
                if(existsIter != pProject_->getCameras().end())
                    ImGui::Text("This camera name already exists!");

                ImGui::BeginDisabled();
            }

            if(ImGui::Button("Create", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();

                auto pCamera = std::make_shared<Camera>(std::string(camNameBuf_));
                pProject_->getCameras().push_back(pCamera);

                snprintf(camNameBuf_, sizeof(camNameBuf_), "Camera %d", pProject_->getCameras().size()+1);
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

void TigersClav::drawSyncPanel()
{
    ImGui::Begin("Sync");

    const float heightCamera = 40.0f;
    const float heightGameLog = 40.0f;
    const float firstColWidth = 100.0f;

    const ImVec2 regionAvail = ImGui::GetContentRegionAvail();
    const int64_t projectDuration = pProject_->getTotalDuration();
    const double scaleX = (regionAvail.x - firstColWidth) / (double)projectDuration;

    if(ImGui::Button("Refresh"))
    {
        pProject_->sync();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    auto pGameLog = pProject_->getGameLog();
    if(pGameLog)
    {
        // draw gamelog
        ImGui::Text("Gamelog");
        ImGui::SameLine(firstColWidth);
        if(pProject_->getMinTStart() < 0)
        {
            ImGui::InvisibleButton("gamelog_gap", ImVec2(-pProject_->getMinTStart() * scaleX, heightGameLog));
            ImGui::SameLine(0.0f, 0.0f);
        }
        ImVec2 logBtnScreenPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##log", ImVec2(pGameLog->getTotalDuration_ns() * scaleX, heightGameLog));

        ImVec2 logBtnSize = ImGui::GetItemRectSize();

        ImGui::GetWindowDrawList()->AddRect(logBtnScreenPos, ImVec2(logBtnScreenPos.x+logBtnSize.x, logBtnScreenPos.y+logBtnSize.y), 0xFF444444);

        const auto& finalCut = pGameLog->getDirector().getFinalCut();
        for(const auto& cut : finalCut)
        {
            ImU32 col = 0xFF205E1B;

            float xPos = logBtnScreenPos.x + logBtnSize.x * (double)cut.tStart_ns_/(double)pGameLog->getTotalDuration_ns();
            float yPos = logBtnScreenPos.y + 4.0f;
            float xPosEnd = logBtnScreenPos.x + logBtnSize.x * (double)cut.tEnd_ns_/(double)pGameLog->getTotalDuration_ns();
            float yPosEnd = logBtnScreenPos.y + logBtnSize.y - 20.0f;
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(xPos, yPos), ImVec2(xPosEnd, yPosEnd), col);
        }

        const auto& sceneBlocks = pGameLog->getDirector().getSceneBlocks();
        for(const auto& block : sceneBlocks)
        {
            ImU32 col = 0xFF000000;

            switch(block.state_)
            {
                case Director::SceneState::HALT: col = 0xFF101077; break;
                case Director::SceneState::STOP: col = 0xFF0051E6; break;
                case Director::SceneState::PREPARE: col = 0xFF505E4B; break;
                case Director::SceneState::RUNNING: col = 0xFF205E1B; break;
                case Director::SceneState::TIMEOUT: col = 0xFF666666; break;
                case Director::SceneState::BALL_PLACEMENT:  col = 0xFF880074; break;
                default: break;
            }

            float xPos = logBtnScreenPos.x + logBtnSize.x * (double)block.tStart_ns_/(double)pGameLog->getTotalDuration_ns();
            float yPos = logBtnScreenPos.y + 24.0f;
            float xPosEnd = logBtnScreenPos.x + logBtnSize.x * (double)block.tEnd_ns_/(double)pGameLog->getTotalDuration_ns();
            float yPosEnd = logBtnScreenPos.y + logBtnSize.y - 4.0f;
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(xPos, yPos), ImVec2(xPosEnd, yPosEnd), col);
        }

        for(const auto& marker : pGameLog->getSyncMarkers())
        {
            float xPos = logBtnScreenPos.x + logBtnSize.x * (double)marker.timestamp_ns/(double)pGameLog->getTotalDuration_ns();
            float yPos = logBtnScreenPos.y - 1.0f;
            ImGui::GetWindowDrawList()->AddLine(ImVec2(xPos, yPos), ImVec2(xPos, yPos+logBtnSize.y), 0xFF00FF00, 2.0f);
        }
    }

    // draw cameras
    int recordingIndex = 0;
    for(const auto& pCamera : pProject_->getCameras())
    {
        ImGui::Text(pCamera->getName().c_str());
        ImGui::SameLine(100.0f);

        for(const auto& pRecording : pCamera->getVideos())
        {
            ImGui::InvisibleButton((pRecording->getName() + "_gap").c_str(), ImVec2(pRecording->frontGap_ns_ * scaleX, heightCamera));
            ImGui::SameLine(0.0f, 0.0f);

            ImVec2 btnScreenPos = ImGui::GetCursorScreenPos();
            if(ImGui::Button(pRecording->getName().c_str(), ImVec2(pRecording->pVideo_->getDuration_ns() * scaleX, heightCamera)))
            {
                recordingIndex_ = recordingIndex;
                recordingAutoPlay_ = false;

                // TODO: or try to sync with bufferedRecordingTimes_? need to construct name then to match keys
                if(pRecording->syncMarker_.has_value())
                    recordingTime_s_ = pRecording->syncMarker_->timestamp_ns * 1e-9;
                else
                    recordingTime_s_ = 0.0f;
            }

            ImVec2 btnSize = ImGui::GetItemRectSize();

            if(pRecording->syncMarker_.has_value())
            {
                float xPos = btnScreenPos.x + btnSize.x * (double)pRecording->syncMarker_->timestamp_ns/(double)pRecording->pVideo_->getDuration_ns();
                float yPos = btnScreenPos.y - 1.0f;
                ImGui::GetWindowDrawList()->AddLine(ImVec2(xPos, yPos), ImVec2(xPos, yPos+btnSize.y), 0xFF00FF00, 2.0f);
            }

            ImGui::SameLine(0.0f, 0.0f);

            recordingIndex++;
        }

        ImGui::NewLine();
    }

    ImGui::PopStyleVar();

    ImGui::End();
}

void TigersClav::drawGameLogPanel()
{
    ImGui::Begin("Gamelog");

    const ImVec2 regionAvail = ImGui::GetContentRegionAvail();

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

        if(ImGui::BeginTable("##Markers", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter))
        {
            // Time, Name, GoTo, Delete
            std::vector<SyncMarker>::iterator removeIter = pGameLog->getSyncMarkers().end();

            for(auto iter = pGameLog->getSyncMarkers().begin(); iter != pGameLog->getSyncMarkers().end(); iter++)
            {
                auto marker = *iter;

                ImGui::PushID(marker.name.c_str());

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%.3f", marker.timestamp_ns * 1e-9);

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::Text(marker.name.c_str());

                ImGui::TableNextColumn();
                if(ImGui::Button("Jump To"))
                {
                    gameLogTime_s_ = marker.timestamp_ns * 1e-9;
                    pGameLog->seekTo(gameLogTime_s_ * 1e9);
                }

                ImGui::TableNextColumn();
                if(ImGui::Button("Delete"))
                {
                    removeIter = iter;
                }

                ImGui::PopID();
            }

            if(removeIter != pGameLog->getSyncMarkers().end())
                pGameLog->getSyncMarkers().erase(removeIter);

            ImGui::EndTable();
        }

        if(ImGui::Button("Add Marker"))
            ImGui::OpenPopup("MarkerName");

        if(ImGui::BeginPopup("MarkerName"))
        {
            ImGui::InputText("Marker Name", markerNameBuf_,  sizeof(markerNameBuf_));

            auto findIter = std::find_if(pGameLog->getSyncMarkers().begin(), pGameLog->getSyncMarkers().end(), [&](SyncMarker& marker){ return marker.name == std::string(markerNameBuf_); });
            if(findIter == pGameLog->getSyncMarkers().end())
            {
                if(ImGui::Button("Create"))
                {
                    SyncMarker marker;
                    marker.name = std::string(markerNameBuf_);
                    marker.timestamp_ns = gameLogTime_s_ * 1e9;

                    pGameLog->getSyncMarkers().push_back(marker);

                    markerNameBuf_[0] = 0;
                    ImGui::CloseCurrentPopup();
                }
            }
            else
            {
                ImGui::Text("Duplicate marker name!");
            }

            ImGui::EndPopup();
        }

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

    std::vector<std::pair<std::string, std::shared_ptr<VideoRecording>>> recordings;

    for(auto pCam : pProject_->getCameras())
    {
        for(std::shared_ptr<VideoRecording> pRec : pCam->getVideos())
        {
            recordings.push_back(std::make_pair(pCam->getName() + " - " + pRec->getName(), pRec));
        }
    }

    if(!recordings.empty())
    {
        if(recordingIndex_ < 0 || recordingIndex_ >= recordings.size())
            recordingIndex_ = 0;

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Source:");
        ImGui::SameLine();

        ImGui::SetNextItemWidth(-1.0f);
        if(ImGui::BeginCombo("##RecordingSrc", recordings.at(recordingIndex_).first.c_str()))
        {
            for(size_t iRec = 0; iRec < recordings.size(); iRec++)
            {
                bool isSelected = iRec == recordingIndex_;

                if(ImGui::Selectable(recordings.at(iRec).first.c_str(), isSelected))
                {
                    recordingIndex_ = iRec;

                    auto bufIter = bufferedRecordingTimes_.find(recordings.at(iRec).first);
                    if(bufIter != bufferedRecordingTimes_.end())
                    {
                        if(bufIter->second >= 0.0f && bufIter->second <= recordings[iRec].second->pVideo_->getDuration_ns()*1e-9)
                        {
                            recordingTime_s_ = bufIter->second;
                        }
                        else
                            recordingTime_s_ = 0.0f;
                    }
                    else
                    {
                        recordingTime_s_ = 0.0f;
                    }

                    recordingAutoPlay_ = false;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
    }

    if(recordingIndex_ >= recordings.size())
    {
        recordingIndex_ = -1;
    }

    if(recordingIndex_ >= 0)
    {
        bufferedRecordingTimes_[recordings.at(recordingIndex_).first] = recordingTime_s_;

        std::shared_ptr<VideoRecording> pRecording = recordings.at(recordingIndex_).second;
        std::shared_ptr<Video> pVideo = pRecording->pVideo_;

        Video::CacheLevels cacheLevels = pVideo->getCacheLevels();

        cacheLevelBuffer_.push_back(cacheLevels);
        while(cacheLevelBuffer_.size() > 200)
            cacheLevelBuffer_.pop_front();

        float tMax_s = pVideo->getDuration_ns() * 1e-9f;
        float dt_s = pVideo->getFrameDeltaTime();

        float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::PushButtonRepeat(true);
        if(ImGui::ArrowButton("##left", ImGuiDir_Left) || (recordingSliderHovered_ && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)))
        {
            if(recordingTime_s_ > dt_s)
                recordingTime_s_ -= dt_s;

            recordingAutoPlay_ = false;
        }

        ImGui::SameLine(0.0f, spacing);
        ImGui::SetNextItemWidth(-78.0f);
        ImVec2 camTimePos = ImGui::GetCursorPos();
        ImVec2 camTimePosScreen = ImGui::GetCursorScreenPos();
        ImGui::SliderFloat("##CameraTime", &recordingTime_s_, 0.0f, tMax_s, "%.3fs", ImGuiSliderFlags_AlwaysClamp);
        recordingSliderHovered_ = ImGui::IsItemHovered();
        if(ImGui::IsItemEdited())
            recordingAutoPlay_ = false;

        ImVec2 camTimeSize = ImGui::GetItemRectSize();

        ImGui::SameLine(0.0f, spacing);
        const ImVec2 seekNextPos = ImGui::GetCursorPos();
        if(ImGui::ArrowButton("##right", ImGuiDir_Right) || (recordingSliderHovered_ && ImGui::IsKeyPressed(ImGuiKey_RightArrow)))
        {
            if(recordingTime_s_+dt_s < tMax_s)
                recordingTime_s_ += dt_s;

            recordingAutoPlay_ = false;
        }

        ImGui::PopButtonRepeat();

        ImGui::SameLine(0.0f, spacing);

        if(recordingAutoPlay_)
        {
//            if(pCamera->getLastCacheLevels().after > 0.33f)
                recordingTime_s_ += ImGui::GetIO().DeltaTime;

            if(ImGui::Button("Pause", ImVec2(50, 0)))
            {
                recordingAutoPlay_ = false;
            }
        }
        else
        {
            if(ImGui::Button("Play", ImVec2(50, 0)))
            {
                recordingAutoPlay_ = true;
            }
        }

        // Marker drawing
        if(pRecording->syncMarker_.has_value())
        {
            const float markerWidth = 10.0f;

            camTimePos.x += ImGui::GetStyle().GrabMinSize/2 + 2.0f;
            camTimePosScreen.x += ImGui::GetStyle().GrabMinSize/2 + 2.0f;
            camTimeSize.x -= ImGui::GetStyle().GrabMinSize + 4.0f;

            float lineBottom = ImGui::GetCursorScreenPos().y;

            float markerTime = pRecording->syncMarker_->timestamp_ns * 1e-9;

            ImGui::SetCursorPosX(camTimePos.x + camTimeSize.x * markerTime/tMax_s - markerWidth/2);

            ImGui::PushStyleColor(ImGuiCol_Button, 0xFF0000B0);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0xFF0000B0);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0xFF0000FF);

            if(ImGui::Button("##Marker", ImVec2(markerWidth, 0)))
            {
                recordingTime_s_ = markerTime;
                recordingAutoPlay_ = false;
            }

            if(ImGui::BeginPopupContextItem())
            {
                if(ImGui::Button("Delete"))
                {
                    pRecording->syncMarker_.reset();
                }

                ImGui::EndPopup();
            }

            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Name: %s\nTime: %.3fs", pRecording->syncMarker_->name.c_str(), markerTime);
            }

            ImGui::PopStyleColor(3);

            ImGui::SameLine();

            ImGui::GetWindowDrawList()->AddLine(ImVec2(camTimePosScreen.x + camTimeSize.x * markerTime/tMax_s - 1.0f, camTimePosScreen.y),
                            ImVec2(camTimePosScreen.x + camTimeSize.x * markerTime/tMax_s - 1.0f, lineBottom), 0xFF0000B0, 2.0f);

            ImGui::NewLine();
        }
        else
        {
            ImGui::SetCursorPosX(camTimePos.x + camTimeSize.x/2 - 100.0f);
            if(ImGui::Button("Set Marker", ImVec2(200.0f, 0.0f)))
                ImGui::OpenPopup("VideoMarker");

            if(ImGui::BeginPopup("VideoMarker"))
            {
                // TODO: list markers from gamelog for easy name copy?
                ImGui::InputText("Marker Name", markerNameBuf_,  sizeof(markerNameBuf_));

                if(ImGui::Button("Create"))
                {
                    SyncMarker marker;
                    marker.name = std::string(markerNameBuf_);
                    marker.timestamp_ns = recordingTime_s_ * 1e9;

                    pRecording->syncMarker_ = marker;

                    markerNameBuf_[0] = 0;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }

        // Video cache drawing
        std::vector<float> before;
        std::vector<float> after;

        for(Video::CacheLevels levels : cacheLevelBuffer_)
        {
            before.push_back(levels.before);
            after.push_back(levels.after);
        }

        char beforeText[16];
        char afterText[16];

        snprintf(beforeText, sizeof(beforeText), "%.0f%%", cacheLevels.before*100.0f);
        snprintf(afterText, sizeof(afterText), "%.0f%%", cacheLevels.after*100.0f);

        ImGui::Separator();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Frame Cache:");
        ImGui::SameLine(120.0f);
        ImGui::PlotLines("##Cache before", before.data(), before.size(), 0, beforeText, 0.0f, 1.0f, ImVec2((regionAvail.x - 120.0f)*0.5f, ImGui::GetFrameHeight()));
        ImGui::SameLine();
        ImGui::PlotLines("##Cache after", after.data(), after.size(), 0, afterText, 0.0f, 1.0f, ImVec2((regionAvail.x - 120.0f)*0.5f, ImGui::GetFrameHeight()));

        // Play logic and finally frame drawing
        if(recordingTime_s_ > tMax_s)
        {
            recordingTime_s_ = tMax_s;
            recordingAutoPlay_ = false;
        }

        AVFrame* pFrame = pVideo->getFrameByTime(recordingTime_s_ * 1e9);
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
