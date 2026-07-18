#define NOMINMAX
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <random>
#include <ctime>
#include <windows.h>
#include <winhttp.h>

#include "GLFW/glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "nlohmann/json.hpp"
#include "miniz.h"

#pragma comment(lib, "winhttp.lib")

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

struct DiecastCar {
    std::string filename, displayName, folderPath, make = "Classic", model = "Model", color = "Paint", scale = "1:64", condition = "Mint", tags = "01-10", realHistory, factorySpecs;
    int year = 1990, showdownWins = 0, showdownBattles = 0;
    bool favorite = false, selected = false;
    std::vector<std::string> triviaNuggets;
    float collectabilityScore = 5.0f;
};

struct HistoryState { std::vector<DiecastCar> catalog; int selectedCarIndex; };
struct PolaroidCard { int carIndex = -1; ImVec2 position = ImVec2(50, 50); float rotation = 0.0f; bool initialized = false; };

// Globals
std::vector<DiecastCar> g_Catalog;
int g_SelectedCarIndex = -1;
bool g_UnsavedChanges = false;
enum Theme { THEME_LIGHT, THEME_NAVY, THEME_BEIGE };
Theme g_ActiveTheme = THEME_LIGHT;
char g_SearchInput[128] = "";
char g_ApiKeyInput[128] = "";

// Texture cache
std::map<std::string, GLuint> g_TextureCache;
int g_ActiveTextureWidth = 0, g_ActiveTextureHeight = 0;
GLuint g_ActiveTextureID = 0;
std::string g_ActiveTexturePath = "";

// State buffers
std::vector<HistoryState> g_UndoStack, g_RedoStack;
bool g_GachaRolling = false;
float g_GachaTimer = 0.0f, g_GachaInterval = 0.05f, g_GachaDuration = 0.0f;
std::vector<PolaroidCard> g_PolaroidCards;
int g_ShowdownLeftIndex = -1, g_ShowdownRightIndex = -1;
bool g_ShowdownActive = false;

// Interface state helpers
int g_ActiveSoundscape = 0;
const char* g_SoundscapeNames[] = { "Soundscape: Off", "Soundscape: Rain", "Soundscape: V8 Idle", "Soundscape: Jazz Cafe" };
std::vector<std::string> g_PendingImportPaths;
bool g_ShowImportConfirmPrompt = false, g_ShowExitPrompt = false;
char g_ChatInput[256] = "";
char g_CuratorNotes[512] = "No curator remarks entered yet.";
std::vector<std::pair<std::string, std::string>> g_ChatLog;
int g_StarredCount = 0;

std::wstring toWString(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size);
    return wstr;
}

GLuint GetOrCreateTexture(const std::string& path) {
    if (path.empty()) return 0;
    auto it = g_TextureCache.find(path);
    if (it != g_TextureCache.end()) return it->second;
    if (g_TextureCache.size() > 100) {
        auto first = g_TextureCache.begin();
        glDeleteTextures(1, &(first->second));
        g_TextureCache.erase(first);
    }
    GLuint texID = 0;
    int channels, w, h;
    unsigned char* data = nullptr;
    #ifdef _WIN32
    std::wstring wpath = toWString(path);
    FILE* f = _wfopen(wpath.c_str(), L"rb");
    #else
    FILE* f = fopen(path.c_str(), "rb");
    #endif
    if (f) { data = stbi_load_from_file(f, &w, &h, &channels, 4); fclose(f); }
    if (data) {
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        if (g_SelectedCarIndex >= 0 && g_Catalog[g_SelectedCarIndex].folderPath == path) {
            g_ActiveTextureWidth = w; g_ActiveTextureHeight = h; g_ActiveTextureID = texID;
        }
        g_TextureCache[path] = texID;
        return texID;
    }
    return 0;
}

