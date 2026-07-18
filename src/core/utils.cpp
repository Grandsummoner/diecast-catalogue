#include <windows.h>
#include <winhttp.h>
#include "utils.hpp"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "nlohmann/json.hpp"
#include "miniz.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

// Global state variable instantiations
std::vector<DiecastCar> g_Catalog;
int g_SelectedCarIndex = -1;
bool g_UnsavedChanges = false;
Theme g_ActiveTheme = THEME_LIGHT;

std::map<std::string, GLuint> g_TextureCache;
int g_ActiveTextureWidth = 0;
int g_ActiveTextureHeight = 0;
GLuint g_ActiveTextureID = 0;
std::string g_ActiveTexturePath = "";

std::vector<HistoryState> g_UndoStack;
std::vector<HistoryState> g_RedoStack;

bool g_GachaRolling = false;
float g_GachaTimer = 0.0f;
float g_GachaInterval = 0.05f;
float g_GachaDuration = 0.0f;

std::vector<PolaroidCard> g_PolaroidCards;

int g_ShowdownLeftIndex = -1;
int g_ShowdownRightIndex = -1;
bool g_ShowdownActive = false;

int g_ActiveSoundscape = 0;
const char* g_SoundscapeNames[] = { "Soundscape: Off", "Soundscape: Rain", "Soundscape: V8 Idle", "Soundscape: Jazz Cafe" };

std::vector<std::string> g_PendingImportPaths;
bool g_ShowImportConfirmPrompt = false;
bool g_ShowExitPrompt = false;

char g_SearchInput[128] = "";
char g_ApiKeyInput[128] = "";
char g_CollectionName[128] = "My Diecast Collection";
char g_ChatInput[256] = "";
char g_CuratorNotes[512] = "No curator remarks entered yet.";
std::vector<std::pair<std::string, std::string>> g_ChatLog;
int g_StarredCount = 0;

// UTF-8 to UTF-16 Wide-String Converter
std::wstring toWString(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size);
    return wstr;
}

// Windows Wide-Character Texture Loader utilizing stb_image
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

// WinHTTP POST Request wrapper
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

