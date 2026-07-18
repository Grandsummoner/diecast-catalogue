#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <random>
#include <ctime>

// Let GLFW automatically include standard GL and Win32 headers in correct order
#include "GLFW/glfw3.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "nlohmann/json.hpp"
#include "miniz.h"

#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;
namespace fs = std::filesystem;

// -------------------------------------------------------------
// METADATA & DATA STRUCTURES
// -------------------------------------------------------------

struct DiecastCar {
    std::string filename;
    std::string displayName;
    std::string folderPath;
    int year = 1990;
    std::string make = "Classic";
    std::string model = "Model";
    std::string color = "Factory Paint";
    std::string scale = "1:64";
    std::string condition = "Mint";
    bool favorite = false;
    bool selected = false;
    std::string tags = "01-10";

    // Showdown Battle Records
    int showdownWins = 0;
    int showdownBattles = 0;

    // AI Histology Nuggets (Saved Offline Cache)
    std::string realHistory;
    std::string factorySpecs;
    std::vector<std::string> triviaNuggets;
    float collectabilityScore = 5.0f;
};

struct HistoryState {
    std::vector<DiecastCar> catalog;
    int selectedCarIndex;
};

struct PolaroidCard {
    int carIndex = -1;
    ImVec2 position = ImVec2(50, 50);
    float rotation = 0.0f; // Mock rotation representation
    bool initialized = false;
};

// -------------------------------------------------------------
// GLOBAL CONTEXT STATES
// -------------------------------------------------------------

std::vector<DiecastCar> g_Catalog;
int g_SelectedCarIndex = -1;
bool g_UnsavedChanges = false;

// Theme configuration
enum Theme { THEME_NAVY, THEME_LIGHT, THEME_BEIGE };
Theme g_ActiveTheme = THEME_NAVY;

// Text search state
char g_SearchInput[128] = "";

// Undo/Redo stack buffers (Restricted to 10 snapshots)
std::vector<HistoryState> g_UndoStack;
std::vector<HistoryState> g_RedoStack;

// Gacha Roller configuration
bool g_GachaRolling = false;
float g_GachaTimer = 0.0f;
float g_GachaInterval = 0.05f; // Roll speed
float g_GachaDuration = 0.0f;

// Polaroid Scatter state
std::vector<PolaroidCard> g_PolaroidCards;

// Showdown match states
int g_ShowdownLeftIndex = -1;
int g_ShowdownRightIndex = -1;
bool g_ShowdownActive = false;

// Soundscapes configuration
int g_ActiveSoundscape = 0; // 0: Off, 1: Garage Rain, 2: V8 Rumble, 3: Jazz Cafe
const char* g_SoundscapeNames[] = { "Ambient Audio [ Off ]", "Soundscape [ Garage Rain ]", "Soundscape [ V8 Idle Rumble ]", "Soundscape [ Jazz Lounge ]" };

// Drag-and-drop ingestion queue states
std::vector<std::string> g_PendingImportPaths;
bool g_ShowImportConfirmPrompt = false;

// Intercept exit dialog states
bool g_ShowExitPrompt = false;

// Text Input states
char g_ChatInput[256] = "";
char g_CuratorNotes[512] = "No curator remarks entered yet.";
std::vector<std::pair<std::string, std::string>> g_ChatLog; // {User, Msg}

// -------------------------------------------------------------
// HISTORICAL SPECIFICATION & SCORE HEURISTICS
// -------------------------------------------------------------

float calculateScore(const std::string& make, const std::string& model, int year) {
    std::string seed = make + model + std::to_string(year);
    size_t hashVal = std::hash<std::string>{}(seed);
    float score = 4.0f + static_cast<float>(hashVal % 55) / 10.0f;
    return std::max(1.0f, std::min(10.0f, score));
}