std::string makeHttpsPostRequest(const std::string& host, const std::string& path, const std::string& payload) {
    std::string response;
    HINTERNET hSession = WinHttpOpen(L"DiecastCatalogue/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";
    HINTERNET hConnect = WinHttpConnect(hSession, toWString(host).c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", toWString(path).c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }
    std::wstring headers = L"Content-Type: application/json\r\n";
    BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1L, (LPVOID)payload.c_str(), (DWORD)payload.size(), (DWORD)payload.size(), 0);
    if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (bResults) {
        DWORD dwSize = 0;
        do {
            DWORD dwDownloaded = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;
            std::vector<char> buffer(dwSize + 1, 0);
            if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) response.append(buffer.data(), dwDownloaded);
        } while (dwSize > 0);
    }
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return response;
}

float calculateScore(const std::string& make, const std::string& model, int year) {
    std::string seed = make + model + std::to_string(year);
    size_t hashVal = std::hash<std::string>{}(seed);
    return (std::max)(1.0f, (std::min)(10.0f, 4.0f + static_cast<float>(hashVal % 55) / 10.0f));
}

void applyOfflineSpecsAndTrivia(DiecastCar& car) {
    car.collectabilityScore = calculateScore(car.make, car.model, car.year);
    std::string modelLower = car.model;
    std::transform(modelLower.begin(), modelLower.end(), modelLower.begin(), ::tolower);
    if (modelLower.find("mustang") != std::string::npos || modelLower.find("ford") != std::string::npos) {
        car.realHistory = "The Mustang, debuting in mid-1964, pioneered the American 'pony car' movement—sporty, long-hood coupes with accessible engines.";
        car.factorySpecs = "Engine Layout: Naturally Aspirated V8 / Windsor 4.7L\nGearbox: 4-Speed Close-Ratio Manual\nDrivetrain: Front-Engine, Rear-Wheel Drive (FR)";
        car.triviaNuggets = { "More than 22,000 orders were submitted on launch day.", "Named after the WWII P-51 fighter plane, not the horse." };
    } else if (modelLower.find("911") != std::string::npos || modelLower.find("porsche") != std::string::npos) {
        car.realHistory = "The Porsche 911 debuted in 1963 as a replacement for the 356. It holds legendary sportscar status with its signature fastback flyline.";
        car.factorySpecs = "Engine Layout: Rear-Mounted Air-Cooled Flat-Six\nGearbox: 5-Speed Synchronized Manual\nDrivetrain: Rear-Engine, Rear-Wheel Drive (RR)";
        car.triviaNuggets = { "Originally named the 901, but adjusted due to Peugeot naming rights.", "Over 70% of all 911s built are still road-worthy today." };
    } else {
        car.realHistory = "This model represents a celebrated iteration in manufacturing design. Characterized by its period-accurate styling and distinct presence, it remains highly regarded.";
        car.factorySpecs = "Engine Layout: Naturally Aspirated Inline-6 / V-Configuration\nGearbox: Performance Manual Gearbox\nDrivetrain: Rear-Wheel Drive (RWD) Layout";
        car.triviaNuggets = { "Chassis geometries were optimized for grand touring stability.", "Vintage matching-numbers configurations command high premiums." };
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
    std::stringstream ss(clean); std::string buf;
    while (ss >> buf) parts.push_back(buf);
    if (parts.size() >= 3) {
        outCar.year = 1990; outCar.make = parts[0]; outCar.make[0] = std::toupper(outCar.make[0]);
        std::string compiledModel = "";
        for (size_t i = 1; i < parts.size(); ++i) {
            std::string segment = parts[i]; segment[0] = std::toupper(segment[0]);
            if (i > 1) compiledModel += " ";
            compiledModel += segment;
        }
        outCar.model = compiledModel;
    } else {
        outCar.year = 1990; outCar.make = "Generic"; outCar.model = clean;
    }
    outCar.color = "Paint"; outCar.displayName = clean;
}

// State Machine
void PushHistoryState() {
    HistoryState state; state.catalog = g_Catalog; state.selectedCarIndex = g_SelectedCarIndex;
    g_UndoStack.push_back(state);
    if (g_UndoStack.size() > 10) g_UndoStack.erase(g_UndoStack.begin());
    g_RedoStack.clear();
}

void ExecuteUndo() {
    if (g_UndoStack.empty()) return;
    HistoryState current; current.catalog = g_Catalog; current.selectedCarIndex = g_SelectedCarIndex;
    g_RedoStack.push_back(current);
    HistoryState prev = g_UndoStack.back(); g_UndoStack.pop_back();
    g_Catalog = prev.catalog; g_SelectedCarIndex = prev.selectedCarIndex;
    g_UnsavedChanges = true;
}

void ExecuteRedo() {
    if (g_RedoStack.empty()) return;
    HistoryState current; current.catalog = g_Catalog; current.selectedCarIndex = g_SelectedCarIndex;
    g_UndoStack.push_back(current);
    HistoryState next = g_RedoStack.back(); g_RedoStack.pop_back();
    g_Catalog = next.catalog; g_SelectedCarIndex = next.selectedCarIndex;
    g_UnsavedChanges = true;
}

bool exportWorkspace(const std::string& zipPath) {
    mz_zip_archive zip; memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, zipPath.c_str(), 0)) return false;
    json catalogJson = json::array();
    for (const auto& car : g_Catalog) {
        json item = {
            {"filename", car.filename}, {"folderPath", car.folderPath}, {"year", car.year},
            {"make", car.make}, {"model", car.model}, {"scale", car.scale}, {"condition", car.condition},
            {"favorite", car.favorite}, {"score", car.collectabilityScore}, {"wins", car.showdownWins}, {"battles", car.showdownBattles}
        };
        catalogJson.push_back(item);
    }
    std::string s = catalogJson.dump(2);
    mz_zip_writer_add_mem(&zip, "catalog.json", s.c_str(), s.size(), MZ_DEFAULT_COMPRESSION);
    mz_zip_writer_finalize_archive(&zip); mz_zip_writer_end(&zip);
    g_UnsavedChanges = false;
    return true;
}