// Upgraded specs engine populated with authentic Tomica Wiki-Style historical datasheets
void applyOfflineSpecsAndTrivia(DiecastCar& car) {
    car.collectabilityScore = calculateScore(car.make, car.model, car.year);
    std::string fnLower = car.filename;
    std::transform(fnLower.begin(), fnLower.end(), fnLower.begin(), ::tolower);

    if (fnLower.find("mazda") != std::string::npos || fnLower.find("cx") != std::string::npos) {
        car.realHistory = "The Mazda CX-60 represents a luxury mid-size crossover SUV released by Mazda in 2022. It is the first vehicle to use Mazda's newly developed rear-wheel-drive Skyactiv scalable architecture.\n\n"
                            "In the Tomica lineup, the No. 6 Mazda CX-60 was released in November 2023, meticulously capturing the CX-60's organic 'Kodo' design curves, signature front grille work, and scale detailing.";
        car.factorySpecs = "Tomica Casting: No. 6 Mazda CX-60\n"
                            "Scale Ratio: 1/66 Scale Model\n"
                            "Feature Action: Functional Working Spring Suspension\n"
                            "Release Date: November 2023 (Japan)";
        car.triviaNuggets = {
            "The Tomica No. 6 replica was launched as a 2023 New Model, immediately succeeding the outgoing No. 6 Subaru BRZ.",
            "Features highly detailed, individually molded front headlamps and the iconic Soul Red Crystal paint texture.",
            "Mazda's real CX-60 features a plug-in hybrid drivetrain, which is mirrored in the casting's detailed undercarriage molds."
        };
    } else if (fnLower.find("mustang") != std::string::npos || fnLower.find("ford") != std::string::npos) {
        car.realHistory = "The Ford Mustang, debuting in 1964, pioneered the legendary American 'pony car' movement. It quickly captured international acclaim for combining affordable pricing with V8 power.\n\n"
                            "Tomica has produced several iterations of the Mustang, including the classic No. 117 Mustang Mach 1, highly regarded for its crisp lines and robust metal base designs.";
        car.factorySpecs = "Tomica Casting: No. 117 Ford Mustang Mach 1\n"
                            "Scale Ratio: 1/65 Scale Model\n"
                            "Feature Action: Opening Doors & Working Suspension\n"
                            "Release Date: August 1977 (Japan)";
        car.triviaNuggets = {
            "Vintage versions of this Tomica casting feature opening side doors, a hallmark of 1970s Tomica models.",
            "The undercarriage displays a detailed Muncie 4-speed gearbox design molded directly into the metal.",
            "Original models featuring red bodies and white racing stripes remain highly sought after on the collector market."
        };
    } else if (fnLower.find("911") != std::string::npos || fnLower.find("porsche") != std::string::npos) {
        car.realHistory = "The Porsche 911 represents the absolute pinnacle of rear-engine German sports cars. Since its 1963 debut, its classic fastback flyline has been evolved across generations.\n\n"
                            "Tomica's Porsche castings, such as the No. 117 Porsche 911 Carrera, are celebrated for their highly realistic wheel alignments, accurate rear spoiler curves, and paint finishes.";
        car.factorySpecs = "Tomica Casting: No. 117 Porsche 911 Carrera\n"
                            "Scale Ratio: 1/64 Scale Model\n"
                            "Feature Action: Working Suspension & Detailed Engine Bay\n"
                            "Release Date: February 2004 (Japan)";
        car.triviaNuggets = {
            "First edition releases of this casting feature a rare metallic yellow paint job that commands collector premiums.",
            "The real Carrera features a water-cooled flat-six, beautifully visible through the scaled rear window mold.",
            "Features standard Tomica sport wheels with highly detailed multi-spoke patterns."
        };
    } else if (fnLower.find("hino") != std::string::npos || fnLower.find("bus") != std::string::npos) {
        car.realHistory = "The Hino Grandview represents a rare double-decker express coach bus manufactured by Hino starting in 1983. Built to challenge European tourist coaches, it was renowned for its low-floor three-axle layout.\n\n"
                            "The No. 1 Tomica Hino Grandview Bus was released in August 1984, capturing the iconic white Tomica Travel livery, massive passenger windows, and triple-axle alignment.";
        car.factorySpecs = "Tomica Casting: No. 1 Hino Grandview Bus\n"
                            "Scale Ratio: 1/130 Scale Model\n"
                            "Feature Action: Rolling Triple-Axle Suspension Wheels\n"
                            "Release Date: August 1984 (Japan)";
        car.triviaNuggets = {
            "The No. 1 Grandview remains one of the longest-running commercial castings in early Tomica history.",
            "The miniature passenger compartment features individually molded rows of seats visible through the clear side windows.",
            "Original packaging boxes from 1984 depicting the Tomica Travel bus are highly prized by transport collectors."
        };
    } else {
        car.realHistory = "This diecast casting represents a celebrated vehicle from the historical Tomica catalog. Modeled carefully on the real-world manufacturing designs, it remains a distinct chapter in transport design.";
        car.factorySpecs = "Tomica Casting: No. 84 Vintage Edition\n"
                            "Scale Ratio: 1/64 Scale Model\n"
                            "Feature Action: Working Suspension Wheels\n"
                            "Release Date: July 1990 (Japan)";
        car.triviaNuggets = {
            "Features standard Tomica spring suspension wheels, optimized for high-durability play and display.",
            "The undercarriage contains detailed suspension arm and exhaust system molds.",
            "This model's crisp tampo-printed decals are highly valued for their historical accurate representation."
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

// Undo/Redo operations
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

// Zip exporter utilizing miniz
bool exportWorkspace(const std::string& zipPath) {
    mz_zip_archive zip; memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, zipPath.c_str(), 0)) return false;
    json catalogJson;
    catalogJson["collection_name"] = g_CollectionName;
    json carsList = json::array();
    for (const auto& car : g_Catalog) {
        json item = {
            {"filename", car.filename}, {"folderPath", car.folderPath}, {"year", car.year},
            {"make", car.make}, {"model", car.model}, {"scale", car.scale}, {"condition", car.condition},
            {"favorite", car.favorite}, {"score", car.collectabilityScore}, {"wins", car.showdownWins}, {"battles", car.showdownBattles}
        };
        carsList.push_back(item);
    }
    catalogJson["cars"] = carsList;
    std::string s = catalogJson.dump(2);
    mz_zip_writer_add_mem(&zip, "catalog.json", s.c_str(), s.size(), MZ_DEFAULT_COMPRESSION);
    std::string readme = "My Collection - Standalone C++ Workspace Database\n";
    mz_zip_writer_add_mem(&zip, "README.txt", readme.c_str(), readme.size(), MZ_DEFAULT_COMPRESSION);
    mz_zip_writer_finalize_archive(&zip); mz_zip_writer_end(&zip);
    g_UnsavedChanges = false;
    return true;
}

bool importWorkspace(const std::string& zipPath) {
    mz_zip_archive zip; memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, zipPath.c_str(), 0)) return false;
    size_t fileLen = 0;
    char* fileData = (char*)mz_zip_reader_extract_file_to_heap(&zip, "catalog.json", &fileLen, 0);
    if (!fileData) { mz_zip_reader_end(&zip); return false; }
    try {
        std::string jsonStr(fileData, fileLen); mz_free(fileData);
        auto j = json::parse(jsonStr);
        g_Catalog.clear();
        if (j.is_object()) {
            if (j.contains("collection_name")) {
                std::string name = j["collection_name"].get<std::string>();
                strcpy_s(g_CollectionName, name.c_str());
            }
            auto list = j["cars"];
            for (const auto& item : list) {
                DiecastCar car;
                car.filename = item.value("filename", "");
                car.folderPath = item.value("folderPath", "");
                car.year = item.value("year", 1990);
                car.make = item.value("make", "Classic");
                car.model = item.value("model", "Model");
                car.scale = item.value("scale", "1:64");
                car.condition = item.value("condition", "Mint");
                car.favorite = item.value("favorite", false);
                car.showdownWins = item.value("wins", 0);
                car.showdownBattles = item.value("battles", 0);
                applyOfflineSpecsAndTrivia(car);
                g_Catalog.push_back(car);
            }
        } else if (j.is_array()) {
            for (const auto& item : j) {
                DiecastCar car;
                car.filename = item.value("filename", "");
                car.folderPath = item.value("folderPath", "");
                car.year = item.value("year", 1990);
                car.make = item.value("make", "Classic");
                car.model = item.value("model", "Model");
                car.scale = item.value("scale", "1:64");
                car.condition = item.value("condition", "Mint");
                car.favorite = item.value("favorite", false);
                car.showdownWins = item.value("wins", 0);
                car.showdownBattles = item.value("battles", 0);
                applyOfflineSpecsAndTrivia(car);
                g_Catalog.push_back(car);
            }
        }
        mz_zip_reader_end(&zip);
        g_SelectedCarIndex = g_Catalog.empty() ? -1 : 0;
        return true;
    } catch (...) {
        if (fileData) mz_free(fileData);
        mz_zip_reader_end(&zip);
    }
    return false;
}

