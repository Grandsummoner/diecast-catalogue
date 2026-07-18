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
// STRUCTS & DATA ENGINE
// -------------------------------------------------------------

struct DiecastCar {
    std::string filename;
    int year = 0;
    std::string make;
    std::string model;
    std::string color;
    std::string scale = "1:18";
    std::string condition = "Mint";
    bool favorite = false;

    // AI Histology Nuggets
    std::string history;
    std::vector<std::pair<std::string, std::string>> specs;
    std::vector<std::string> facts;
    float collectabilityScore = 5.0f;
};

// Global application states
std::vector<DiecastCar> g_Catalog;
int g_SelectedCarIndex = -1;
char g_ApiKeyInput[128] = "";
char g_FilenameInput[256] = "1970_ford_mustang_mach_1_orange.jpg";
char g_ChatInput[256] = "";
std::vector<std::pair<std::string, std::string>> g_ChatLog; // {Sender, Message}

// Collage Stage visual parameters
enum CollageLayout { LAYOUT_GRID, LAYOUT_STRIP, LAYOUT_POLAROID, LAYOUT_STACKED };
int g_ActiveLayout = LAYOUT_GRID;
enum CollageTheme { THEME_LEATHER, THEME_CARBON, THEME_WOOD, THEME_SPACE };
int g_ActiveTheme = THEME_LEATHER;
float g_ItemSpacing = 16.0f;
float g_BorderSize = 2.0f;
ImVec4 g_CustomBgColor = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);

// -------------------------------------------------------------
// FILENAME AUTO-PARSING LOGIC
// -------------------------------------------------------------

void parseUploadedFilename(const std::string& input, DiecastCar& outCar) {
    outCar.filename = input;
    
    // Strip file extension safely
    size_t lastDot = input.find_last_of(".");
    std::string cleanName = (lastDot == std::string::npos) ? input : input.substr(0, lastDot);

    // Convert underscore/dash symbols to spaces
    std::replace(cleanName.begin(), cleanName.end(), '_', ' ');
    std::replace(cleanName.begin(), cleanName.end(), '-', ' ');

    std::vector<std::string> segments;
    std::stringstream ss(cleanName);
    std::string buffer;
    while (ss >> buffer) {
        segments.push_back(buffer);
    }

    if (segments.size() >= 4) {
        try {
            int parsedYear = std::stoi(segments[0]);
            if (parsedYear >= 1800 && parsedYear <= 2100) {
                outCar.year = parsedYear;
                
                // Segment 1 mapped to Make
                outCar.make = segments[1];
                outCar.make[0] = std::toupper(outCar.make[0]);

                // Segment Last mapped to Color
                outCar.color = segments.back();
                outCar.color[0] = std::toupper(outCar.color[0]);

                // Everything remaining compiled as Model
                std::string compiledModel = "";
                for (size_t i = 2; i < segments.size() - 1; ++i) {
                    std::string segment = segments[i];
                    segment[0] = std::toupper(segment[0]);
                    if (i > 2) compiledModel += " ";
                    compiledModel += segment;
                }
                outCar.model = compiledModel;
                return;
            }
        } catch (...) {}
    }

    // Default Fallback mapping structures
    outCar.year = 1990;
    outCar.make = "Classic";
    outCar.model = cleanName;
    outCar.color = "Red";
}

// -------------------------------------------------------------
// NATIVE WINDOWS HTTPS WINHTTP CLIENT & GEMINI CALLS
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

// -------------------------------------------------------------
// HISTOLOGY & CHAT FALLBACK ENGINES
// -------------------------------------------------------------

