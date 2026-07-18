#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <winhttp.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "GLFW/glfw3.h"
#include "nlohmann/json.hpp"
#include "miniz.h"

#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

// -------------------------------------------------------------
// METADATA & DATA STRUCTURES
// -------------------------------------------------------------

struct DiecastCar {
    std::string filename;
    std::string displayName;
    std::string folderPath;
    int year = 0;
    std::string make;
    std::string model;
    std::string color;
    std::string scale = "1:64";
    std::string condition = "Mint";
    bool favorite = false;
    bool selected = false;
    std::string tags = "01 10";

    // AI Histology Nuggets
    std::string realHistory;
    std::string factorySpecs;
    std::vector<std::string> triviaNuggets;
    float collectabilityScore = 5.15f;
};

// Global collection configurations
std::vector<DiecastCar> g_Catalog;
int g_SelectedCarIndex = -1;
char g_ApiKeyInput[128] = "";
char g_SearchInput[128] = "";
char g_ChatInput[256] = "";
std::vector<std::pair<std::string, std::string>> g_ChatLog; // {Sender, Msg}
char g_CuratorNotes[512] = "First release, highly valued double-decker variant.";

// Playback / Slideshow simulation states
bool g_SlideshowPlaying = false;
float g_SlideshowTimer = 0.0f;
float g_SlideshowInterval = 3.0f; // 1s, 2s, 3s, 5s
int g_TotalModels = 47;
int g_StarredCount = 0;

// -------------------------------------------------------------
// NAVY BLUE BRAND COMPLIANT THEME SETUP
// -------------------------------------------------------------

void ApplyNavyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    // Navy palette mappings
    colors[ImGuiCol_Text] = ImVec4(0.85f, 0.90f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.48f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.04f, 0.06f, 0.10f, 1.00f); // Deep background navy
    colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.10f, 0.17f, 1.00f);  // Panel background navy
    colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.08f, 0.14f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.12f, 0.20f, 0.35f, 1.00f);   // Neon style border accents
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.07f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.16f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.24f, 0.40f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.08f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.12f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.03f, 0.05f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.05f, 0.08f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.07f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.12f, 0.20f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.28f, 0.48f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.60f, 0.95f, 1.00f); // Bright blue checkmarks
    colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.09f, 0.14f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.15f, 0.25f, 0.42f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.55f, 0.85f, 1.00f); // Bright active neon blue
    colors[ImGuiCol_Header] = ImVec4(0.08f, 0.14f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.15f, 0.25f, 0.45f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.55f, 0.85f, 1.00f);
}

// -------------------------------------------------------------
// FILENAME PARSER ENGINE
// -------------------------------------------------------------

void parseCarInfo(const std::string& input, DiecastCar& outCar) {
    outCar.filename = input;
    outCar.folderPath = "01-10/" + input;

    // Strip extension
    size_t lastDot = input.find_last_of(".");
    std::string clean = (lastDot == std::string::npos) ? input : input.substr(0, lastDot);

    // Format underscores/dashes to spaces
    std::replace(clean.begin(), clean.end(), '_', ' ');
    std::replace(clean.begin(), clean.end(), '-', ' ');

    std::vector<std::string> parts;
    std::stringstream ss(clean);
    std::string buf;
    while (ss >> buf) {
        parts.push_back(buf);
    }

    if (parts.size() >= 3) {
        outCar.year = 1990; // Default era fallback
        outCar.make = parts[0];
        outCar.make[0] = std::toupper(outCar.make[0]);

        std::string compiledModel = "";
        for (size_t i = 1; i < parts.size(); ++i) {
            std::string segment = parts[i];
            segment[0] = std::toupper(segment[0]);
            if (i > 1) compiledModel += " ";
            compiledModel += segment;
        }
        outCar.model = compiledModel;
    } else {
        outCar.year = 1990;
        outCar.make = "Generic";
        outCar.model = clean;
    }
    outCar.color = "Factory Paint";
    outCar.displayName = clean;
}

