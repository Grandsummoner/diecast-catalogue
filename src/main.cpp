#include "types.hpp"
#include "utils.hpp"
#include "theme.hpp"

#include <algorithm> // Critical: Resolves std::sort and std::transform errors
#include <ctime>
#include <cstdlib>
#include <filesystem> // Essential to resolve filesystem operations

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace fs = std::filesystem;

// External linkage safeguards
extern int g_DraggedPolaroidIndex;

// Global swipe offsets for Tinder-style Faceoff animations
float g_LeftCardSwipeOffset = 0.0f;
float g_RightCardSwipeOffset = 0.0f;

// Native Win32 popup uploader file-pickers
std::vector<std::string> openFileDialog();
std::string openFolderDialog();

// -------------------------------------------------------------
// DYNAMIC GLOBAL SCALING ENGINE (Ctrl + Scroll Wheel Zoom)
// -------------------------------------------------------------
void ScaleStyleAndFont(float scale) {
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = scale;

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 16.0f * scale;
    style.ChildRounding = 12.0f * scale;
    style.FrameRounding = 10.0f * scale;
    style.GrabRounding = 10.0f * scale;
    style.PopupRounding = 10.0f * scale;
    style.ScrollbarRounding = 10.0f * scale;
    style.WindowPadding = ImVec2(12.0f * scale, 12.0f * scale);
    style.FramePadding = ImVec2(8.0f * scale, 4.0f * scale);
    style.ItemSpacing = ImVec2(8.0f * scale, 6.0f * scale);
}

// -------------------------------------------------------------
// MAIN RENDER LOOP & WINDOW callbacks
// -------------------------------------------------------------