void generateFallbackNuggets(DiecastCar& car) {
    // Generate deterministic score from the identifier strings
    std::string seed = car.make + car.model + std::to_string(car.year);
    size_t hashValue = std::hash<std::string>{}(seed);
    car.collectabilityScore = 6.0f + static_cast<float>(hashValue % 36) / 10.0f;
    if (car.collectabilityScore > 10.0f) car.collectabilityScore = 10.0f;

    std::string modelLower = car.model;
    std::transform(modelLower.begin(), modelLower.end(), modelLower.begin(), ::tolower);

    if (modelLower.find("mustang") != std::string::npos) {
        car.history = "The Ford Mustang, introduced in mid-1964, established the pony car class of American sports performance. Combining an accessible, sporty long-hood layout with classic V8 propulsion, it became an iconic representation of classic highway culture.";
        car.specs = {
            {"Engine Layout", "Naturally Aspirated V8 / 4.7L Windsor"},
            {"Horsepower", "271 hp - 375 hp"},
            {"Top Speed", "135 mph"},
            {"0-60 mph", "5.8 seconds"}
        };
        car.facts = {
            "Over 22,000 units were bought on its official launch day.",
            "Named in tribute to the WWII P-51 Mustang fighter plane.",
            "Pioneered massive customized parts catalogs directly from assembly."
        };
    } else if (modelLower.find("911") != std::string::npos) {
        car.history = "The Porsche 911 debuted in 1963, representing the peak of rear-engine German engineering. Retaining its continuous aerodynamic silhouette and flat-six layout for over six decades, it holds unparalleled status in high-precision racing history.";
        car.specs = {
            {"Engine Layout", "Air-Cooled Flat-Six / 2.0L"},
            {"Horsepower", "130 hp - 320 hp"},
            {"Top Speed", "155 mph"},
            {"0-60 mph", "4.9 seconds"}
        };
        car.facts = {
            "Originally named the 901, but adjusted because Peugeot held numeric middle-zero claims.",
            "Over 70% of all 911 designs constructed remain drivable today.",
            "Renowned for its unique rear weight bias and handling traction."
        };
    } else {
        car.history = "The " + std::to_string(car.year) + " " + car.make + " " + car.model + " represents a highly notable chapter in physical automotive progression. Characterized by its signature styling profile, this creation continues to command premium placement among historical archives.";
        car.specs = {
            {"Engine Config", "High-Compression Straight-6"},
            {"Estimated Power", "180 hp - 320 hp"},
            {"Top Speed", "120 mph"},
            {"0-60 mph", "7.1 seconds"}
        };
        car.facts = {
            "Tuned structurally to yield high performance stability under high-speed touring.",
            "Original matching-numbers configurations are extremely sought after by collectors.",
            "Classic paint variations remain highly prized for their historical accurate representation."
        };
    }
}