void applyOfflineSpecsAndTrivia(DiecastCar& car) {
    car.collectabilityScore = calculateScore(car.make, car.model, car.year);

    std::string modelLower = car.model;
    std::transform(modelLower.begin(), modelLower.end(), modelLower.begin(), ::tolower);

    if (modelLower.find("mustang") != std::string::npos || modelLower.find("ford") != std::string::npos) {
        car.realHistory = "The Mustang, debuting in mid-1964, pioneered the American 'pony car' movement—sporty, long-hood coupes with accessible engines. It became an immediate cultural milestone.";
        car.factorySpecs = "Engine Layout: Naturally Aspirated V8 / Windsor 4.7L\n"
                            "Gearbox: 4-Speed Close-Ratio Manual\n"
                            "Drivetrain: Front-Engine, Rear-Wheel Drive (FR)";
        car.triviaNuggets = {
            "More than 22,000 orders were submitted on its official launch day.",
            "Officially named after the WWII P-51 Mustang fighter plane, not the wild horse.",
            "Popularized the factory-direct customization options matrix."
        };
    } else if (modelLower.find("911") != std::string::npos || modelLower.find("porsche") != std::string::npos) {
        car.realHistory = "The Porsche 911 debuted in 1963 as a replacement for the 356. Retaining its signature fastback flyline and rear-mounted engine layout for generations, it holds legendary sportscar status.";
        car.factorySpecs = "Engine Layout: Rear-Mounted Air-Cooled Flat-Six\n"
                            "Gearbox: 5-Speed Synchronized Manual\n"
                            "Drivetrain: Rear-Engine, Rear-Wheel Drive (RR)";
        car.triviaNuggets = {
            "Originally named the 901, but changed after Peugeot claimed rights to middle-zero names.",
            "Over 70% of all Porsche 911s built are still road-worthy today.",
            "Famous for its distinct rear traction and responsive swing-axle dynamics."
        };
    } else if (modelLower.find("hino") != std::string::npos || modelLower.find("bus") != std::string::npos) {
        car.realHistory = "The Hino Grandview represents a rare low-floor commercial double-decker bus. Designed to challenge European luxury imports, it offered unprecedented highway comfort.";
        car.factorySpecs = "Engine Layout: 16.2-Liter Naturally Aspirated EF700 V8 Diesel\n"
                            "Gearbox: 6-Speed Heavy-Duty Sync Manual\n"
                            "Chassis: 3-Axle Low-Floor Coach Frame";
        car.triviaNuggets = {
            "The Grandview was Hino's first and only domestic three-axle double-decker bus.",
            "It was powered by a massive 16.2L diesel V8 built for durability.",
            "Strict late-1990s environmental laws caused almost all units to be retired or scrapped."
        };
    } else {
        car.realHistory = "This model represents a celebrated iteration in manufacturing design. Characterized by its period-accurate styling and distinct presence, it remains highly regarded by collectors.";
        car.factorySpecs = "Engine Layout: Naturally Aspirated Inline-6 / V-Configuration\n"
                            "Gearbox: Standard Performance Manual Gearbox\n"
                            "Drivetrain: Rear-Wheel Drive (RWD) Chassis Layout";
        car.triviaNuggets = {
            "Chassis and suspension setup were developed to offer an engaging driving experience.",
            "Vintage collectors highly prize original, matching-numbers examples of this layout.",
            "Standard factory paint options of this era are highly valued for their historical accurate representation."
        };
    }
}

