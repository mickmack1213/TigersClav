#include "TigersClav.hpp"
#include <cstdio>
#include <cstdint>
#include <stdexcept>
#include <fstream>
#include <memory>
#include "util/gzstream.h"
#include "ImGuiFileDialog.h"
#include "util/CustomFont.h"
#include "LogViewer.hpp"
#include <chrono>
#include <iomanip>

TigersClav::TigersClav()
:lastFileOpenPath_("."),
 drawVideoFrame_(false),
 gameLogRefPos_(0),
 gameLogRefPosHovered_(false),
 tPlayGamelog_ns_(-1)
{
    glGenTextures(1, &scoreBoardTexture_);
    glGenTextures(1, &fieldVisualizerTexture_);

    pScoreBoard_ = std::make_unique<ScoreBoard>();
    pFieldVisualizer_ = std::make_unique<FieldVisualizer>();
    pImageComposer_ = std::make_unique<ImageComposer>(ImVec2(3840, 2160));

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
        if(ImGui::BeginMenu("File"))
        {
            if(ImGui::MenuItem("Load Gamelog", "CTRL+O"))
            {
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", "Gamelogs {.log,.gz}", lastFileOpenPath_, 1, 0, ImGuiFileDialogFlags_Modal);
            }

            if(ImGui::MenuItem("Load Video"))
            {
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".*", lastFileOpenPath_, 1, (void*)1, ImGuiFileDialogFlags_Modal);
            }

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
            if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
            ImGui::Separator();

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::End();

    // File Dialog handling
    if(ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, ImVec2(500, 500)))
    {
        // action if OK
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            lastFileOpenPath_ = ImGuiFileDialog::Instance()->GetCurrentPath() + "/";

            std::ofstream openPath("lastFileOpenPath.txt", std::ofstream::trunc);
            openPath << lastFileOpenPath_;
            openPath.close();

            uint64_t type = (uint64_t)ImGuiFileDialog::Instance()->GetUserDatas();

            if(type == 0)
            {
                pGameLog_ = std::make_unique<SSLGameLog>(ImGuiFileDialog::Instance()->GetFilePathName(),
                                std::set<SSLMessageType>{ MESSAGE_SSL_REFBOX_2013, MESSAGE_SSL_VISION_TRACKER_2020, MESSAGE_SSL_VISION_2014 });
                gameLogRefPos_ = 0;
                pFieldVisualizer_->setGeometry(nullptr);
                trackerSources_.clear();
            }
            else if(type == 1)
            {
                pVideo_ = std::make_unique<Video>(ImGuiFileDialog::Instance()->GetFilePathName());
            }
        }

        // close
        ImGuiFileDialog::Instance()->Close();
    }

    // Log Panel
    el::Helpers::logDispatchCallback<LogViewer>("LogViewer")->render();

    // Gamelog Panel
    ImGui::SetNextWindowDockID(dockspaceId, ImGuiCond_FirstUseEver);
    ImGui::Begin("Gamelog");

    ImVec2 regionAvail = ImGui::GetContentRegionAvail();

    const float aspectRatioScoreBoard = (float)pScoreBoard_->getImageData().size.h / pScoreBoard_->getImageData().size.w;
    const float aspectRatioField = (float)pFieldVisualizer_->getImageData().size.h / pFieldVisualizer_->getImageData().size.w;

    createGamestateTextures();

    ImGui::Image((void*)(intptr_t)scoreBoardTexture_, ImVec2(regionAvail.x, regionAvail.x*aspectRatioScoreBoard));
    ImGui::Image((void*)(intptr_t)fieldVisualizerTexture_, ImVec2(regionAvail.x, regionAvail.x*aspectRatioField));

    if(pGameLog_)
    {
        if(pGameLog_->isLoaded() && !pGameLog_->isEmpty(MESSAGE_SSL_REFBOX_2013))
        {
            // Logic
            if(!pFieldVisualizer_->hasGeometry())
            {
                auto visionIter = pGameLog_->begin(MESSAGE_SSL_VISION_2014);
                while(visionIter != pGameLog_->end(MESSAGE_SSL_VISION_2014))
                {
                    auto optVision = pGameLog_->convertTo<SSL_WrapperPacket>(visionIter);
                    if(optVision && optVision->has_geometry())
                    {
                        LOG(INFO) << "Found Geometry Frame. "
                                  << optVision->geometry().field().field_length() << "x" << optVision->geometry().field().field_width();

                        pFieldVisualizer_->setGeometry(optVision);
                        break;
                    }

                    visionIter++;
                }
            }

            // Data info and stats
            SSLGameLog::MsgMapIter refIter;

            if(tPlayGamelog_ns_ > 0)
            {
                refIter = pGameLog_->findLastMsgBeforeTimestamp(MESSAGE_SSL_REFBOX_2013, tPlayGamelog_ns_);
                gameLogRefPos_ = std::distance(pGameLog_->begin(MESSAGE_SSL_REFBOX_2013), refIter);
            }
            else
            {
                refIter = pGameLog_->begin(MESSAGE_SSL_REFBOX_2013);
                std::advance(refIter, gameLogRefPos_);
            }
            auto optRef = pGameLog_->convertTo<Referee>(refIter);

            auto trackerIter = pGameLog_->findLastMsgBeforeTimestamp(MESSAGE_SSL_VISION_TRACKER_2020, refIter->first);
            auto optTracker = pGameLog_->convertTo<TrackerWrapperPacket>(trackerIter);

            if(optTracker)
            {
                trackerSources_[optTracker->uuid()] = optTracker->has_source_name() ? optTracker->source_name() : "Unknown";

                while(trackerIter != pGameLog_->end(MESSAGE_SSL_VISION_TRACKER_2020) && !preferredTracker_.empty() && optTracker->uuid() != preferredTracker_)
                {
                    optTracker = pGameLog_->convertTo<TrackerWrapperPacket>(trackerIter);

                    trackerIter++;
                }

                pFieldVisualizer_->update(optTracker);
            }

            if(optRef)
            {
                pScoreBoard_->update(optRef);
            }

            // Slider
            size_t numRefMessages = pGameLog_->getNumMessages(MESSAGE_SSL_REFBOX_2013);

            float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
            ImGui::PushButtonRepeat(true);
            if(ImGui::ArrowButton("##left", ImGuiDir_Left) || (gameLogRefPosHovered_ && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)))
            {
                if(gameLogRefPos_ > 0)
                    gameLogRefPos_--;

                tPlayGamelog_ns_ = -1;
            }

            ImGui::SameLine(0.0f, spacing);
            ImGui::SetNextItemWidth(-78.0f);
            ImGui::SliderInt("##Ref Message Pos", &gameLogRefPos_, 0, numRefMessages-1, "%d", ImGuiSliderFlags_AlwaysClamp);
            gameLogRefPosHovered_ = ImGui::IsItemHovered();
            if(ImGui::IsItemEdited())
                tPlayGamelog_ns_ = -1;

            ImGui::SameLine(0.0f, spacing);
            if(ImGui::ArrowButton("##right", ImGuiDir_Right) || (gameLogRefPosHovered_ && ImGui::IsKeyPressed(ImGuiKey_RightArrow)))
            {
                if(gameLogRefPos_ < numRefMessages-1)
                    gameLogRefPos_++;

                tPlayGamelog_ns_ = -1;
            }

            ImGui::PopButtonRepeat();

            ImGui::SameLine(0.0f, spacing);

            if(tPlayGamelog_ns_ < 0)
            {
                if(ImGui::Button("Play", ImVec2(50, 0)))
                {
                    tPlayGamelog_ns_ = refIter->first;
                }

            }
            else
            {
                tPlayGamelog_ns_ += (int64_t)((double)ImGui::GetIO().DeltaTime * 1e9);

                if(ImGui::Button("Pause", ImVec2(50, 0)) || tPlayGamelog_ns_ > pGameLog_->getLastTimestamp_ns())
                {
                    tPlayGamelog_ns_ = -1;
                }
            }

            // Tracker source selection
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Tracker Source: ");
            ImGui::SameLine();

            for(const auto& source : trackerSources_)
            {
                if(ImGui::RadioButton(source.second.c_str(), source.first == preferredTracker_))
                    preferredTracker_ = source.first;

                ImGui::SameLine();
            }

            ImGui::NewLine();

            if(optRef)
            {
                uint64_t timestamp_us = optRef->packet_timestamp();
                int64_t unixTimestamp = timestamp_us / 1000000;
                std::tm tm = *std::localtime(&unixTimestamp);
                std::stringstream dateStream;
                dateStream << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
                ImGui::Text("Time: %s.%03u", dateStream.str().c_str(), (timestamp_us / 1000) % 1000);
            }
        }

        if(ImGui::CollapsingHeader("Gamelog Details", ImGuiTreeNodeFlags_DefaultOpen))
        {
            SSLGameLogStats stats = pGameLog_->getStats();

            ImGui::Text("Gamelog: %s", pGameLog_->getFilename().c_str());
            ImGui::Text("Type: %s", stats.type.c_str());
            ImGui::Text("Format: %d", stats.formatVersion);
            ImGui::Text("Size: %.1fMB", stats.totalSize/(1024.0f*1024.0f));
            ImGui::Text("msgs: %u", stats.numMessages);
            ImGui::Text("vision2010: %u", stats.numMessagesPerType[MESSAGE_SSL_VISION_2010]);
            ImGui::Text("vision2014: %u", stats.numMessagesPerType[MESSAGE_SSL_VISION_2014]);
            ImGui::Text("ref:        %u", stats.numMessagesPerType[MESSAGE_SSL_REFBOX_2013]);
            ImGui::Text("tracker:    %u", stats.numMessagesPerType[MESSAGE_SSL_VISION_TRACKER_2020]);
            ImGui::Text("Duration: %.1fs", stats.duration_s);
        }
    }

    ImGui::End();

    // Video Panel
    ImGui::SetNextWindowDockID(dockspaceId, ImGuiCond_FirstUseEver);
    ImGui::Begin("Video");

    ImGui::Checkbox("Draw videoframe", &drawVideoFrame_);

    pImageComposer_->begin();

    if(pVideo_ && pVideo_->isLoaded() && drawVideoFrame_)
    {
        AVFrame* pFrame = pVideo_->getFrame(98765);

        pImageComposer_->drawVideoFrameRGB(pFrame);
    }

    pImageComposer_->end();

    ImGui::Image((void*)(intptr_t)pImageComposer_->getTexture(), ImVec2(3840/4, 2160/4));

    ImGui::End();

    // Drawing and interaction tests
/*    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 orig = ImGui::GetCursorScreenPos();
    orig.x += 50;
    ImGui::SetCursorScreenPos(orig);

    const ImVec2 p = ImGui::GetCursorScreenPos();
    float x = p.x + 4.0f;
    float y = p.y + 4.0f;
    float sz = 36.0f;
    ImVec4 colf = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
    const ImU32 col = ImColor(colf);
    float th = 1.0f;

    static float offset = 0.0f;

    // draw calls do not affect cursor pos
    draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x, y + sz), col);
    draw_list->AddRect(ImVec2(x+offset, y), ImVec2(x + offset + sz, y + sz), ImColor(1.0f, 0.0f, 0.0f, 1.0f), 0.0f, ImDrawFlags_None, th);

    // button starts at cursor pos
    ImGui::InvisibleButton("canvas", ImGui::GetContentRegionAvail());
    if (ImGui::IsItemActive())
    {
        draw_list->AddLine(ImGui::GetIO().MouseClickedPos[0], ImGui::GetIO().MousePos, ImGui::GetColorU32(ImGuiCol_Button), 4.0f); // Draw a line between the button and the mouse cursor
        offset += ImGui::GetIO().MouseDelta.x;
    }
*/
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