void triggerNuggetsApi(DiecastCar& car) {
    std::string key = g_ApiKeyInput;
    if (key.empty()) {
        generateFallbackNuggets(car);
        return;
    }

    std::string host = "generativelanguage.googleapis.com";
    std::string path = "/v1beta/models/gemini-2.5-flash:generateContent?key=" + key;

    json payload = {
        {"contents", {
            {{"parts", {
                {{"text", "You are an automotive historian. Output exactly four fields in raw JSON matching this format: "
                          "{\"history\": \"Paragraph describing history\", \"specs\": [{\"label\":\"Engine\",\"value\":\"V8\"}], \"facts\": [\"Fact1\",\"Fact2\"], \"score\": 8.7} "
                          "for the car: " + std::to_string(car.year) + " " + car.make + " " + car.model}}
            }}}
        }}
    };

    std::string rawRes = makeHttpsPostRequest(host, path, payload.dump());
    
    try {
        auto j = json::parse(rawRes);
        std::string rawText = j["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        
        // Sanitize any markdown markdown wrapping if returned
        if (rawText.find("```") != std::string::npos) {
            size_t start = rawText.find("{");
            size_t end = rawText.find_last_of("}");
            if (start != std::string::npos && end != std::string::npos) {
                rawText = rawText.substr(start, end - start + 1);
            }
        }

        auto data = json::parse(rawText);
        car.history = data.value("history", "");
        car.collectabilityScore = data.value("score", 7.0f);
        
        car.specs.clear();
        if (data.contains("specs")) {
            for (auto& item : data["specs"]) {
                car.specs.push_back({item.value("label", ""), item.value("value", "")});
            }
        }
        
        car.facts.clear();
        if (data.contains("facts")) {
            for (auto& item : data["facts"]) {
                car.facts.push_back(item.get<std::string>());
            }
        }
    } catch (...) {
        // Safe fallback in case of JSON formatting mismatches or network blocks
        generateFallbackNuggets(car);
    }
}

std::string getGeminiChatResponse(const std::string& question, const DiecastCar& car) {
    std::string key = g_ApiKeyInput;
    std::string carDesc = std::to_string(car.year) + " " + car.make + " " + car.model;

    if (key.empty()) {
        std::string q = question;
        std::transform(q.begin(), q.end(), q.begin(), ::tolower);
        if (q.find("engine") != std::string::npos || q.find("hp") != std::string::npos || q.find("fast") != std::string::npos) {
            return "The real-world " + carDesc + " is famous for its mechanical footprint. Standard configurations delivered robust performance and immediate driver engagement. Is there a specific trim profile you want to explore?";
        }
        if (q.find("value") != std::string::npos || q.find("worth") != std::string::npos) {
            return "Pristine physical examples of the " + carDesc + " command excellent appreciation. For your scaled model representation, value is determined primarily by manufacturer casting rarity and package condition.";
        }
        return "That is a great perspective on the " + carDesc + "! Historically, this layout made a solid impact in its design segment. Let me know if you would like to go over engine specs or facts!";
    }

    std::string host = "generativelanguage.googleapis.com";
    std::string path = "/v1beta/models/gemini-2.5-flash:generateContent?key=" + key;

    json payload = {
        {"contents", {
            {{"parts", {
                {{"text", "You are an expert automotive historian conversing with a collector who owns a diecast representation of a " + carDesc + ". Respond to this question in under 120 words: " + question}}
            }}}
        }}
    };

    std::string rawRes = makeHttpsPostRequest(host, path, payload.dump());
    try {
        auto j = json::parse(rawRes);
        return j["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
    } catch (...) {
        return "Failed to parse API connection. Returning fallback state offline.";
    }
}

// -------------------------------------------------------------
// WORKSPACE EXPORTER (ZIP FORMATTING VIA MINIZ)
// -------------------------------------------------------------

bool exportWorkspace(const std::string& filePath) {
    mz_zip_archive zipArchive;
    memset(&zipArchive, 0, sizeof(zipArchive));

    if (!mz_zip_writer_init_file(&zipArchive, filePath.c_str(), 0)) {
        return false;
    }

    // Convert catalogue vector structure into structured json
    json catalogJson = json::array();
    for (const auto& car : g_Catalog) {
        json item = {
            {"filename", car.filename},
            {"year", car.year},
            {"make", car.make},
            {"model", car.model},
            {"color", car.color},
            {"scale", car.scale},
            {"condition", car.condition},
            {"favorite", car.favorite},
            {"history", car.history},
            {"score", car.collectabilityScore}
        };
        catalogJson.push_back(item);
    }

    std::string catalogStr = catalogJson.dump(2);
    if (!mz_zip_writer_add_mem(&zipArchive, "catalog.json", catalogStr.c_str(), catalogStr.size(), MZ_DEFAULT_COMPRESSION)) {
        mz_zip_writer_end(&zipArchive);
        return false;
    }

    std::string readme = "Diecast Photo Catalogue - Local Workspace Export\n"
                         "Created on native C++ compiler structures.\n";
    mz_zip_writer_add_mem(&zipArchive, "README.txt", readme.c_str(), readme.size(), MZ_DEFAULT_COMPRESSION);

    mz_zip_writer_finalize_archive(&zipArchive);
    mz_zip_writer_end(&zipArchive);
    return true;
}

// -------------------------------------------------------------
// WINDOW ENTRY POINT & RENDERING ENGINE
// -------------------------------------------------------------

int main() {
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Diecast Photo Catalogue (Native C++)", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable v-sync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Add default mock items to collection
    DiecastCar car1, car2;
    parseUploadedFilename("1970_ford_mustang_mach_1_orange.jpg", car1);
    triggerNuggetsApi(car1);
    g_Catalog.push_back(car1);

    parseUploadedFilename("1995_porsche_911_gt2_silver.png", car2);
    triggerNuggetsApi(car2);
    g_Catalog.push_back(car2);

    g_SelectedCarIndex = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Standard Full-Screen Layout Workspace
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));

        ImGui::Begin("Workspace Canvas", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar);

        // Menubar configuration
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Export Workspace (.zip)")) {
                    exportWorkspace("workspace.zip");
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Draw structural 3-panel split
        float panelWidth = (float)width / 3.0f;

        // -------------------------------------------------------------
        // PANEL 1: CATALOG ENTRY & FILTER INDEX
        // -------------------------------------------------------------
        ImGui::BeginChild("LeftPanel", ImVec2(panelWidth - 10.0f, 0), true);
        ImGui::Text("Diecast Catalogue Panel");
        ImGui::Separator();

        ImGui::InputText("API Key", g_ApiKeyInput, IM_ARRAYSIZE(g_ApiKeyInput), ImGuiInputTextFlags_Password);
        ImGui::Separator();

        ImGui::Text("Auto-Parse File Ingestion:");
        ImGui::InputText("Filename", g_FilenameInput, IM_ARRAYSIZE(g_FilenameInput));
        if (ImGui::Button("Add and Parse Entry")) {
            DiecastCar newCar;
            parseUploadedFilename(g_FilenameInput, newCar);
            triggerNuggetsApi(newCar);
            g_Catalog.push_back(newCar);
            g_SelectedCarIndex = (int)g_Catalog.size() - 1;
        }

        ImGui::Separator();
        ImGui::Text("Collection Database:");
        for (int i = 0; i < (int)g_Catalog.size(); ++i) {
            std::string label = g_Catalog[i].make + " " + g_Catalog[i].model + " (" + std::to_string(g_Catalog[i].year) + ")";
            if (ImGui::Selectable(label.c_str(), g_SelectedCarIndex == i)) {
                g_SelectedCarIndex = i;
                g_ChatLog.clear();
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // -------------------------------------------------------------
        // PANEL 2: MIDDLE VISUAL STAGE & COLLAGE STAGE
        // -------------------------------------------------------------
        ImGui::BeginChild("MiddlePanel", ImVec2(panelWidth - 10.0f, 0), true);
        if (ImGui::BeginTabBar("MiddleTabs")) {
            if (ImGui::BeginTabItem("Catalog Cards")) {
                for (int i = 0; i < (int)g_Catalog.size(); ++i) {
                    auto& car = g_Catalog[i];
                    ImGui::BeginChild((std::string("Card##") + std::to_string(i)).c_str(), ImVec2(0, 100), true);
                    ImGui::Text("%d %s %s", car.year, car.make.c_str(), car.model.c_str());
                    ImGui::Text("Color: %s | Scale: %s", car.color.c_str(), car.scale.c_str());
                    ImGui::Text("Condition: %s", car.condition.c_str());
                    if (ImGui::Checkbox((std::string("Favorite##") + std::to_string(i)).c_str(), &car.favorite)) {}
                    ImGui::EndChild();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Collage Stage")) {
                ImGui::Text("Layout Controls:");
                ImGui::Combo("Layout", &g_ActiveLayout, "Grid\0Strip\0Polaroid\0Stacked\0");
                ImGui::Combo("Theme Texture", &g_ActiveTheme, "Leather\0Carbon Fiber\0Fine Wood\0Deep Space\0");
                ImGui::SliderFloat("Spacing", &g_ItemSpacing, 0.0f, 32.0f);
                ImGui::SliderFloat("Borders", &g_BorderSize, 0.0f, 10.0f);

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                ImVec2 canvasSize = ImVec2(panelWidth - 30.0f, 300.0f);

                // Draw background box mapping textures based on selection
                ImU32 themeBgColor = IM_COL32(40, 40, 40, 255);
                if (g_ActiveTheme == THEME_LEATHER) themeBgColor = IM_COL32(50, 30, 20, 255);
                else if (g_ActiveTheme == THEME_CARBON) themeBgColor = IM_COL32(25, 25, 25, 255);
                else if (g_ActiveTheme == THEME_WOOD) themeBgColor = IM_COL32(110, 60, 30, 255);
                else if (g_ActiveTheme == THEME_SPACE) themeBgColor = IM_COL32(10, 10, 35, 255);

                drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), themeBgColor);

                // Render mock representations representing the layout parameters
                float itemWidth = 80.0f;
                float itemHeight = 60.0f;
                float currentX = canvasPos.x + 10.0f;
                float currentY = canvasPos.y + 10.0f;

                for (size_t i = 0; i < g_Catalog.size(); ++i) {
                    ImVec2 itemMin = ImVec2(currentX, currentY);
                    ImVec2 itemMax = ImVec2(currentX + itemWidth, currentY + itemHeight);

                    if (g_ActiveLayout == LAYOUT_STACKED) {
                        itemMin = ImVec2(canvasPos.x + 20.0f + (i * 25.0f), canvasPos.y + 40.0f + (i * 15.0f));
                        itemMax = ImVec2(itemMin.x + itemWidth, itemMin.y + itemHeight);
                    }

                    // Render mock picture cards
                    drawList->AddRectFilled(itemMin, itemMax, IM_COL32(150, 150, 150, 255));
                    if (g_BorderSize > 0) {
                        drawList->AddRect(itemMin, itemMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, g_BorderSize);
                    }

                    if (g_ActiveLayout == LAYOUT_GRID) {
                        currentX += itemWidth + g_ItemSpacing;
                        if (currentX + itemWidth > canvasPos.x + canvasSize.x) {
                            currentX = canvasPos.x + 10.0f;
                            currentY += itemHeight + g_ItemSpacing;
                        }
                    } else if (g_ActiveLayout == LAYOUT_STRIP || g_ActiveLayout == LAYOUT_POLAROID) {
                        currentX += itemWidth + g_ItemSpacing;
                    }
                }

                ImGui::Dummy(canvasSize);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // -------------------------------------------------------------
        // PANEL 3: AI HISTOLOGY NUGGETS & CHAT PROXY
        // -------------------------------------------------------------
        ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
        if (g_SelectedCarIndex >= 0 && g_SelectedCarIndex < (int)g_Catalog.size()) {
            auto& car = g_Catalog[g_SelectedCarIndex];

            ImGui::Text("AI Histology Nuggets: %s %s", car.make.c_str(), car.model.c_str());
            ImGui::Text("Collectability Score: %.1f / 10.0", car.collectabilityScore);
            ImGui::ProgressBar(car.collectabilityScore / 10.0f, ImVec2(0.0f, 0.0f));
            
            ImGui::Separator();
            ImGui::TextWrapped("History: %s", car.history.c_str());

            ImGui::Separator();
            ImGui::Text("Historical Specs:");
            for (const auto& spec : car.specs) {
                ImGui::Text("- %s: %s", spec.first.c_str(), spec.second.c_str());
            }

            ImGui::Separator();
            ImGui::Text("Automotive Trivia:");
            for (const auto& fact : car.facts) {
                ImGui::TextWrapped("* %s", fact.c_str());
            }

            ImGui::Separator();
            ImGui::Text("Gemini Q&A Terminal:");

            ImGui::BeginChild("ChatLogs", ImVec2(0, 150), true);
            for (const auto& log : g_ChatLog) {
                ImGui::TextColored(log.first == "You" ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f) : ImVec4(0.3f, 0.6f, 0.9f, 1.0f), "%s: ", log.first.c_str());
                ImGui::SameLine();
                ImGui::TextWrapped("%s", log.second.c_str());
            }
            ImGui::EndChild();

            ImGui::InputText("Ask...", g_ChatInput, IM_ARRAYSIZE(g_ChatInput));
            ImGui::SameLine();
            if (ImGui::Button("Send")) {
                std::string question = g_ChatInput;
                if (!question.empty()) {
                    g_ChatLog.push_back({"You", question});
                    std::string reply = getGeminiChatResponse(question, car);
                    g_ChatLog.push_back({"Gemini", reply});
                    memset(g_ChatInput, 0, sizeof(g_ChatInput));
                }
            }
        } else {
            ImGui::Text("Select a model vehicle to view catalog nuggets.");
        }
        ImGui::EndChild();

        ImGui::End();

        // Rendering commands
        ImGui::Render();
        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Terminate engine context
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