int main() {
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(1440, 800, "My Collection - Diecast Photo Catalogue", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window); glfwSwapInterval(1);

    // Bind system OS callbacks to native GLFW windows
    glfwSetWindowCloseCallback(window, glfw_window_close_callback);
    glfwSetDropCallback(window, glfw_drop_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyTheme(g_ActiveTheme);

    ImGuiIO& io = ImGui::GetIO();
    // System Font Loader: Use Comic Sans MS throughout the visual elements
    std::string fontPath = "C:\\Windows\\Fonts\\comic.ttf";
    if (fs::exists(fontPath)) {
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
    } else {
        io.Fonts->AddFontDefault();
    }

    // Initialize global scaling size on startup
    ScaleStyleAndFont(g_GlobalScale);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Startup Load: Automatically restore the last saved workspace state if present
    if (fs::exists("workspace.zip")) {
        importWorkspace("workspace.zip");
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Gacha roller animation calculations
        if (g_GachaRolling && !g_Catalog.empty()) {
            g_GachaTimer += 0.016f; g_GachaDuration += 0.016f;
            if (g_GachaTimer >= g_GachaInterval) {
                g_GachaTimer = 0.0f;
                g_SelectedCarIndex = (g_SelectedCarIndex + 1) % g_Catalog.size();
                g_GachaInterval += 0.015f;
            }
            if (g_GachaDuration >= 2.0f) {
                g_GachaRolling = false; g_GachaInterval = 0.05f;
                g_SelectedCarIndex = std::rand() % g_Catalog.size();
            }
        }

        // Global Ctrl + Mouse Wheel Dynamic Zoom / Symmetrical Scaling handler
        if (io.KeyCtrl && io.MouseWheel != 0.0f) {
            g_GlobalScale += io.MouseWheel * 0.05f;
            g_GlobalScale = (std::max)(0.8f, (std::min)(2.0f, g_GlobalScale));
            ScaleStyleAndFont(g_GlobalScale);
            g_UnsavedChanges = true;
        }

        // Handle basic keyboard shortcuts: Arrow keys to page forward and back
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && !g_Catalog.empty()) {
            g_SelectedCarIndex = (g_SelectedCarIndex - 1 + g_Catalog.size()) % g_Catalog.size();
            g_ChatLog.clear();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && !g_Catalog.empty()) {
            g_SelectedCarIndex = (g_SelectedCarIndex + 1) % g_Catalog.size();
            g_ChatLog.clear();
        }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
        ImGui::Begin("Workspace", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // -------------------------------------------------------------
        // TOP CONTROL BAR
        // -------------------------------------------------------------
        ImGui::BeginChild("TopBar", ImVec2(0, 45), true);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.30f, 0.48f, 1.00f, 1.00f), "My Collection"); ImGui::SameLine();
        
        // Editable Collection Name Field
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("##CollName", g_CollectionName, IM_ARRAYSIZE(g_CollectionName))) {
            g_UnsavedChanges = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Save")) exportWorkspace("workspace.zip"); ImGui::SameLine();
        if (ImGui::Button("Undo")) ExecuteUndo(); ImGui::SameLine();
        if (ImGui::Button("Redo")) ExecuteRedo(); ImGui::SameLine();
        ImGui::TextDisabled("|"); ImGui::SameLine();
        ImGui::Text("Total: %zu", g_Catalog.size()); ImGui::SameLine();
        ImGui::TextDisabled("|"); ImGui::SameLine();
        if (ImGui::Button("Gacha Spin")) {
            if (!g_GachaRolling && !g_Catalog.empty()) {
                g_GachaRolling = true; g_GachaTimer = 0.0f; g_GachaInterval = 0.02f; g_GachaDuration = 0.0f; // Fast spin
            }
        }
        ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine();
        ImGui::Text("%s", g_SoundscapeNames[g_ActiveSoundscape]); ImGui::SameLine();
        if (ImGui::Button("Toggle Ambient")) {
            g_ActiveSoundscape = (g_ActiveSoundscape + 1) % 4;
            TriggerLoopAudioEffect();
        }
        ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine();

        // Color theme selectors based on Image 2 brand specifications
        ImGui::SetNextItemWidth(130);
        if (ImGui::Combo("Theme", (int*)&g_ActiveTheme, "Blue\0Green\0Purple\0Gray\0Orange\0Red\0Pink\0Yellow\0")) {
            ApplyTheme(g_ActiveTheme);
        }

        ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine();
        if (ImGui::Button("Export ZIP")) exportWorkspace("workspace.zip");
        ImGui::EndChild();

        // Proportional alignments: 20% Left panel, 50% Middle panel, 30% Right panel width
        float leftPanelWidth = (float)width * 0.20f;
        float middlePanelWidth = (float)width * 0.50f;
        float rightPanelWidth = (float)width * 0.30f;
        float bodyHeight = (float)height - 65.0f;

        // -------------------------------------------------------------
        // PANEL 1: LEFT PANEL — PHOTO & FILE MANAGEMENT (20% Width)
        // -------------------------------------------------------------
        ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth - 10.0f, bodyHeight), true);
        
        // Polished Neumorphic Upload Explorer Drop-Zone Card (No more "Ingest" word!)
        ImGui::BeginChild("UploadExplorerCard", ImVec2(0, 125), true);
        ImGui::Text(" 📂 Drag & Drop here");
        ImGui::TextDisabled(" Or use manual uploader:");
        if (ImGui::Button("Upload Files")) {
            g_PendingImportPaths = openFileDialog();
            if (!g_PendingImportPaths.empty()) {
                g_ShowUploadConfirmPrompt = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Upload Folder")) {
            std::string folder = openFolderDialog();
            if (!folder.empty()) {
                g_PendingImportPaths.clear();
                scanAndQueuePath(folder);
                if (!g_PendingImportPaths.empty()) {
                    g_ShowUploadConfirmPrompt = true;
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::InputText("##Search", g_SearchInput, IM_ARRAYSIZE(g_SearchInput));
        ImGui::SameLine();
        ImGui::Button("Filter");
        ImGui::Text("Selection Queue:");
        if (ImGui::Button("Select All")) {
            PushHistoryState();
            for (auto& car : g_Catalog) car.selected = true;
            g_UnsavedChanges = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Deselect All")) {
            PushHistoryState();
            for (auto& car : g_Catalog) car.selected = false;
            g_UnsavedChanges = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Unload")) {
            PushHistoryState();
            g_Catalog.erase(std::remove_if(g_Catalog.begin(), g_Catalog.end(), [](const DiecastCar& c) { return c.selected; }), g_Catalog.end());
            g_SelectedCarIndex = g_Catalog.empty() ? -1 : 0;
            g_UnsavedChanges = true;
        }
        ImGui::Separator();
        ImGui::BeginChild("IndexedGrid", ImVec2(0, bodyHeight - 310.0f), false);
        for (int i = 0; i < (int)g_Catalog.size(); ++i) {
            auto& car = g_Catalog[i];
            if (strlen(g_SearchInput) > 0) {
                std::string matchQuery = car.filename;
                std::transform(matchQuery.begin(), matchQuery.end(), matchQuery.begin(), ::tolower);
                std::string searchStr = g_SearchInput;
                std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                if (matchQuery.find(searchStr) == std::string::npos) continue;
            }
            ImGui::PushID(i);
            bool isSelected = (g_SelectedCarIndex == i);
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
                // Force high-contrast white text for selected cards to fix dark-blue/black text unreadability
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
            }

            ImGui::BeginChild((std::string("CardGridChild##") + std::to_string(i)).c_str(), ImVec2(105, 110), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::Checkbox("##SelectCheck", &car.selected); ImGui::SameLine();
            if (ImGui::Button(car.favorite ? "★" : "☆")) {
                PushHistoryState();
                car.favorite = !car.favorite;
                g_StarredCount += car.favorite ? 1 : -1;
                g_UnsavedChanges = true;
            }
            GLuint thumbID = GetOrCreateTexture(car.folderPath);
            if (thumbID != 0) ImGui::Image((void*)(intptr_t)thumbID, ImVec2(80, 45));
            else ImGui::Text("   🚗   ");

            std::string shortName = car.make + " " + car.model;
            if (shortName.length() > 10) shortName = shortName.substr(0, 8) + "..";
            ImGui::TextWrapped("%s", shortName.c_str());
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) g_SelectedCarIndex = i;
            ImGui::EndChild();
            ImGui::PopStyleColor();
            if (isSelected) ImGui::PopStyleColor(); // Pop both ChildBg and Text
            ImGui::PopID();
            if ((i + 1) % 2 != 0) ImGui::SameLine();
        }
        ImGui::EndChild();
        ImGui::Button("Re-link root directory");
        ImGui::EndChild();

        // -------------------------------------------------------------
        // PANEL 2: MIDDLE PANEL — PRESENTATION STAGE (50% Width)
        // -------------------------------------------------------------
        ImGui::SameLine();
        ImGui::BeginChild("MiddlePanel", ImVec2(middlePanelWidth - 10.0f, bodyHeight), true);
        if (ImGui::BeginTabBar("DisplayStageTabs")) {
            if (ImGui::BeginTabItem("Showcase View")) {
                if (g_SelectedCarIndex >= 0 && g_SelectedCarIndex < (int)g_Catalog.size()) {
                    auto& car = g_Catalog[g_SelectedCarIndex];
                    ImGui::Text("ASK COLLECTOR BOT:"); ImGui::SameLine();
                    ImGui::InputText("##BotPrompt", g_ChatInput, IM_ARRAYSIZE(g_ChatInput)); ImGui::SameLine();
                    if (ImGui::Button("Ask")) {
                        std::string q = g_ChatInput;
                        if (!q.empty()) {
                            g_ChatLog.push_back({ "You", q });
                            std::string ans = getGeminiChatResponse(q, car);
                            g_ChatLog.push_back({ "Bot", ans });
                            memset(g_ChatInput, 0, sizeof(g_ChatInput));
                        }
                    }
                    ImGui::Separator();
                    ImGui::Text("DIECAST IDENTIFIER: %s", car.filename.c_str());
                    ImGui::Text("TAGS: [ %s ]  [ + Add Tag ]", car.tags.c_str());

                    ImDrawList* drawList = ImGui::GetWindowDrawList(); ImVec2 canvasCursor = ImGui::GetCursorScreenPos();
                    float canvasWidth = middlePanelWidth - 30.0f;
                    float canvasHeight = canvasWidth * (9.0f / 16.0f); // Cinema style 16:9 Viewport
                    ImVec2 frameSize = ImVec2(canvasWidth, canvasHeight);

                    ImU32 sunkenTrackBg = GetThemeSunkenTrackColor(g_ActiveTheme);
                    drawList->AddRectFilled(canvasCursor, ImVec2(canvasCursor.x + frameSize.x, canvasCursor.y + frameSize.y), sunkenTrackBg);
                    drawList->AddRect(canvasCursor, ImVec2(canvasCursor.x + frameSize.x, canvasCursor.y + frameSize.y), IM_COL32(0, 168, 255, 100), 12.0f, 0, 1.5f);

                    GLuint mainTexID = GetOrCreateTexture(car.folderPath);
                    if (mainTexID != 0) {
                        float canvasAspect = frameSize.x / frameSize.y;
                        float imgAspect = (float)g_ActiveTextureWidth / (float)g_ActiveTextureHeight;
                        ImVec2 displaySize; ImVec2 offset = ImVec2(0, 0);
                        if (imgAspect > canvasAspect) {
                            displaySize.x = frameSize.x; displaySize.y = frameSize.x / imgAspect; offset.y = (frameSize.y - displaySize.y) / 2.0f;
                        } else {
                            displaySize.y = frameSize.y; displaySize.x = frameSize.y * imgAspect; offset.x = (frameSize.x - displaySize.x) / 2.0f;
                        }
                        ImGui::SetCursorScreenPos(ImVec2(canvasCursor.x + offset.x, canvasCursor.y + offset.y));
                        ImGui::Image((void*)(intptr_t)mainTexID, displaySize);
                        ImGui::SetCursorScreenPos(canvasCursor); ImGui::Dummy(frameSize);
                    } else {
                        drawList->AddRectFilled(canvasCursor, ImVec2(canvasCursor.x + frameSize.x, canvasCursor.y + frameSize.y), IM_COL32(15, 23, 42, 255));
                        ImVec2 textPos = ImVec2(canvasCursor.x + (frameSize.x / 2.0f) - 40.0f, canvasCursor.y + (frameSize.y / 2.0f) - 10.0f);
                        drawList->AddText(textPos, IM_COL32(180, 180, 180, 255), "No Photo");
                        ImGui::Dummy(frameSize);
                    }
                    ImGui::Spacing();
                    if (ImGui::Button("< Previous Photo", ImVec2((middlePanelWidth / 2.0f) - 20.0f, 35.0f))) {
                        g_SelectedCarIndex = (g_SelectedCarIndex - 1 + g_Catalog.size()) % g_Catalog.size(); g_ChatLog.clear();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Next Photo >", ImVec2((middlePanelWidth / 2.0f) - 20.0f, 35.0f))) {
                        g_SelectedCarIndex = (g_SelectedCarIndex + 1) % g_Catalog.size(); g_ChatLog.clear();
                    }
                    ImGui::Separator();
                    ImGui::Text("CURATOR'S NOTES (📝):");
                    ImGui::InputTextMultiline("##NotesEditor", g_CuratorNotes, IM_ARRAYSIZE(g_CuratorNotes), ImVec2(0, 60));
                } else {
                    ImGui::Text("Empty repository database queue. Drag folder to Left Panel to populate.");
                }
                ImGui::EndTabItem();
            }

            // MODE 2: TACTILE POLAROID SCATTER (Virtual Shoebox with Draggable Mouse Controls)
            if (ImGui::BeginTabItem("Polaroid Scatter")) {
                ImGui::Text("Tabletop Desk - Drag and organize your collection scatter cards!");
                
                float scatterWidth = middlePanelWidth - 30.0f;
                float scatterHeight = bodyHeight - 120.0f;

                // Initialize card offsets automatically and disperse across ENTIRE space (no clustering)
                if (g_PolaroidCards.empty() && !g_Catalog.empty()) {
                    g_PolaroidCards.clear();
                    for (int i = 0; i < (int)g_Catalog.size(); ++i) {
                        PolaroidCard pc; pc.carIndex = i;
                        float maxX = scatterWidth - 180.0f;
                        float maxY = scatterHeight - 200.0f;
                        pc.position = ImVec2(20.0f + (std::rand() % (int)(std::max)(10.0f, maxX)), 20.0f + (std::rand() % (int)(std::max)(10.0f, maxY)));
                        pc.initialized = true;
                        g_PolaroidCards.push_back(pc);
                    }
                }
                if (ImGui::Button("Toss/Scatter Cards")) {
                    g_PolaroidCards.clear();
                    for (int i = 0; i < (int)g_Catalog.size(); ++i) {
                        PolaroidCard pc; pc.carIndex = i;
                        float maxX = scatterWidth - 180.0f;
                        float maxY = scatterHeight - 200.0f;
                        pc.position = ImVec2(20.0f + (std::rand() % (int)(std::max)(10.0f, maxX)), 20.0f + (std::rand() % (int)(std::max)(10.0f, maxY)));
                        pc.initialized = true;
                        g_PolaroidCards.push_back(pc);
                    }
                }

                ImGui::Separator();
                ImVec2 scatterCursor = ImGui::GetCursorScreenPos();
                ImDrawList* dlist = ImGui::GetWindowDrawList();
                dlist->AddRectFilled(scatterCursor, ImVec2(scatterCursor.x + scatterWidth, scatterCursor.y + scatterHeight), IM_COL32(10, 14, 20, 255));

                ImGui::BeginChild("ScatterStage", ImVec2(0, scatterHeight), false, ImGuiWindowFlags_NoScrollbar);
                
                int hoveredIndex = -1;
                for (size_t i = 0; i < g_PolaroidCards.size(); ++i) {
                    auto& pc = g_PolaroidCards[i]; if (!pc.initialized) continue;
                    ImVec2 cardMin = ImVec2(scatterCursor.x + pc.position.x, scatterCursor.y + pc.position.y);
                    ImVec2 cardMax = ImVec2(cardMin.x + 165.0f, cardMin.y + 185.0f);
                    if (ImGui::IsMouseHoveringRect(cardMin, cardMax)) {
                        hoveredIndex = (int)i;
                    }
                }

                // Click-Lock Drag State Trackers
                if (ImGui::IsMouseClicked(0) && hoveredIndex != -1) {
                    g_DraggedPolaroidIndex = hoveredIndex;
                }
                if (ImGui::IsMouseReleased(0)) {
                    g_DraggedPolaroidIndex = -1;
                }
                if (g_DraggedPolaroidIndex != -1 && ImGui::IsMouseDragging(0)) {
                    ImVec2 delta = ImGui::GetIO().MouseDelta;
                    auto& pc = g_PolaroidCards[g_DraggedPolaroidIndex];
                    pc.position.x += delta.x / ImGui::GetIO().FontGlobalScale;
                    pc.position.y += delta.y / ImGui::GetIO().FontGlobalScale;
                }

                for (size_t i = 0; i < g_PolaroidCards.size(); ++i) {
                    auto& pc = g_PolaroidCards[i]; if (!pc.initialized) continue;
                    ImGui::SetCursorPos(pc.position);
                    ImGui::PushID(static_cast<int>(i));

                    ImGui::BeginChild((std::string("CardWin##") + std::to_string(i)).c_str(), ImVec2(165, 185), true, ImGuiWindowFlags_NoScrollbar);
                    GLuint tex = GetOrCreateTexture(g_Catalog[pc.carIndex].folderPath);
                    if (tex != 0) {
                        ImGui::Image((void*)(intptr_t)tex, ImVec2(145, 80));
                    } else {
                        ImGui::Text("    🚗    ");
                    }
                    
                    ImGui::Text("%d %s", g_Catalog[pc.carIndex].year, g_Catalog[pc.carIndex].make.c_str());
                    std::string shortModel = g_Catalog[pc.carIndex].model;
                    if (shortModel.length() > 14) shortModel = shortModel.substr(0, 12) + "..";
                    ImGui::TextDisabled("%s", shortModel.c_str());
                    ImGui::TextDisabled("Scale: %s", g_Catalog[pc.carIndex].scale.c_str());

                    ImGui::EndChild();
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // MODE 3: FACEOFF CHALLENGE (Tinder-style select side-by-side)
            if (ImGui::BeginTabItem("Faceoff")) {
                if ((g_ShowdownLeftIndex == -1 || g_ShowdownRightIndex == -1) && g_Catalog.size() >= 2) {
                    g_ShowdownLeftIndex = std::rand() % g_Catalog.size(); g_ShowdownRightIndex = std::rand() % g_Catalog.size();
                    while (g_ShowdownLeftIndex == g_ShowdownRightIndex) g_ShowdownRightIndex = std::rand() % g_Catalog.size();
                    g_ActiveQuestionIndex = std::rand() % 5;
                    g_ShowdownActive = true;
                }

                if (g_ShowdownActive && g_ShowdownLeftIndex != -1 && g_ShowdownRightIndex != -1) {
                    // Render Question at top in Bold Blue
                    ImGui::TextColored(ImVec4(0.30f, 0.48f, 1.00f, 1.00f), "Challenge Question: %s", g_FaceoffQuestions[g_ActiveQuestionIndex]);
                    ImGui::Separator();

                    float halfCardWidth = (middlePanelWidth / 2.0f) - 20.0f;
                    float cardHeight = halfCardWidth * (9.0f / 16.0f) + 80.0f;

                    // Tinder Left Card (Click to Select)
                    ImGui::PushID("LCard");
                    ImGui::BeginChild("LeftShowdownCard", ImVec2(halfCardWidth, cardHeight), true);
                    auto& carL = g_Catalog[g_ShowdownLeftIndex];
                    ImGui::Text("%d %s", carL.year, carL.make.c_str()); ImGui::TextWrapped("%s", carL.model.c_str());
                    
                    GLuint texL = GetOrCreateTexture(carL.folderPath);
                    if (texL != 0) {
                        ImGui::Image((void*)(intptr_t)texL, ImVec2(halfCardWidth - 20.0f, (halfCardWidth - 20.0f) * 9.0f / 16.0f));
                    } else {
                        ImGui::Text("   No Photo   ");
                    }
                    
                    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
                        PushHistoryState(); carL.showdownWins++; carL.showdownBattles++;
                        g_Catalog[g_ShowdownRightIndex].showdownBattles++;
                        g_FaceoffFlashTimer = 0.5f; // Trigger flash transition
                        g_ShowdownLeftIndex = -1; g_ShowdownRightIndex = -1; // Next matchups
                        g_UnsavedChanges = true;
                    }
                    ImGui::EndChild();
                    ImGui::PopID();

                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), " ⚔ ");
                    ImGui::SameLine();

                    // Tinder Right Card (Click to Select)
                    ImGui::PushID("RCard");
                    ImGui::BeginChild("RightShowdownCard", ImVec2(halfCardWidth, cardHeight), true);
                    auto& carR = g_Catalog[g_ShowdownRightIndex];
                    ImGui::Text("%d %s", carR.year, carR.make.c_str()); ImGui::TextWrapped("%s", carR.model.c_str());
                    
                    GLuint texR = GetOrCreateTexture(carR.folderPath);
                    if (texR != 0) {
                        ImGui::Image((void*)(intptr_t)texR, ImVec2(halfCardWidth - 20.0f, (halfCardWidth - 20.0f) * 9.0f / 16.0f));
                    } else {
                        ImGui::Text("   No Photo   ");
                    }

                    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
                        PushHistoryState(); carR.showdownWins++; carR.showdownBattles++;
                        g_Catalog[g_ShowdownLeftIndex].showdownBattles++;
                        g_FaceoffFlashTimer = 0.5f; // Trigger flash transition
                        g_ShowdownLeftIndex = -1; g_ShowdownRightIndex = -1; // Next matchups
                        g_UnsavedChanges = true;
                    }
                    ImGui::EndChild();
                    ImGui::PopID();

                    ImGui::Spacing();
                    if (ImGui::Button("End Session", ImVec2(-1, 35))) {
                        g_ShowdownActive = false;
                    }
                } else {
                    // Show Curated Results / Collector Characteristics!
                    ImGui::TextColored(ImVec4(0.30f, 0.48f, 1.00f, 1.00f), "🏆 Curation Analysis:");
                    ImGui::Separator();
                    int totalWins = 0, vintageWins = 0, jdmWins = 0, muscleWins = 0;
                    for (const auto& item : g_Catalog) {
                        if (item.showdownWins > 0) {
                            totalWins += item.showdownWins;
                            if (item.year < 1980) vintageWins += item.showdownWins;
                            std::string m = item.make; std::transform(m.begin(), m.end(), m.begin(), ::tolower);
                            if (m.find("nissan") != std::string::npos || m.find("toyota") != std::string::npos) jdmWins += item.showdownWins;
                            else if (m.find("ford") != std::string::npos || m.find("chev") != std::string::npos) muscleWins += item.showdownWins;
                        }
                    }
                    ImGui::BeginChild("HobbyCuratorResults", ImVec2(0, 160), true);
                    if (totalWins == 0) {
                        ImGui::Text("No matchups completed. Click cards in Faceoff to analyze your profile.");
                    } else {
                        ImGui::Text("You have cast %d showdown votes.", totalWins);
                        if (vintageWins > totalWins / 2) {
                            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.10f, 1.0f), "⭐ Collector Character: [ Vintage Preserver ]");
                            ImGui::TextWrapped("You display a strong leaning toward historic, golden-era automotive engineering and vintage catalog heritage.");
                        } else if (jdmWins > muscleWins) {
                            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "⭐ Collector Character: [ JDM Tuner ]");
                            ImGui::TextWrapped("You prioritize high-tech Japanese performance, drift culture, and modular chassis configurations.");
                        } else {
                            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), "⭐ Collector Character: [ Balanced Enthusiast ]");
                            ImGui::TextWrapped("You appreciate a wide range of vehicles, maintaining a versatile showcase across different eras and styles.");
                        }
                    }
                    ImGui::EndChild();
                    if (ImGui::Button("Restart Face-off Matchup", ImVec2(-1, 35))) {
                        g_ShowdownLeftIndex = -1; g_ShowdownRightIndex = -1;
                    }
                }

                // Draw Flash Transition overlay
                if (g_FaceoffFlashTimer > 0.0f) {
                    g_FaceoffFlashTimer -= ImGui::GetIO().DeltaTime;
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImVec2 minPos = ImGui::GetWindowPos();
                    ImVec2 maxPos = ImVec2(minPos.x + width, minPos.y + height);
                    float alpha = g_FaceoffFlashTimer * 2.0f;
                    if (alpha > 1.0f) alpha = 1.0f;
                    drawList->AddRectFilled(minPos, maxPos, IM_COL32(255, 255, 255, (int)(alpha * 200)));
                }

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();

        // ImGui SameLine spacing binds Middle and Right Panels horizontally
        ImGui::SameLine();

        // -------------------------------------------------------------
        // PANEL 3: RIGHT PANEL — RICH TEXT & CHATBOT (30% Width)
        // -------------------------------------------------------------
        ImGui::BeginChild("RightPanel", ImVec2(0, bodyHeight), true);
        
        // Tab system kept visible by default on startup (With or without photos loaded!)
        if (ImGui::BeginTabBar("RightHistologyTabs")) {
            
            if (ImGui::BeginTabItem("Real History")) {
                if (g_SelectedCarIndex >= 0 && g_SelectedCarIndex < (int)g_Catalog.size()) {
                    ImGui::TextWrapped("%s", g_Catalog[g_SelectedCarIndex].realHistory.c_str());
                } else {
                    ImGui::TextWrapped("Select or upload a model to view real-world history descriptions sourced dynamically.");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Factory Specs")) {
                if (g_SelectedCarIndex >= 0 && g_SelectedCarIndex < (int)g_Catalog.size()) {
                    ImGui::TextWrapped("%s", g_Catalog[g_SelectedCarIndex].factorySpecs.c_str());
                } else {
                    ImGui::TextWrapped("Select or upload a model to view technical factory scale specifications.");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Nuggets")) {
                if (g_SelectedCarIndex >= 0 && g_SelectedCarIndex < (int)g_Catalog.size()) {
                    auto& car = g_Catalog[g_SelectedCarIndex];
                    ImGui::Text("REAL CAR COLLECTABILITY:");
                    ImGui::TextColored(ImVec4(0.30f, 0.48f, 1.00f, 1.00f), "Score: %.2f / 10.00", car.collectabilityScore);
                    ImU32 sunkenTrack = GetThemeSunkenTrackColor(g_ActiveTheme);
                    ImU32 pillFill = GetThemePillFillColor(g_ActiveTheme);
                    DrawNeumorphicProgressBar(car.collectabilityScore / 10.0f, ImVec2(rightPanelWidth - 50.0f, 14.0f), sunkenTrack, pillFill);
                    ImGui::Separator(); ImGui::Text("NOTABLE TRIVIA NUGGETS:");
                    for (size_t n = 0; n < car.triviaNuggets.size(); ++n) {
                        ImGui::BeginChild((std::string("TriviaBoxChild##") + std::to_string(n)).c_str(), ImVec2(0, 85), true);
                        ImGui::TextWrapped("%s", car.triviaNuggets[n].c_str()); ImGui::EndChild();
                    }
                } else {
                    ImGui::TextWrapped("Select or upload a model to generate collectability score progress bars and trivia nuggets.");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Garage Profile")) {
                ImGui::Text("Sourced Personality Archetypes:");
                int jdmCount = 0; int muscleCount = 0;
                for (const auto& item : g_Catalog) {
                    std::string m = item.make; std::transform(m.begin(), m.end(), m.begin(), ::tolower);
                    if (m.find("nissan") != std::string::npos || m.find("toyota") != std::string::npos || m.find("mits") != std::string::npos) jdmCount++;
                    else if (m.find("ford") != std::string::npos || m.find("chev") != std::string::npos || m.find("dodge") != std::string::npos) muscleCount++;
                }
                ImGui::BeginChild("BadgeBox", ImVec2(0, 80), true);
                if (g_Catalog.empty()) {
                    ImGui::TextColored(ImVec4(0.4f, 0.5f, 0.6f, 1.0f), "🏆 Archetype Badge: [ Empty Showroom ]");
                    ImGui::TextWrapped("Your collection profile is currently empty. Drop file folders to analyze your curator badge.");
                } else if (jdmCount > muscleCount) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "🏆 Archetype Badge: [ JDM Specialist ]");
                    ImGui::TextWrapped("Your collection displays high fidelity affinity for Japanese domestic engineering and street-tuner design.");
                } else if (muscleCount > jdmCount) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "🏆 Archetype Badge: [ Muscle Classicist ]");
                    ImGui::TextWrapped("Your collection displays high fidelity affinity for standard American high-displacement V8 drag-strip pedigree.");
                } else {
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), "🏆 Archetype Badge: [ Balanced Curator ]");
                    ImGui::TextWrapped("Your collection displays highly versatile focus across multiple mechanical layouts and classes.");
                }
                ImGui::EndChild();
                ImGui::Separator(); ImGui::Text("Showdown Bracket Leaderboard (Top Wins):");
                std::vector<DiecastCar> sortedCatalog = g_Catalog;
                std::sort(sortedCatalog.begin(), sortedCatalog.end(), [](const DiecastCar& a, const DiecastCar& b) {
                    return a.showdownWins > b.showdownWins;
                });
                for (size_t l = 0; l < (std::min)(sortedCatalog.size(), size_t(5)); ++l) {
                    if (sortedCatalog[l].showdownWins > 0) {
                        ImGui::Text("%zu. %s %s - %d Wins", l+1, sortedCatalog[l].make.c_str(), sortedCatalog[l].model.c_str(), sortedCatalog[l].showdownWins);
                    }
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        if (g_SelectedCarIndex >= 0 && g_SelectedCarIndex < (int)g_Catalog.size()) {
            auto& car = g_Catalog[g_SelectedCarIndex];
            ImGui::Separator();
            ImGui::Text("Collector Chat Memory Log (💬):");
            ImGui::BeginChild("QAChatLogs", ImVec2(0, 120), true);
            for (const auto& chat : g_ChatLog) {
                ImGui::TextColored(chat.first == "You" ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f) : ImVec4(0.0f, 0.6f, 0.9f, 1.0f), "%s: ", chat.first.c_str());
                ImGui::SameLine(); ImGui::TextWrapped("%s", chat.second.c_str());
            }
            ImGui::EndChild();
            ImGui::Separator(); ImGui::Text("CURATOR REMARKS"); ImGui::TextWrapped("Folder path: %s", car.folderPath.c_str());
        }
        ImGui::EndChild();

        // Modals
        if (g_ShowUploadConfirmPrompt) ImGui::OpenPopup("Confirm Import Queue?");
        if (ImGui::BeginPopupModal("Confirm Import Queue?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Confirm upload queue? You are about to import %zu file(s) into your current workspace catalog.", g_PendingImportPaths.size());
            ImGui::Spacing();
            if (ImGui::Button("Confirm Upload", ImVec2(140, 0))) {
                finalizePendingImports(); g_ShowUploadConfirmPrompt = false; ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                g_PendingImportPaths.clear(); g_ShowUploadConfirmPrompt = false; ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (g_ShowExitPrompt) ImGui::OpenPopup("Unsaved Workspace Changes!");
        if (ImGui::BeginPopupModal("Unsaved Workspace Changes!", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("You have unsaved changes in your metadata database.\nWould you like to save changes before exiting the application?");
            ImGui::Spacing();
            if (ImGui::Button("Save and Exit", ImVec2(140, 0))) {
                exportWorkspace("workspace.zip"); glfwSetWindowShouldClose(window, GLFW_TRUE); ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard and Exit", ImVec2(160, 0))) {
                g_UnsavedChanges = false; glfwSetWindowShouldClose(window, GLFW_TRUE); ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                g_ShowExitPrompt = false; ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, width, height);
        if (g_ActiveTheme == THEME_LIGHT) glClearColor(0.88f, 0.90f, 0.93f, 1.00f);
        else if (g_ActiveTheme == THEME_NAVY) glClearColor(0.04f, 0.06f, 0.10f, 1.00f);
        else if (g_ActiveTheme == THEME_BEIGE) glClearColor(0.94f, 0.92f, 0.89f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    if (g_ActiveTextureID != 0) glDeleteTextures(1, &g_ActiveTextureID);
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glfwDestroyWindow(window); glfwTerminate();
    return 0;
}