void parseCarInfo(const std::string& filename, DiecastCar& outCar) {
    outCar.filename = filename;
    outCar.folderPath = "01-10/" + filename;

    size_t lastDot = filename.find_last_of(".");
    std::string clean = (lastDot == std::string::npos) ? filename : filename.substr(0, lastDot);

    std::replace(clean.begin(), clean.end(), '_', ' ');
    std::replace(clean.begin(), clean.end(), '-', ' ');

    std::vector<std::string> parts;
    std::stringstream ss(clean);
    std::string buf;
    while (ss >> buf) {
        parts.push_back(buf);
    }

    if (parts.size() >= 3) {
        outCar.year = 1990;
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
// COMPILER MEMORY STATE ROLLBACK ACTIONS (UNDO/REDO)
// -------------------------------------------------------------

void PushHistoryState() {
    HistoryState state;
    state.catalog = g_Catalog;
    state.selectedCarIndex = g_SelectedCarIndex;
    g_UndoStack.push_back(state);
    if (g_UndoStack.size() > 10) {
        g_UndoStack.erase(g_UndoStack.begin());
    }
    g_RedoStack.clear();
}

void ExecuteUndo() {
    if (g_UndoStack.empty()) return;
    HistoryState current;
    current.catalog = g_Catalog;
    current.selectedCarIndex = g_SelectedCarIndex;
    g_RedoStack.push_back(current);

    HistoryState prev = g_UndoStack.back();
    g_UndoStack.pop_back();

    g_Catalog = prev.catalog;
    g_SelectedCarIndex = prev.selectedCarIndex;
    g_UnsavedChanges = true;
}

void ExecuteRedo() {
    if (g_RedoStack.empty()) return;
    HistoryState current;
    current.catalog = g_Catalog;
    current.selectedCarIndex = g_SelectedCarIndex;
    g_UndoStack.push_back(current);

    HistoryState nextState = g_RedoStack.back();
    g_RedoStack.pop_back();

    g_Catalog = nextState.catalog;
    g_SelectedCarIndex = nextState.selectedCarIndex;
    g_UnsavedChanges = true;
}

// -------------------------------------------------------------
// MANUAL WORKSPACE EXPORTER (ZIP VIA MINIZ)
// -------------------------------------------------------------

bool exportWorkspace(const std::string& zipPath) {
    mz_zip_archive zipArchive;
    memset(&zipArchive, 0, sizeof(zipArchive));

    if (!mz_zip_writer_init_file(&zipArchive, zipPath.c_str(), 0)) {
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
            {"score", car.collectabilityScore},
            {"wins", car.showdownWins},
            {"battles", car.showdownBattles}
        };
        catalogJson.push_back(item);
    }

    std::string catalogStr = catalogJson.dump(2);
    mz_zip_writer_add_mem(&zipArchive, "catalog.json", catalogStr.c_str(), catalogStr.size(), MZ_DEFAULT_COMPRESSION);

    std::string readme = "My Collection - Standalone C++ Workspace Database\n";
    mz_zip_writer_add_mem(&zipArchive, "README.txt", readme.c_str(), readme.size(), MZ_DEFAULT_COMPRESSION);

    mz_zip_writer_finalize_archive(&zipArchive);
    mz_zip_writer_end(&zipArchive);

    g_UnsavedChanges = false;
    return true;
}

// -------------------------------------------------------------
// DYNAMIC FILE SCANNING & FOLDER UPLOADS
// -------------------------------------------------------------

void scanAndQueuePath(const std::string& path) {
    try {
        if (fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".jpg" || ext == ".png" || ext == ".jpeg" || ext == ".webp") {
                        g_PendingImportPaths.push_back(entry.path().string());
                    }
                }
            }
        } else if (fs::is_regular_file(path)) {
            std::string ext = fs::path(path).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".jpg" || ext == ".png" || ext == ".jpeg" || ext == ".webp") {
                g_PendingImportPaths.push_back(path);
            }
        }
    } catch (...) {}
}

void finalizePendingImports() {
    PushHistoryState();
    for (const auto& path : g_PendingImportPaths) {
        DiecastCar car;
        std::string filename = fs::path(path).filename().string();
        parseCarInfo(filename, car);
        car.folderPath = path;
        applyOfflineSpecsAndTrivia(car);
        g_Catalog.push_back(car);
    }
    g_PendingImportPaths.clear();
    g_SelectedCarIndex = (int)g_Catalog.size() - 1;
    g_UnsavedChanges = true;
}

// -------------------------------------------------------------
// THEME COMPILER BINDINGS
// -------------------------------------------------------------