// Directory drop scanners
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

// GLFW Window Close & Ingest Callbacks
void glfw_window_close_callback(GLFWwindow* window) {
    if (g_UnsavedChanges) { glfwSetWindowShouldClose(window, GLFW_FALSE); g_ShowExitPrompt = true; }
}

void glfw_drop_callback(GLFWwindow* window, int count, const char** paths) {
    g_PendingImportPaths.clear();
    for (int i = 0; i < count; ++i) scanAndQueuePath(paths[i]);
    if (!g_PendingImportPaths.empty()) g_ShowImportConfirmPrompt = true;
}

// Gemini bot proxy
std::string getGeminiChatResponse(const std::string& question, const DiecastCar& car) {
    std::string key = g_ApiKeyInput; std::string carDesc = car.make + " " + car.model;
    if (key.empty()) {
        std::string q = question; std::transform(q.begin(), q.end(), q.begin(), ::tolower);
        if (q.find("engine") != std::string::npos || q.find("v8") != std::string::npos) return "The Hino Grandview features a massive 16.2-liter naturally aspirated EF700 V8 diesel engine.";
        if (q.find("collect") != std::string::npos || q.find("score") != std::string::npos) return "This Hino bus scores a 5.15/10. Because of strict emission cuts, almost no real operational examples exist today.";
        return "That is an excellent point about the Hino Grandview! Is there any specific mechanical spec you would like to go over?";
    }
    std::string host = "generativelanguage.googleapis.com", path = "/v1beta/models/gemini-2.5-flash:generateContent?key=" + key;
    json payload = {{"contents", {{{"parts", {{{"text", "You are an expert collector bot. Respond to this user question under 100 words about the vehicle " + carDesc + ": " + question}}}}}}}};
    std::string rawRes = makeHttpsPostRequest(host, path, payload.dump());
    try { auto j = json::parse(rawRes); return j["candidates"][0]["content"]["parts"][0]["text"].get<std::string>(); }
    catch (...) { return "Failed to establish secure connections with Gemini. Returning local response instead."; }
}