void scanAndQueuePath(const std::string& path) {
    try {
        if (fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".jpg" || ext == ".png" || ext == ".jpeg" || ext == ".webp") g_PendingImportPaths.push_back(entry.path().string());
                }
            }
        } else if (fs::is_regular_file(path)) {
            std::string ext = fs::path(path).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".jpg" || ext == ".png" || ext == ".jpeg" || ext == ".webp") g_PendingImportPaths.push_back(path);
        }
    } catch (...) {}
}

void finalizePendingImports() {
    PushHistoryState();
    for (const auto& path : g_PendingImportPaths) {
        DiecastCar car; std::string filename = fs::path(path).filename().string();
        parseCarInfo(filename, car); car.folderPath = path;
        applyOfflineSpecsAndTrivia(car); g_Catalog.push_back(car);
    }
    g_PendingImportPaths.clear(); g_SelectedCarIndex = (int)g_Catalog.size() - 1; g_UnsavedChanges = true;
}

void ApplyTheme(Theme theme) {
    ImGuiStyle& style = ImGui::GetStyle(); ImVec4* colors = style.Colors;
    style.WindowRounding = 16.0f; style.ChildRounding = 12.0f; style.FrameRounding = 10.0f;
    style.GrabRounding = 10.0f; style.PopupRounding = 10.0f; style.ScrollbarRounding = 10.0f;
    style.WindowBorderSize = 1.0f; style.ChildBorderSize = 1.0f; style.FrameBorderSize = 0.0f;
    if (theme == THEME_LIGHT) {
        colors[ImGuiCol_Text] = ImVec4(0.24f, 0.29f, 0.37f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.88f, 0.90f, 0.93f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.88f, 0.90f, 0.93f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.64f, 0.69f, 0.78f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.82f, 0.85f, 0.88f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.77f, 0.80f, 0.84f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.72f, 0.75f, 0.80f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.84f, 0.86f, 0.90f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.48f, 1.00f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.85f, 0.87f, 0.91f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.78f, 0.81f, 0.86f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.48f, 1.00f, 1.00f);
    } else if (theme == THEME_NAVY) {
        colors[ImGuiCol_Text] = ImVec4(0.85f, 0.92f, 0.98f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.08f, 0.12f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.11f, 0.16f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.12f, 0.22f, 0.38f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.07f, 0.10f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.18f, 0.28f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.26f, 0.40f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.12f, 0.20f, 0.32f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.28f, 0.45f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.60f, 0.95f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.12f, 0.20f, 0.32f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.28f, 0.45f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.60f, 0.95f, 1.00f);
    } else if (theme == THEME_BEIGE) {
        colors[ImGuiCol_Text] = ImVec4(0.18f, 0.13f, 0.09f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.93f, 0.89f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.99f, 0.98f, 0.96f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.85f, 0.80f, 0.75f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.92f, 0.90f, 0.87f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.87f, 0.84f, 0.81f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.81f, 0.78f, 0.75f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.89f, 0.85f, 0.80f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.81f, 0.77f, 0.72f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.31f, 0.23f, 0.19f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.91f, 0.87f, 0.82f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.84f, 0.79f, 0.74f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.23f, 0.19f, 1.00f);
    }
}