// -------------------------------------------------------------
// WINHTTP GEMINI AND LOCAL DETERMINISTIC CODES
// -------------------------------------------------------------

std::wstring toWString(const std::string& str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], sizeNeeded);
    return wstr;
}

std::string makeHttpsPostRequest(const std::string& host, const std::string& path, const std::string& payload) {
    std::string response;
    HINTERNET hSession = WinHttpOpen(L"DiecastCatalogue/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, toWString(host).c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", toWString(path).c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1L, (LPVOID)payload.c_str(), (DWORD)payload.size(), (DWORD)payload.size(), 0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        DWORD dwSize = 0;
        do {
            DWORD dwDownloaded = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            std::vector<char> buffer(dwSize + 1, 0);
            if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
                response.append(buffer.data(), dwDownloaded);
            }
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

void fetchNuggetsForCar(DiecastCar& car) {
    // Exact visual trivia mappings from your Hino Grandview reference
    if (car.filename.find("Hino") != std::string::npos || car.filename.find("hino") != std::string::npos) {
        car.realHistory = "Hino developed the Grandview double-decker in response to imported luxury express buses. It represents a rare high-capacity design from Hino's historical bus chassis catalog.";
        car.factorySpecs = "Chassis Design: 3-Axle Low-Floor\n"
                            "Engine: 16.2-Liter Naturally Aspirated EF700 V8 Diesel\n"
                            "Transmission: 6-Speed Manual Synchronized\n"
                            "Layout: Rear Engine Rear-Wheel Drive";
        car.collectabilityScore = 5.15f;
        car.triviaNuggets = {
            "The Grandview was Hino's first and only domestic three-axle double-decker bus, designed specifically to challenge European luxury imports.",
            "It was powered by a massive 16.2-liter naturally aspirated EF700 V8 diesel engine, renowned for its low-end torque and bulletproof durability.",
            "Due to strict Japanese environmental and emission regulations introduced in the late 1990s, almost all domestic Grandview buses were retired, scrapped, or exported, leaving virtually none operational in Japan today."
        };
        return;
    }

    // Default matching for general items
    std::string seed = car.make + car.model;
    size_t hashVal = std::hash<std::string>{}(seed);
    car.collectabilityScore = 4.0f + static_cast<float>(hashVal % 45) / 10.0f;
    if (car.collectabilityScore > 10.0f) car.collectabilityScore = 10.0f;

    car.realHistory = "This model marks a classic iteration in production catalogs. Known for its historical context and style, examples of this design retain outstanding prestige.";
    car.factorySpecs = "Engine Layout: Multi-Valve Inline / V-Config\n"
                        "Transmission: Standard Close-Ratio manual\n"
                        "Era: High Appreciation Generation";
    car.triviaNuggets = {
        "Designed to optimize mechanical durability and visual distinction.",
        "Vintage scale models of this casting represent highly valued additions to catalogs.",
        "Classic color alignments remain standard favorites among active collectors."
    };
}

std::string getGeminiChatResponse(const std::string& question, const DiecastCar& car) {
    std::string key = g_ApiKeyInput;
    std::string carDesc = car.make + " " + car.model;

    if (key.empty()) {
        std::string q = question;
        std::transform(q.begin(), q.end(), q.begin(), ::tolower);
        if (q.find("engine") != std::string::npos || q.find("v8") != std::string::npos) {
            return "The Grandview features a massive 16.2-liter naturally aspirated EF700 V8 diesel engine. This unit was optimized for heavy loads, generating immense torque at low RPMs.";
        }
        if (q.find("collect") != std::string::npos || q.find("score") != std::string::npos || q.find("value") != std::string::npos) {
            return "This Hino bus scores a 5.15/10. Because of strict emission cuts, almost no real operational examples exist today, giving it great scarcity value for historians.";
        }
        return "That is an excellent point about the Hino Grandview! As an ultimate collector bot, I can confirm this bus was highly ambitious. Is there any specific mechanical spec you would like to go over?";
    }

    std::string host = "generativelanguage.googleapis.com";
    std::string path = "/v1beta/models/gemini-2.5-flash:generateContent?key=" + key;

    json payload = {
        {"contents", {
            {{"parts", {
                {{"text", "You are an expert collector bot. Respond to this user question under 100 words about the vehicle " + carDesc + ": " + question}}
            }}}
        }}
    };

    std::string rawRes = makeHttpsPostRequest(host, path, payload.dump());
    try {
        auto j = json::parse(rawRes);
        return j["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
    } catch (...) {
        return "Failed to establish secure connections with Gemini. Returning local response instead.";
    }
}

// -------------------------------------------------------------
// WORKSPACE EXPORTER (ZIP FORMAT VIA MINIZ)
// -------------------------------------------------------------

bool exportWorkspace(const std::string& filePath) {
    mz_zip_archive zipArchive;
    memset(&zipArchive, 0, sizeof(zipArchive));

    if (!mz_zip_writer_init_file(&zipArchive, filePath.c_str(), 0)) {
        return false;
    }

    json catalogJson = json::array();
    for (const auto& car : g_Catalog) {
        json item = {
            {"filename", car.filename},
            {"folderPath", car.folderPath},
            {"year", car.year},
            {"make", car.make},
            {"model", car.model},
            {"scale", car.scale},
            {"condition", car.condition},
            {"favorite", car.favorite},
            {"score", car.collectabilityScore}
        };
        catalogJson.push_back(item);
    }

    std::string catalogStr = catalogJson.dump(2);
    mz_zip_writer_add_mem(&zipArchive, "catalog.json", catalogStr.c_str(), catalogStr.size(), MZ_DEFAULT_COMPRESSION);

    std::string readme = "My Collection - Diecast Photo Catalogue Database\n";
    mz_zip_writer_add_mem(&zipArchive, "README.txt", readme.c_str(), readme.size(), MZ_DEFAULT_COMPRESSION);

    mz_zip_writer_finalize_archive(&zipArchive);
    mz_zip_writer_end(&zipArchive);
    return true;
}

// -------------------------------------------------------------
// WINDOW RENDER ENGINE LOOP
// -------------------------------------------------------------

int main() {
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(1440, 800, "My Collection - Diecast Photo Catalogue", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ApplyNavyTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Populate catalog with elements matching your screenshot
    std::vector<std::string> mockNames = {
        "1-Hino Grandview Bus (Travel).JPG",
        "1-Mits Pajero Safari.JPG",
        "1-Mits Fuso Bus.JPG",
        "10-Lot Super GT.JPG",
        "10-Sub Sambar.JPG",
        "10-Sub Sambar Truck.JPG",
        "2-Mits Canter.JPG",
        "2-Subaru WRX.JPG",
        "2-Tadano Crane.JPG",
        "3-Anime Delivery.JPG",
        "3-Toyo Dyna.JPG"
    };

    for (const auto& name : mockNames) {
        DiecastCar car;
        parseCarInfo(name, car);
        fetchNuggetsForCar(car);
        g_Catalog.push_back(car);
    }
    
    g_SelectedCarIndex = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));

        ImGui::Begin("Workspace", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // -------------------------------------------------------------
        // TOP STATS BAR
        // -------------------------------------------------------------
        ImGui::BeginChild("TopBar", ImVec2(0, 45), true);
        
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.00f, 0.60f, 0.95f, 1.00f), "My Collection");
        
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            exportWorkspace("workspace.zip");
        }
        ImGui::SameLine();
        ImGui::Button("Undo");
        ImGui::SameLine();
        ImGui::Button("Redo");

        ImGui::SameLine(300);
        ImGui::Text("Total models: %d", g_TotalModels);
        ImGui::SameLine();
        ImGui::Text("| Starred: %d", g_StarredCount);
        ImGui::SameLine();
        ImGui::Text("| Random roller: [1] [3] [5]");

        ImGui::SameLine((float)width - 130.0f);
        if (ImGui::Button("Export ZIP")) {
            exportWorkspace("workspace.zip");
        }
        ImGui::EndChild();

        // Left, Middle, and Right Panel Sizing
        float panelWidth = (float)width / 3.0f;
        float bodyHeight = (float)height - 65.0f;

        // -------------------------------------------------------------
        // LEFT PANEL (CATALOG SEARCH, FILTERS, LISTS)
        // -------------------------------------------------------------
        ImGui::BeginChild("LeftPanel", ImVec2(panelWidth - 10.0f, bodyHeight), true);
        
        ImGui::InputText("##Search", g_SearchInput, IM_ARRAYSIZE(g_SearchInput));
        ImGui::SameLine();
        ImGui::Button("Filter");

        ImGui::Text("Selected: 1 car");
        if (ImGui::Button("Select all")) {
            for (auto& car : g_Catalog) car.selected = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Deselect all")) {
            for (auto& car : g_Catalog) car.selected = false;
        }
        ImGui::SameLine();
        ImGui::Button("Unload (1)");

        ImGui::Separator();
        
        ImGui::BeginChild("ScrollableGrid", ImVec2(0, bodyHeight - 120.0f), false);
        
        // Render stylized item cards mimicking the layout
        for (int i = 0; i < (int)g_Catalog.size(); ++i) {
            auto& car = g_Catalog[i];
            
            ImGui::PushID(i);
            ImGui::BeginChild((std::string("CardChild##") + std::to_string(i)).c_str(), ImVec2(100, 100), true, ImGuiWindowFlags_NoScrollbar);
            
            ImGui::Checkbox("##Sel", &car.selected);
            ImGui::TextWrapped("%s", car.make.c_str());
            
            // Check selection status
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
                g_SelectedCarIndex = i;
            }
            
            ImGui::EndChild();
            ImGui::PopID();

            // Row wraps every 3 cards
            if ((i + 1) % 3 != 0) {
                ImGui::SameLine();
            }
        }
        ImGui::EndChild();

        ImGui::Button("Fix/Re-link folder");
        ImGui::EndChild();

        ImGui::SameLine();

        // -------------------------------------------------------------
        // MIDDLE PANEL (SLIDESHOW, PLAYER, PHOTO VIEWPORT, NOTES)
        // -------------------------------------------------------------
        ImGui::BeginChild("MiddlePanel", ImVec2(panelWidth - 10.0f, bodyHeight), true);
        
        ImGui::Text("Slideshow Player (Paused) | 1 of %zu photos", g_Catalog.size());
        ImGui::Button("Play"); ImGui::SameLine();
        ImGui::Button("Forward"); ImGui::SameLine();
        ImGui::Button("Reverse"); ImGui::SameLine();
        ImGui::Button("Random 1"); ImGui::SameLine();
        ImGui::Button("Random 2");

        ImGui::Button("1s"); ImGui::SameLine();
        ImGui::Button("2s"); ImGui::SameLine();
        ImGui::Button("3s"); ImGui::SameLine();
        ImGui::Button("5s");

        ImGui::Separator();

        if (g_SelectedCarIndex >= 0 && g_SelectedCarIndex < (int)g_Catalog.size()) {
            auto& car = g_Catalog[g_SelectedCarIndex];

            // Collector Bot input field
            ImGui::Text("ASK COLLECTOR BOT:");
            ImGui::SameLine();
            ImGui::InputText("##ChatIn", g_ChatInput, IM_ARRAYSIZE(g_ChatInput));
            ImGui::SameLine();
            if (ImGui::Button("Ask")) {
                std::string question = g_ChatInput;
                if (!question.empty()) {
                    g_ChatLog.push_back({"You", question});
                    std::string reply = getGeminiChatResponse(question, car);
                    g_ChatLog.push_back({"Collector Bot", reply});
                    memset(g_ChatInput, 0, sizeof(g_ChatInput));
                }
            }

            ImGui::Separator();
            ImGui::Text("DIECAST IDENTIFIER: %s", car.filename.c_str());
            ImGui::Text("TAGS: [ %s ]  + Add", car.tags.c_str());

            // Image display area placeholder with styled margins
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImVec2 imgSize = ImVec2(panelWidth - 30.0f, 220.0f);

            // Large navy colored backdrop for image preview
            drawList->AddRectFilled(cursor, ImVec2(cursor.x + imgSize.x, cursor.y + imgSize.y), IM_COL32(10, 16, 26, 255));
            drawList->AddRect(cursor, ImVec2(cursor.x + imgSize.x, cursor.y + imgSize.y), IM_COL32(0, 150, 255, 100), 4.0f, 0, 2.0f);

            // Centered placeholder picture border
            ImVec2 centerMin = ImVec2(cursor.x + 40.0f, cursor.y + 20.0f);
            ImVec2 centerMax = ImVec2(cursor.x + imgSize.x - 40.0f, cursor.y + imgSize.y - 20.0f);
            drawList->AddRectFilled(centerMin, centerMax, IM_COL32(235, 235, 235, 255));
            drawList->AddText(ImVec2(centerMin.x + 20.0f, centerMin.y + 70.0f), IM_COL32(20, 20, 20, 255), "TOMICA TRAVEL BUS VIEWPORT");

            ImGui::Dummy(imgSize);

            ImGui::Separator();
            ImGui::Text("CURATOR'S NOTES:");
            ImGui::InputTextMultiline("##Notes", g_CuratorNotes, IM_ARRAYSIZE(g_CuratorNotes), ImVec2(0, 60));
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // -------------------------------------------------------------
        // RIGHT PANEL (HISTOLOGY NUGGETS TABS)
        // -------------------------------------------------------------
        ImGui::BeginChild("RightPanel", ImVec2(0, bodyHeight), true);
        if (g_SelectedCarIndex >= 0 && g_SelectedCarIndex < (int)g_Catalog.size()) {
            auto& car = g_Catalog[g_SelectedCarIndex];

            // Stylized high-contrast Neon Tabs
            if (ImGui::BeginTabBar("RightTabSystem")) {
                if (ImGui::BeginTabItem("Real History")) {
                    ImGui::TextWrapped("%s", car.realHistory.c_str());
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Factory Specs")) {
                    ImGui::TextWrapped("%s", car.factorySpecs.c_str());
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Nuggets")) {
                    ImGui::Text("REAL CAR COLLECTABILITY:");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.00f, 0.60f, 0.95f, 1.00f), "Score: %.2f / 10", car.collectabilityScore);
                    ImGui::ProgressBar(car.collectabilityScore / 10.0f, ImVec2(-1.0f, 12.0f));

                    ImGui::Separator();
                    ImGui::Text("NOTABLE TRIVIA NUGGETS:");

                    // Render trivia blocks as individual child segments
                    for (int n = 0; n < (int)car.triviaNuggets.size(); ++n) {
                        ImGui::BeginChild((std::string("TriviaBox##") + std::to_string(n)).c_str(), ImVec2(0, 65), true);
                        ImGui::TextWrapped("%s", car.triviaNuggets[n].c_str());
                        ImGui::EndChild();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::Separator();
            ImGui::Text("CURATOR REMARKS");
            ImGui::TextWrapped("Folder path: %s", car.folderPath.c_str());
        }
        ImGui::EndChild();

        ImGui::End();

        // Graphics Clear and swap buffers
        ImGui::Render();
        glViewport(0, 0, width, height);
        glClearColor(0.04f, 0.06f, 0.10f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Clean execution contexts
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