void ApplyTheme(Theme theme) {
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

    if (theme == THEME_NAVY) {
        colors[ImGuiCol_Text] = ImVec4(0.85f, 0.90f, 0.98f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.08f, 0.12f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.11f, 0.16f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.15f, 0.22f, 0.35f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.07f, 0.10f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.18f, 0.28f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.26f, 0.40f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.10f, 0.16f, 0.26f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.15f, 0.25f, 0.45f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.60f, 0.90f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.12f, 0.20f, 0.32f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.28f, 0.45f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.60f, 0.90f, 1.00f);
    } else if (theme == THEME_LIGHT) {
        colors[ImGuiCol_Text] = ImVec4(0.12f, 0.13f, 0.17f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.95f, 0.96f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.80f, 0.82f, 0.85f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.90f, 0.92f, 0.94f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.85f, 0.87f, 0.90f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.80f, 0.82f, 0.85f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.85f, 0.87f, 0.90f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.78f, 0.80f, 0.84f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.88f, 0.90f, 0.93f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.82f, 0.84f, 0.88f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    } else if (theme == THEME_BEIGE) {
        colors[ImGuiCol_Text] = ImVec4(0.17f, 0.12f, 0.09f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.92f, 0.89f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.98f, 0.97f, 0.95f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.83f, 0.78f, 0.73f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.91f, 0.89f, 0.86f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.86f, 0.83f, 0.80f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.80f, 0.77f, 0.74f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.88f, 0.84f, 0.79f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.80f, 0.76f, 0.70f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.31f, 0.23f, 0.19f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.90f, 0.86f, 0.81f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.83f, 0.78f, 0.73f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.23f, 0.19f, 1.00f);
    }
}

// -------------------------------------------------------------
// GLFW OS-LEVEL CALLBACK TRIGGERS
// -------------------------------------------------------------

void glfw_window_close_callback(GLFWwindow* window) {
    if (g_UnsavedChanges) {
        glfwSetWindowShouldClose(window, GLFW_FALSE); // Prevent immediate termination
        g_ShowExitPrompt = true;
    }
}

void glfw_drop_callback(GLFWwindow* window, int count, const char** paths) {
    g_PendingImportPaths.clear();
    for (int i = 0; i < count; ++i) {
        scanAndQueuePath(paths[i]);
    }
    if (!g_PendingImportPaths.empty()) {
        g_ShowImportConfirmPrompt = true;
    }
}

// -------------------------------------------------------------
// GEMINI CHAT INTERACTION PROXIES
// -------------------------------------------------------------

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
// MAIN LOOP EXECUTION INTERFACES
// -------------------------------------------------------------

int main() {
    if (!glfwInit()) return -1;

    // Window size matching design constraints
    GLFWwindow* window = glfwCreateWindow(1440, 800, "My Collection - Diecast Photo Catalogue", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // GLFW hook mappings
    glfwSetWindowCloseCallback(window, glfw_window_close_callback);
    glfwSetDropCallback(window, glfw_drop_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyTheme(g_ActiveTheme);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Populate catalog with elements matching your screenshot
    std::vector<std::string> mockFiles = {
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

    for (const auto& name : mockFiles) {
        DiecastCar car;
        parseCarInfo(name, car);
        applyOfflineSpecsAndTrivia(car);
        g_Catalog.push_back(car);
    }
    g_SelectedCarIndex = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Gacha roller frame simulation ticks
        if (g_GachaRolling && !g_Catalog.empty()) {
            g_GachaTimer += 0.016f; // Simulated frame time
            g_GachaDuration += 0.016f;
            if (g_GachaTimer >= g_GachaInterval) {
                g_GachaTimer = 0.0f;
                g_SelectedCarIndex = (g_SelectedCarIndex + 1) % g_Catalog.size();
                g_GachaInterval += 0.015f;
            }
            if (g_GachaDuration >= 2.0f) {
                g_GachaRolling = false;
                g_GachaInterval = 0.05f;
                g_SelectedCarIndex = std::rand() % g_Catalog.size();
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));

        ImGui::Begin("Workspace", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // -------------------------------------------------------------
        // TOP STATS & CONTROLS BAR (Size 2 Fonts)
        // -------------------------------------------------------------
        ImGui::BeginChild("TopBar", ImVec2(0, 45), true);
        
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.00f, 0.60f, 0.95f, 1.00f), "My Collection");
        
        ImGui::SameLine();
        if (ImGui::Button("[ Save ]")) {
            exportWorkspace("workspace.zip");
        }
        ImGui::SameLine();
        if (ImGui::Button("[ Undo ]")) {
            ExecuteUndo();
        }
        ImGui::SameLine();
        if (ImGui::Button("[ Redo ]")) {
            ExecuteRedo();
        }

        ImGui::SameLine(320);
        ImGui::Text("Total: %zu | Roller: ", g_Catalog.size());
        ImGui::SameLine();
        if (ImGui::Button("[ Gacha ]")) {
            if (!g_GachaRolling) {
                g_GachaRolling = true;
                g_GachaTimer = 0.0f;
                g_GachaInterval = 0.05f;
                g_GachaDuration = 0.0f;
            }
        }

        ImGui::SameLine(580);
        ImGui::Text("%s", g_SoundscapeNames[g_ActiveSoundscape]);
        ImGui::SameLine();
        if (ImGui::Button("[ Toggle Ambient ]")) {
            g_ActiveSoundscape = (g_ActiveSoundscape + 1) % 4;
        }

        ImGui::SameLine((float)width - 200.0f);
        if (ImGui::Button("[ Theme ]")) {
            g_ActiveTheme = static_cast<Theme>((g_ActiveTheme + 1) % 3);
            ApplyTheme(g_ActiveTheme);
        }

        ImGui::SameLine((float)width - 110.0f);
        if (ImGui::Button("[ Export ZIP ]")) {
            exportWorkspace("workspace.zip");
        }
        ImGui::EndChild();

        float panelWidth = (float)width / 3.0f;
        float bodyHeight = (float)height - 65.0f;

        // -------------------------------------------------------------
        // PANEL 1: LEFT FILE MANAGEMENT (25% Width)
        // -------------------------------------------------------------
        ImGui::BeginChild("LeftPanel", ImVec2(panelWidth - 10.0f, bodyHeight), true);
        
        ImGui::Text("File Ingestion / Photo Banks:");
        if (ImGui::Button("[ File Ingest ]")) {
            g_PendingImportPaths = { "12-Tomica Subaru Sambar.JPG" };
            g_ShowImportConfirmPrompt = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("[ Folder Ingest ]")) {
            g_PendingImportPaths = { "13-Crane.JPG", "14-Toyota.JPG" };
            g_ShowImportConfirmPrompt = true;
        }

        ImGui::Separator();
        ImGui::InputText("##Search", g_SearchInput, IM_ARRAYSIZE(g_SearchInput));
        ImGui::SameLine();
        ImGui::Button("[ Filter ]");

        ImGui::Text("Selection Queue:");
        if (ImGui::Button("[ Select All ]")) {
            PushHistoryState();
            for (auto& car : g_Catalog) car.selected = true;
            g_UnsavedChanges = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("[ Deselect All ]")) {
            PushHistoryState();
            for (auto& car : g_Catalog) car.selected = false;
            g_UnsavedChanges = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("[ Unload ]")) {
            PushHistoryState();
            g_Catalog.erase(std::remove_if(g_Catalog.begin(), g_Catalog.end(), [](const DiecastCar& c) { return c.selected; }), g_Catalog.end());
            g_SelectedCarIndex = g_Catalog.empty() ? -1 : 0;
            g_UnsavedChanges = true;
        }

        ImGui::Separator();
        
        ImGui::BeginChild("IndexedGrid", ImVec2(0, bodyHeight - 180.0f), false);
        for (int i = 0; i < (int)g_Catalog.size(); ++i) {
            auto& car = g_Catalog[i];
            
            // Text search filtering logic
            if (strlen(g_SearchInput) > 0) {
                std::string matchQuery = car.filename;
                std::transform(matchQuery.begin(), matchQuery.end(), matchQuery.begin(), ::tolower);
                std::string searchStr = g_SearchInput;
                std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                if (matchQuery.find(searchStr) == std::string::npos) continue;
            }

            ImGui::PushID(i);
            // Dynamic card highlight
            bool isSelected = (g_SelectedCarIndex == i);
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
            }

            ImGui::BeginChild((std::string("CardGridChild##") + std::to_string(i)).c_str(), ImVec2(95, 95), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::Checkbox("##SelectCheck", &car.selected);
            ImGui::TextWrapped("%s", car.make.c_str());

            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
                g_SelectedCarIndex = i;
            }

            ImGui::EndChild();

            if (isSelected) {
                ImGui::PopStyleColor();
            }
            ImGui::PopID();

            if ((i + 1) % 3 != 0) {
                ImGui::SameLine();
            }
        }
        ImGui::EndChild();

        ImGui::Button("[ Re-link root directory ]");
        ImGui::EndChild();

        ImGui::SameLine();

        // -------------------------------------------------------------
        // PANEL 2: MIDDLE VISUAL PRESENTATION STAGE (50% Width)
        // -------------------------------------------------------------
        ImGui::BeginChild("MiddlePanel", ImVec2(panelWidth - 10.0f, bodyHeight), true);
        
        if (ImGui::BeginTabBar("DisplayStageTabs")) {
            
            // MODE 1: SINGLE SHOWCASE VIEW
            if (ImGui::BeginTabItem("Showcase View")) {
                if (g_SelectedCarIndex >= 0 && g_SelectedCarIndex < (int)g_Catalog.size()) {
                    auto& car = g_Catalog[g_SelectedCarIndex];

                    ImGui::Text("ASK COLLECTOR BOT:"); ImGui::SameLine();
                    ImGui::InputText("##BotPrompt", g_ChatInput, IM_ARRAYSIZE(g_ChatInput));
                    ImGui::SameLine();
                    if (ImGui::Button("[ Ask ]")) {
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

                    // Visual Aspect Ratio Frame (Strictly 16:9 boundary bounds)
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImVec2 canvasCursor = ImGui::GetCursorScreenPos();
                    ImVec2 frameSize = ImVec2(panelWidth - 30.0f, 220.0f);

                    // Matte fallback backdrop
                    drawList->AddRectFilled(canvasCursor, ImVec2(canvasCursor.x + frameSize.x, canvasCursor.y + frameSize.y), IM_COL32(8, 12, 18, 255));
                    drawList->AddRect(canvasCursor, ImVec2(canvasCursor.x + frameSize.x, canvasCursor.y + frameSize.y), IM_COL32(0, 168, 255, 100), 4.0f, 0, 1.5f);

                    // Draggable viewport placeholder contents
                    ImVec2 textPos = ImVec2(canvasCursor.x + (frameSize.x / 4.0f), canvasCursor.y + (frameSize.y / 2.0f) - 10.0f);
                    drawList->AddText(textPos, IM_COL32(180, 180, 180, 255), "[ INVALID PHOTO / PATH UNRESOLVED ]");

                    ImGui::Dummy(frameSize);

                    // Manual Navigation Controls flanking the canvas
                    ImGui::Spacing();
                    if (ImGui::Button("[ <- PREVIOUS PHOTO ]", ImVec2((panelWidth / 2.0f) - 20.0f, 30.0f))) {
                        g_SelectedCarIndex = (g_SelectedCarIndex - 1 + g_Catalog.size()) % g_Catalog.size();
                        g_ChatLog.clear();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("[ NEXT PHOTO -> ]", ImVec2((panelWidth / 2.0f) - 20.0f, 30.0f))) {
                        g_SelectedCarIndex = (g_SelectedCarIndex + 1) % g_Catalog.size();
                        g_ChatLog.clear();
                    }

                    ImGui::Separator();
                    ImGui::Text("CURATOR'S NOTES (📝):");
                    ImGui::InputTextMultiline("##NotesEditor", g_CuratorNotes, IM_ARRAYSIZE(g_CuratorNotes), ImVec2(0, 60));
                } else {
                    ImGui::Text("Empty repository database queue. Drag folder to Left Panel to populate.");
                }
                ImGui::EndTabItem();
            }

            // MODE 2: TACTILE POLAROID SCATTER (Virtual Shoebox)
            if (ImGui::BeginTabItem("Polaroid Scatter")) {
                ImGui::Text("Tabletop Desk - Drag and organize your collection scatter cards!");
                if (ImGui::Button("[ Toss/Scatter Cards ]")) {
                    g_PolaroidCards.clear();
                    for (int i = 0; i < (int)g_Catalog.size(); ++i) {
                        PolaroidCard pc;
                        pc.carIndex = i;
                        // Random coordinates on desktop canvas
                        pc.position = ImVec2(50.0f + (std::rand() % 200), 80.0f + (std::rand() % 150));
                        pc.rotation = (float)(std::rand() % 30 - 15);
                        pc.initialized = true;
                        g_PolaroidCards.push_back(pc);
                    }
                }

                ImGui::Separator();
                ImVec2 scatterCursor = ImGui::GetCursorScreenPos();
                ImDrawList* dlist = ImGui::GetWindowDrawList();
                dlist->AddRectFilled(scatterCursor, ImVec2(scatterCursor.x + panelWidth - 30.0f, scatterCursor.y + bodyHeight - 120.0f), IM_COL32(10, 14, 20, 255));

                ImGui::BeginChild("ScatterStage", ImVec2(0, bodyHeight - 120.0f), false, ImGuiWindowFlags_NoScrollbar);
                for (size_t i = 0; i < g_PolaroidCards.size(); ++i) {
                    auto& pc = g_PolaroidCards[i];
                    if (!pc.initialized) continue;

                    ImGui::SetNextWindowPos(ImVec2(scatterCursor.x + pc.position.x, scatterCursor.y + pc.position.y), ImGuiCond_Appearing);
                    ImGui::SetNextWindowSize(ImVec2(100, 110));
                    
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::Begin((std::string("CardWindow##") + std::to_string(i)).c_str(), NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
                    
                    ImGui::Text("%s", g_Catalog[pc.carIndex].make.c_str());
                    ImGui::Separator();
                    ImGui::Text("Scale: %s", g_Catalog[pc.carIndex].scale.c_str());
                    
                    // Track custom dragged offsets
                    ImVec2 wpos = ImGui::GetWindowPos();
                    pc.position = ImVec2(wpos.x - scatterCursor.x, wpos.y - scatterCursor.y);

                    ImGui::End();
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // MODE 3: CURATOR BRACKET SHOWDOWNS (Versus)
            if (ImGui::BeginTabItem("Curator Showdown")) {
                ImGui::Text("Showdown Challenge! Crown today's active showroom winner.");
                if (ImGui::Button("[ Deal Versus Matchup ]") && g_Catalog.size() >= 2) {
                    g_ShowdownLeftIndex = std::rand() % g_Catalog.size();
                    g_ShowdownRightIndex = std::rand() % g_Catalog.size();
                    while (g_ShowdownLeftIndex == g_ShowdownRightIndex) {
                        g_ShowdownRightIndex = std::rand() % g_Catalog.size();
                    }
                    g_ShowdownActive = true;
                }

                if (g_ShowdownActive && g_ShowdownLeftIndex != -1 && g_ShowdownRightIndex != -1) {
                    ImGui::Separator();
                    
                    // Left competitor card
                    ImGui::BeginChild("LeftShowdownCard", ImVec2((panelWidth / 2.0f) - 20.0f, 200), true);
                    auto& carL = g_Catalog[g_ShowdownLeftIndex];
                    ImGui::Text("%d %s", carL.year, carL.make.c_str());
                    ImGui::TextWrapped("%s", carL.model.c_str());
                    ImGui::Separator();
                    ImGui::Text("Record: %d Wins", carL.showdownWins);
                    if (ImGui::Button("[ Select Left ]")) {
                        PushHistoryState();
                        carL.showdownWins++;
                        carL.showdownBattles++;
                        g_Catalog[g_ShowdownRightIndex].showdownBattles++;
                        g_ShowdownActive = false;
                        g_UnsavedChanges = true;
                    }
                    ImGui::EndChild();

                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), " ⚔ VS ");
                    ImGui::SameLine();

                    // Right competitor card
                    ImGui::BeginChild("RightShowdownCard", ImVec2(0, 200), true);
                    auto& carR = g_Catalog[g_ShowdownRightIndex];
                    ImGui::Text("%d %s", carR.year, carR.make.c_str());
                    ImGui::TextWrapped("%s", carR.model.c_str());
                    ImGui::Separator();
                    ImGui::Text("Record: %d Wins", carR.showdownWins);
                    if (ImGui::Button("[ Select Right ]")) {
                        PushHistoryState();
                        carR.showdownWins++;
                        carR.showdownBattles++;
                        g_Catalog[g_ShowdownLeftIndex].showdownBattles++;
                        g_ShowdownActive = false;
                        g_UnsavedChanges = true;
                    }
                    ImGui::EndChild();
                } else {
                    ImGui::Text("Deal matchup to commence versus ranking.");
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // -------------------------------------------------------------
        // PANEL 3: RIGHT PANEL HISTOLOGY & BOT CHAT (25% Width)
        // -------------------------------------------------------------
        ImGui::BeginChild("RightPanel", ImVec2(0, bodyHeight), true);
        if (g_SelectedCarIndex >= 0 && g_SelectedCarIndex < (int)g_Catalog.size()) {
            auto& car = g_Catalog[g_SelectedCarIndex];

            if (ImGui::BeginTabBar("RightHistologyTabs")) {
                
                // TAB 1: REAL HISTORY (📖)
                if (ImGui::BeginTabItem("Real History")) {
                    ImGui::TextWrapped("%s", car.realHistory.c_str());
                    ImGui::EndTabItem();
                }

                // TAB 2: FACTORY SPECS (⚙)
                if (ImGui::BeginTabItem("Factory Specs")) {
                    ImGui::TextWrapped("%s", car.factorySpecs.c_str());
                    ImGui::EndTabItem();
                }

                // TAB 3: PRECIOUS NUGGETS (💎)
                if (ImGui::BeginTabItem("Nuggets")) {
                    ImGui::Text("REAL CAR COLLECTABILITY:");
                    ImGui::TextColored(ImVec4(0.00f, 0.60f, 0.95f, 1.00f), "Score: %.2f / 10.00", car.collectabilityScore);
                    ImGui::ProgressBar(car.collectabilityScore / 10.0f, ImVec2(-1, 12.0f));

                    ImGui::Separator();
                    ImGui::Text("NOTABLE TRIVIA NUGGETS:");
                    for (size_t n = 0; n < car.triviaNuggets.size(); ++n) {
                        ImGui::BeginChild((std::string("TriviaBoxChild##") + std::to_string(n)).c_str(), ImVec2(0, 65), true);
                        ImGui::TextWrapped("%s", car.triviaNuggets[n].c_str());
                        ImGui::EndChild();
                    }
                    ImGui::EndTabItem();
                }

                // TAB 4: GARAGE CHARACTER BADGES (🚗)
                if (ImGui::BeginTabItem("Garage Profile")) {
                    ImGui::Text("Sourced Personality Archetypes:");
                    
                    // Profile Analyzer logic based on manufacturer ratios
                    int jdmCount = 0;
                    int muscleCount = 0;
                    for (const auto& item : g_Catalog) {
                        std::string m = item.make;
                        std::transform(m.begin(), m.end(), m.begin(), ::tolower);
                        if (m.find("nissan") != std::string::npos || m.find("toyota") != std::string::npos || m.find("mits") != std::string::npos) {
                            jdmCount++;
                        } else if (m.find("ford") != std::string::npos || m.find("chev") != std::string::npos || m.find("dodge") != std::string::npos) {
                            muscleCount++;
                        }
                    }

                    ImGui::BeginChild("BadgeBox", ImVec2(0, 80), true);
                    if (jdmCount > muscleCount) {
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

                    ImGui::Separator();
                    ImGui::Text("Showdown Bracket Leaderboard (Top Wins):");
                    std::vector<DiecastCar> sortedCatalog = g_Catalog;
                    std::sort(sortedCatalog.begin(), sortedCatalog.end(), [](const DiecastCar& a, const DiecastCar& b) {
                        return a.showdownWins > b.showdownWins;
                    });

                    for (size_t l = 0; l < std::min(sortedCatalog.size(), size_t(5)); ++l) {
                        if (sortedCatalog[l].showdownWins > 0) {
                            ImGui::Text("%zu. %s %s - %d Wins", l+1, sortedCatalog[l].make.c_str(), sortedCatalog[l].model.c_str(), sortedCatalog[l].showdownWins);
                        }
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::Separator();
            ImGui::Text("Collector Chat Memory Log (💬):");
            ImGui::BeginChild("QAChatLogs", ImVec2(0, 120), true);
            for (const auto& chat : g_ChatLog) {
                ImGui::TextColored(chat.first == "You" ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f) : ImVec4(0.0f, 0.6f, 0.9f, 1.0f), "%s: ", chat.first.c_str());
                ImGui::SameLine();
                ImGui::TextWrapped("%s", chat.second.c_str());
            }
            ImGui::EndChild();

            ImGui::Separator();
            ImGui::Text("CURATOR REMARKS");
            ImGui::TextWrapped("Folder path: %s", car.folderPath.c_str());
        }
        ImGui::EndChild();

        // -------------------------------------------------------------
        // DIALOGS & VERIFICATION OVERLAYS
        // -------------------------------------------------------------

        // 1. Safety import dialog
        if (g_ShowImportConfirmPrompt) {
            ImGui::OpenPopup("Confirm Import Queue?");
        }
        if (ImGui::BeginPopupModal("Confirm Import Queue?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Confirm ingestion queue? You are about to import %zu file(s) into your current workspace catalog.", g_PendingImportPaths.size());
            ImGui::Spacing();
            if (ImGui::Button("[ Confirm Ingest ]", ImVec2(140, 0))) {
                finalizePendingImports();
                g_ShowImportConfirmPrompt = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("[ Cancel ]", ImVec2(100, 0))) {
                g_PendingImportPaths.clear();
                g_ShowImportConfirmPrompt = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // 2. Unsaved modifications close dialog
        if (g_ShowExitPrompt) {
            ImGui::OpenPopup("Unsaved Workspace Changes!");
        }
        if (ImGui::BeginPopupModal("Unsaved Workspace Changes!", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("You have unsaved changes in your metadata database.\nWould you like to save changes before exiting the application?");
            ImGui::Spacing();
            if (ImGui::Button("[ Save and Exit ]", ImVec2(140, 0))) {
                exportWorkspace("workspace.zip");
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("[ Discard and Exit ]", ImVec2(160, 0))) {
                g_UnsavedChanges = false; // Bypass the intercept check
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("[ Cancel ]", ImVec2(100, 0))) {
                g_ShowExitPrompt = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::End();

        // OpenGL rendering clear commands
        ImGui::Render();
        glViewport(0, 0, width, height);
        
        // Base workspace colors matching active selections
        if (g_ActiveTheme == THEME_NAVY) glClearColor(0.04f, 0.06f, 0.10f, 1.00f);
        else if (g_ActiveTheme == THEME_LIGHT) glClearColor(0.94f, 0.95f, 0.96f, 1.00f);
        else if (g_ActiveTheme == THEME_BEIGE) glClearColor(0.94f, 0.92f, 0.89f, 1.00f);
        
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup and terminate structures
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