void glfw_window_close_callback(GLFWwindow* window) {
    if (g_UnsavedChanges) { glfwSetWindowShouldClose(window, GLFW_FALSE); g_ShowExitPrompt = true; }
}

void glfw_drop_callback(GLFWwindow* window, int count, const char** paths) {
    g_PendingImportPaths.clear();
    for (int i = 0; i < count; ++i) scanAndQueuePath(paths[i]);
    if (!g_PendingImportPaths.empty()) g_ShowImportConfirmPrompt = true;
}

std::string getGeminiChatResponse(const std::string& question, const DiecastCar& car) {
    std::string key = g_ApiKeyInput; std::string carDesc = car.make + " " + car.model;
    if (key.empty()) {
        std::string q = question; std::transform(q.begin(), q.end(), q.begin(), ::tolower);
        if (q.find("engine") != std::string::npos || q.find("v8") != std::string::npos) return "The Grandview features a massive 16.2-liter naturally aspirated EF700 V8 diesel engine.";
        if (q.find("collect") != std::string::npos || q.find("score") != std::string::npos) return "This Hino bus scores a 5.15/10. Because of strict emission cuts, almost no real operational examples exist today.";
        return "That is an excellent point about the Hino Grandview! Is there any specific mechanical spec you would like to go over?";
    }
    std::string host = "generativelanguage.googleapis.com", path = "/v1beta/models/gemini-2.5-flash:generateContent?key=" + key;
    json payload = {{"contents", {{{"parts", {{{"text", "You are an expert collector bot. Respond to this user question under 100 words about the vehicle " + carDesc + ": " + question}}}}}}}};
    std::string rawRes = makeHttpsPostRequest(host, path, payload.dump());
    try { auto j = json::parse(rawRes); return j["candidates"][0]["content"]["parts"][0]["text"].get<std::string>(); }
    catch (...) { return "Failed to establish secure connections with Gemini. Returning local response instead."; }
}

void DrawNeumorphicProgressBar(float fraction, const ImVec2& size, ImU32 barBgColor, ImU32 fillColor) {
    ImDrawList* drawList = ImGui::GetWindowDrawList(); ImVec2 pos = ImGui::GetCursorScreenPos();
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), barBgColor, size.y / 2.0f);
    if (fraction > 0.01f) {
        float w = size.x * fraction; drawList->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + size.y), fillColor, size.y / 2.0f);
    }
    ImGui::Dummy(size);
}

