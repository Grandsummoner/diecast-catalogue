#pragma once

#include <string>
#include <vector>
#include <map>
#include "imgui.h"

// Let GLFW include standard GL and Win32 headers in correct order
#include "GLFW/glfw3.h"

struct DiecastCar {
    std::string filename;
    std::string displayName;
    std::string folderPath;
    int year = 1990;
    std::string make = "Classic";
    std::string model = "Model";
    std::string color = "Paint";
    std::string scale = "1:64";
    std::string condition = "Mint";
    bool favorite = false;
    bool selected = false;
    std::string tags = "01-10";
    int showdownWins = 0;
    int showdownBattles = 0;
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
    float rotation = 0.0f;
    bool initialized = false;
};

// Unified color scheme bindings derived from Image 2
enum Theme { 
    THEME_BLUE, 
    THEME_GREEN, 
    THEME_PURPLE, 
    THEME_GRAY, 
    THEME_ORANGE, 
    THEME_RED, 
    THEME_PINK, 
    THEME_YELLOW 
};

// Extern global state declarations to ensure proper compilation linkage
extern std::vector<DiecastCar> g_Catalog;
extern int g_SelectedCarIndex;
extern bool g_UnsavedChanges;
extern Theme g_ActiveTheme;
extern char g_SearchInput[128];
extern char g_ApiKeyInput[128];
extern char g_CollectionName[128]; // User-editable collection name

extern std::map<std::string, GLuint> g_TextureCache;
extern int g_ActiveTextureWidth;
extern int g_ActiveTextureHeight;
extern GLuint g_ActiveTextureID;
extern std::string g_ActiveTexturePath;

extern std::vector<HistoryState> g_UndoStack;
extern std::vector<HistoryState> g_RedoStack;

extern bool g_GachaRolling;
extern float g_GachaTimer;
extern float g_GachaInterval;
extern float g_GachaDuration;

extern std::vector<PolaroidCard> g_PolaroidCards;
extern int g_DraggedPolaroidIndex; // Click-Lock Drag State tracker

// Tinder Faceoff Gameplay States
extern int g_ShowdownLeftIndex;
extern int g_ShowdownRightIndex;
extern bool g_ShowdownActive;
extern float g_FaceoffFlashTimer;
extern int g_ActiveQuestionIndex;
extern const char* g_FaceoffQuestions[];

extern int g_ActiveSoundscape;
extern const char* g_SoundscapeNames[];

extern std::vector<std::string> g_PendingImportPaths;
extern bool g_ShowUploadConfirmPrompt; // Scrubbed "ingest"
extern bool g_ShowExitPrompt;

extern char g_ChatInput[256];
extern char g_CuratorNotes[512];
extern std::vector<std::pair<std::string, std::string>> g_ChatLog;
extern int g_StarredCount;

extern float g_GlobalScale; // Ctrl + Scroll Wheel Zoom factor