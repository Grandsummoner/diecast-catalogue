#pragma once

#include <string>
#include <vector>
#include "types.hpp"

// GLFW OS-level callbacks
void glfw_window_close_callback(GLFWwindow* window);
void glfw_drop_callback(GLFWwindow* window, int count, const char** paths);

// Texture Cache & Loading
GLuint GetOrCreateTexture(const std::string& path);

// String helpers
std::wstring toWString(const std::string& str);

// WinHTTP Client wrapper
std::string makeHttpsPostRequest(const std::string& host, const std::string& path, const std::string& payload);

// Gemini Chatbot proxies
std::string getGeminiChatResponse(const std::string& question, const DiecastCar& car);

// Offline Heuristic Engine
float calculateScore(const std::string& make, const std::string& model, int year);
void applyOfflineSpecsAndTrivia(DiecastCar& car);
void parseCarInfo(const std::string& filename, DiecastCar& outCar);

// Undo/Redo Stack Machine
void PushHistoryState();
void ExecuteUndo();
void ExecuteRedo();

// Workspace miniz file exporters/importers
bool exportWorkspace(const std::string& zipPath);
bool importWorkspace(const std::string& zipPath);

// Drag-and-drop directory scanning triggers
void scanAndQueuePath(const std::string& path);
void finalizePendingImports();