int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(1440, 800, "My Collection - Diecast Photo Catalogue", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window); glfwSwapInterval(1);
    glfwSetWindowCloseCallback(window, glfw_window_close_callback); glfwSetDropCallback(window, glfw_drop_callback);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ApplyTheme(g_ActiveTheme);
    ImGui::GetIO().FontGlobalScale = 1.30f;
    ImGui_ImplGlfw_InitForOpenGL(window, true); ImGui_ImplOpenGL3_Init("#version 130");

    std::vector<std::string> mockFiles = {
        "1-Hino Grandview Bus (Travel).JPG", "1-Mits Pajero Safari.JPG", "1-Mits Fuso Bus.JPG",
        "10-Lot Super GT.JPG", "10-Sub Sambar.JPG", "10-Sub Sambar Truck.JPG",
        "2-Mits Canter.JPG", "2-Subaru WRX.JPG", "2-Tadano Crane.JPG", "3-Anime Delivery.JPG", "3-Toyo Dyna.JPG"
    };
    for (const auto& name : mockFiles) {
        DiecastCar car; parseCarInfo(name, car); applyOfflineSpecsAndTrivia(car); g_Catalog.push_back(car);
    }
    g_SelectedCarIndex = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (g_GachaRolling && !g_Catalog.empty()) {
            g_GachaTimer += 0.016f; g_GachaDuration += 0.016f;
            if (g_GachaTimer >= g_GachaInterval) {
                g_GachaTimer = 0.0f; g_SelectedCarIndex = (g_SelectedCarIndex + 1) % g_Catalog.size(); g_GachaInterval += 0.015f;
            }
            if (g_GachaDuration >= 2.0f) { g_GachaRolling = false; g_GachaInterval = 0.05f; g_SelectedCarIndex = std::rand() % g_Catalog.size(); }
        }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        int width, height; glfwGetWindowSize(window, &width, &height);
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
        ImGui::Begin("Workspace", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // Topbar
        ImGui::BeginChild("TopBar", ImVec2(0, 45), true);
        ImGui::AlignTextToFramePadding(); ImGui::TextColored(ImVec4(0.30f, 0.48f, 1.00f, 1.00f), "My Collection"); ImGui::SameLine();
        if (ImGui::Button("Save")) exportWorkspace("workspace.zip"); ImGui::SameLine();
        if (ImGui::Button("Undo")) ExecuteUndo(); ImGui::SameLine();
        if (ImGui::Button("Redo")) ExecuteRedo(); ImGui::SameLine();
        ImGui::TextDisabled("|"); ImGui::SameLine();
        ImGui::Text("Total: %zu", g_Catalog.size()); ImGui::SameLine();
        ImGui::TextDisabled("|"); ImGui::SameLine();
        if (ImGui::Button("Gacha Spin")) {
            if (!g_GachaRolling) { g_GachaRolling = true; g_GachaTimer = 0.0f; g_GachaInterval = 0.05f; g_GachaDuration = 0.0f; }
        }
        ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine();
        ImGui::Text("%s", g_SoundscapeNames[g_ActiveSoundscape]); ImGui::SameLine();
        if (ImGui::Button("Toggle Ambient")) g_ActiveSoundscape = (g_ActiveSoundscape + 1) % 4; ImGui::SameLine();
        ImGui::TextDisabled("|"); ImGui::SameLine();
        if (ImGui::Button("Theme")) { g_ActiveTheme = static_cast<Theme>((g_ActiveTheme + 1) % 3); ApplyTheme(g_ActiveTheme); }
        ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine();
        if (ImGui::Button("Export ZIP")) exportWorkspace("workspace.zip");
        ImGui::EndChild();

        // 20% Left panel, 50% Middle panel, 30% Right panel width
        float leftPanelWidth = (float)width * 0.20f, middlePanelWidth = (float)width * 0.50f, rightPanelWidth = (float)width * 0.30f, bodyHeight = (float)height - 65.0f;

        // Panel 1: Left (20%)
        ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth - 10.0f, bodyHeight), true);
        ImGui::Text("Ingestion / Photo Banks:");
        if (ImGui::Button("File Ingest")) { g_PendingImportPaths = { "12-Tomica Subaru Sambar.JPG" }; g_ShowImportConfirmPrompt = true; } ImGui::SameLine();
        if (ImGui::Button("Folder Ingest")) { g_PendingImportPaths = { "13-Crane.JPG", "14-Toyota.JPG" }; g_ShowImportConfirmPrompt = true; }
        ImGui::Separator();
        ImGui::InputText("##Search", g_SearchInput, IM_ARRAYSIZE(g_SearchInput)); ImGui::SameLine(); ImGui::Button("Filter");
        ImGui::Text("Selection Queue:");
        if (ImGui::Button("Select All")) { PushHistoryState(); for (auto& car : g_Catalog) car.selected = true; g_UnsavedChanges = true; } ImGui::SameLine();
        if (ImGui::Button("Deselect All")) { PushHistoryState(); for (auto& car : g_Catalog) car.selected = false; g_UnsavedChanges = true; } ImGui::SameLine();
        if (ImGui::Button("Unload")) {
            PushHistoryState();
            g_Catalog.erase(std::remove_if(g_Catalog.begin(), g_Catalog.end(), [](const DiecastCar& c) { return c.selected; }), g_Catalog.end());
            g_SelectedCarIndex = g_Catalog.empty() ? -1 : 0; g_UnsavedChanges = true;
        }
        ImGui::Separator();
        ImGui::BeginChild("IndexedGrid", ImVec2(0, bodyHeight - 180.0f), false);
        for (int i = 0; i < (int)g_Catalog.size(); ++i) {
            auto& car = g_Catalog[i];
            if (strlen(g_SearchInput) > 0) {
                std::string matchQuery = car.filename; std::transform(matchQuery.begin(), matchQuery.end(), matchQuery.begin(), ::tolower);
                std::string searchStr = g_SearchInput; std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                if (matchQuery.find(searchStr) == std::string::npos) continue;
            }
            ImGui::PushID(i);
            bool isSelected = (g_SelectedCarIndex == i);
            if (isSelected) ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            else ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
            
            ImGui::BeginChild((std::string("CardGridChild##") + std::to_string(i)).c_str(), ImVec2(105, 110), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::Checkbox("##SelectCheck", &car.selected); ImGui::SameLine();
            if (ImGui::Button(car.favorite ? "★" : "☆")) {
                PushHistoryState(); car.favorite = !car.favorite; g_StarredCount += car.favorite ? 1 : -1; g_UnsavedChanges = true;
            }
            GLuint thumbID = GetOrCreateTexture(car.folderPath);
            if (thumbID != 0) ImGui::Image((void*)(intptr_t)thumbID, ImVec2(80, 45));
            else ImGui::Text("   🚗   ");

            std::string shortName = car.make + " " + car.model;
            if (shortName.length() > 10) shortName = shortName.substr(0, 8) + "..";
            ImGui::TextWrapped("%s", shortName.c_str());
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) g_SelectedCarIndex = i;
            ImGui::EndChild(); ImGui::PopStyleColor(); ImGui::PopID();
            if ((i + 1) % 2 != 0) ImGui::SameLine(); // Two cards per row in 20% width panel
        }
        ImGui::EndChild();
        ImGui::Button("Re-link root directory");
        ImGui::EndChild();

        ImGui::SameLine();

        // Panel 2: Middle (50% Front and Centre focus)
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
                            std::string ans = getGeminiChatResponse(q, car); g_ChatLog.push_back({ "Bot", ans });
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

                    drawList->AddRectFilled(canvasCursor, ImVec2(canvasCursor.x + frameSize.x, canvasCursor.y + frameSize.y), IM_COL32(210, 215, 224, 255));
                    if (g_ActiveTheme == THEME_NAVY) drawList->AddRectFilled(canvasCursor, ImVec2(canvasCursor.x + frameSize.x, canvasCursor.y + frameSize.y), IM_COL32(15, 23, 42, 255));
                    else if (g_ActiveTheme == THEME_BEIGE) drawList->AddRectFilled(canvasCursor, ImVec2(canvasCursor.x + frameSize.x, canvasCursor.y + frameSize.y), IM_COL32(210, 205, 198, 255));
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
                        ImVec2 textPos = ImVec2(canvasCursor.x + (frameSize.x / 4.0f), canvasCursor.y + (frameSize.y / 2.0f) - 10.0f);
                        drawList->AddText(textPos, IM_COL32(180, 180, 180, 255), "[ INVALID PHOTO / PATH UNRESOLVED ]");
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

            if (ImGui::BeginTabItem("Polaroid Scatter")) {
                ImGui::Text("Tabletop Desk - Drag and organize your collection scatter cards!");
                if (ImGui::Button("Toss/Scatter Cards")) {
                    g_PolaroidCards.clear();
                    for (int i = 0; i < (int)g_Catalog.size(); ++i) {
                        PolaroidCard pc; pc.carIndex = i;
                        pc.position = ImVec2(50.0f + (std::rand() % 200), 80.0f + (std::rand() % 150));
                        pc.rotation = (float)(std::rand() % 30 - 15); pc.initialized = true;
                        g_PolaroidCards.push_back(pc);
                    }
                }
                ImGui::Separator();
                ImVec2 scatterCursor = ImGui::GetCursorScreenPos();
                ImDrawList* dlist = ImGui::GetWindowDrawList();
                dlist->AddRectFilled(scatterCursor, ImVec2(scatterCursor.x + middlePanelWidth - 30.0f, scatterCursor.y + bodyHeight - 120.0f), IM_COL32(10, 14, 20, 255));

                ImGui::BeginChild("ScatterStage", ImVec2(0, bodyHeight - 120.0f), false, ImGuiWindowFlags_NoScrollbar);
                for (size_t i = 0; i < g_PolaroidCards.size(); ++i) {
                    auto& pc = g_PolaroidCards[i]; if (!pc.initialized) continue;
                    ImGui::SetNextWindowPos(ImVec2(scatterCursor.x + pc.position.x, scatterCursor.y + pc.position.y), ImGuiCond_Appearing);
                    ImGui::SetNextWindowSize(ImVec2(100, 110));
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::Begin((std::string("CardWindow##") + std::to_string(i)).c_str(), NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
                    ImGui::Text("%s", g_Catalog[pc.carIndex].make.c_str());
                    ImGui::Separator();
                    ImGui::Text("Scale: %s", g_Catalog[pc.carIndex].scale.c_str());
                    ImVec2 wpos = ImGui::GetWindowPos(); pc.position = ImVec2(wpos.x - scatterCursor.x, wpos.y - scatterCursor.y);
                    ImGui::End(); ImGui::PopID();
                }
                ImGui::EndChild(); ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Curator Showdown")) {
                ImGui::Text("Showdown Challenge! Crown today's active showroom winner.");
                if (ImGui::Button("Deal Versus Matchup") && g_Catalog.size() >= 2) {
                    g_ShowdownLeftIndex = std::rand() % g_Catalog.size(); g_ShowdownRightIndex = std::rand() % g_Catalog.size();
                    while (g_ShowdownLeftIndex == g_ShowdownRightIndex) g_ShowdownRightIndex = std::rand() % g_Catalog.size();
                    g_ShowdownActive = true;
                }
                if (g_ShowdownActive && g_ShowdownLeftIndex != -1 && g_ShowdownRightIndex != -1) {
                    ImGui::Separator();
                    ImGui::BeginChild("LeftShowdownCard", ImVec2((middlePanelWidth / 2.0f) - 20.0f, 200), true);
                    auto& carL = g_Catalog[g_ShowdownLeftIndex];
                    ImGui::Text("%d %s", carL.year, carL.make.c_str()); ImGui::TextWrapped("%s", carL.model.c_str()); ImGui::Separator();
                    ImGui::Text("Record: %d Wins", carL.showdownWins);
                    if (ImGui::Button("Select Left")) {
                        PushHistoryState(); carL.showdownWins++; carL.showdownBattles++;
                        g_Catalog[g_ShowdownRightIndex].showdownBattles++;
                        g_ShowdownActive = false; g_UnsavedChanges = true;
                    }
                    ImGui::EndChild(); ImGui::SameLine(); ImGui::AlignTextToFramePadding(); ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), " ⚔ VS "); ImGui::SameLine();
                    ImGui::BeginChild("RightShowdownCard", ImVec2(0, 200), true);
                    auto& carR = g_Catalog[g_ShowdownRightIndex];
                    ImGui::Text("%d %s", carR.year, carR.make.c_str()); ImGui::TextWrapped("%s", carR.model.c_str()); ImGui::Separator();
                    ImGui::Text("Record: %d Wins", carR.showdownWins);
                    if (ImGui::Button("Select Right")) {
                        PushHistoryState(); carR.showdownWins++; carR.showdownBattles++;
                        g_Catalog[g_ShowdownLeftIndex].showdownBattles++;
                        g_ShowdownActive = false; g_UnsavedChanges = true;
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

        // Panel 3: Right (30%)
        ImGui::BeginChild("RightPanel", ImVec2(0, bodyHeight), true);
        if (g_SelectedCarIndex >= 0 && g_SelectedCarIndex < (int)g_Catalog.size()) {
            auto& car = g_Catalog[g_SelectedCarIndex];
            if (ImGui::BeginTabBar("RightHistologyTabs")) {
                if (ImGui::BeginTabItem("Real History")) {
                    ImGui::TextWrapped("%s", car.realHistory.c_str()); ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Factory Specs")) {
                    ImGui::TextWrapped("%s", car.factorySpecs.c_str()); ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Nuggets")) {
                    ImGui::Text("REAL CAR COLLECTABILITY:");
                    ImGui::TextColored(ImVec4(0.30f, 0.48f, 1.00f, 1.00f), "Score: %.2f / 10.00", car.collectabilityScore);
                    ImU32 sunkenTrack = IM_COL32(210, 215, 224, 255); ImU32 pillFill = IM_COL32(77, 124, 255, 255);
                    if (g_ActiveTheme == THEME_NAVY) {
                        sunkenTrack = IM_COL32(15, 23, 42, 255); pillFill = IM_COL32(0, 240, 255, 255);
                    } else if (g_ActiveTheme == THEME_BEIGE) {
                        sunkenTrack = IM_COL32(210, 205, 198, 255); pillFill = IM_COL32(184, 115, 51, 255);
                    }
                    DrawNeumorphicProgressBar(car.collectabilityScore / 10.0f, ImVec2(rightPanelWidth - 50.0f, 14.0f), sunkenTrack, pillFill);
                    ImGui::Separator(); ImGui::Text("NOTABLE TRIVIA NUGGETS:");
                    for (size_t n = 0; n < car.triviaNuggets.size(); ++n) {
                        ImGui::BeginChild((std::string("TriviaBoxChild##") + std::to_string(n)).c_str(), ImVec2(0, 85), true);
                        ImGui::TextWrapped("%s", car.triviaNuggets[n].c_str()); ImGui::EndChild();
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
            ImGui::Separator(); ImGui::Text("Collector Chat Memory Log (💬):");
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
        if (g_ShowImportConfirmPrompt) ImGui::OpenPopup("Confirm Import Queue?");
        if (ImGui::BeginPopupModal("Confirm Import Queue?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Confirm ingestion queue? You are about to import %zu file(s) into your current workspace catalog.", g_PendingImportPaths.size());
            ImGui::Spacing();
            if (ImGui::Button("Confirm Ingest", ImVec2(140, 0))) {
                finalizePendingImports(); g_ShowImportConfirmPrompt = false; ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                g_PendingImportPaths.clear(); g_ShowImportConfirmPrompt = false; ImGui::CloseCurrentPopup();
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
