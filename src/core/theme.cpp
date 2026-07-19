#include "theme.hpp"

void ApplyTheme(Theme theme) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 16.0f;
    style.ChildRounding = 12.0f;
    style.FrameRounding = 10.0f;
    style.GrabRounding = 10.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    // Default Neumorphic Clay Base Colors (Trust Blue Style)
    ImVec4 textCol = ImVec4(0.24f, 0.29f, 0.37f, 1.00f);
    ImVec4 winBg = ImVec4(0.88f, 0.90f, 0.93f, 1.00f); // Neumorphic Clay Grey `#E0E5EC`
    ImVec4 borderCol = ImVec4(0.64f, 0.69f, 0.78f, 1.00f);
    ImVec4 frameBg = ImVec4(0.82f, 0.85f, 0.88f, 1.00f);
    ImVec4 btnCol = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
    ImVec4 activeCol = ImVec4(0.13f, 0.43f, 0.73f, 1.00f); // Default Blue

    if (theme == THEME_GREEN) {
        winBg = ImVec4(0.88f, 0.92f, 0.90f, 1.00f);
        activeCol = ImVec4(0.30f, 0.69f, 0.31f, 1.00f); // Tropicana Green
    } else if (theme == THEME_PURPLE) {
        winBg = ImVec4(0.93f, 0.88f, 0.95f, 1.00f);
        activeCol = ImVec4(0.61f, 0.15f, 0.69f, 1.00f); // Thai Purple
    } else if (theme == THEME_GRAY) {
        winBg = ImVec4(0.91f, 0.92f, 0.94f, 1.00f);
        activeCol = ImVec4(0.45f, 0.47f, 0.50f, 1.00f); // Apple Gray
    } else if (theme == THEME_ORANGE) {
        winBg = ImVec4(0.95f, 0.91f, 0.88f, 1.00f);
        activeCol = ImVec4(0.90f, 0.45f, 0.05f, 1.00f); // Amazon Orange
    } else if (theme == THEME_RED) {
        winBg = ImVec4(0.96f, 0.88f, 0.89f, 1.00f);
        activeCol = ImVec4(0.85f, 0.10f, 0.12f, 1.00f); // Coca-Cola Red
    } else if (theme == THEME_PINK) {
        winBg = ImVec4(0.96f, 0.88f, 0.92f, 1.00f);
        activeCol = ImVec4(0.91f, 0.12f, 0.39f, 1.00f); // Lyft Pink
    } else if (theme == THEME_YELLOW) {
        winBg = ImVec4(0.96f, 0.95f, 0.88f, 1.00f);
        activeCol = ImVec4(0.95f, 0.75f, 0.10f, 1.00f); // Hertz Yellow
    }

    colors[ImGuiCol_Text] = textCol;
    colors[ImGuiCol_WindowBg] = winBg;
    colors[ImGuiCol_ChildBg] = winBg;
    colors[ImGuiCol_Border] = borderCol;
    colors[ImGuiCol_FrameBg] = frameBg;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(frameBg.x - 0.05f, frameBg.y - 0.05f, frameBg.z - 0.05f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(frameBg.x - 0.10f, frameBg.y - 0.10f, frameBg.z - 0.10f, 1.00f);
    colors[ImGuiCol_Button] = btnCol;
    colors[ImGuiCol_ButtonHovered] = ImVec4(btnCol.x - 0.08f, btnCol.y - 0.07f, btnCol.z - 0.05f, 1.00f);
    colors[ImGuiCol_ButtonActive] = activeCol;
    colors[ImGuiCol_Header] = btnCol;
    colors[ImGuiCol_HeaderHovered] = ImVec4(btnCol.x - 0.08f, btnCol.y - 0.07f, btnCol.z - 0.05f, 1.00f);
    colors[ImGuiCol_HeaderActive] = activeCol;
    colors[ImGuiCol_CheckMark] = activeCol;
    colors[ImGuiCol_SliderGrab] = activeCol;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(activeCol.x + 0.05f, activeCol.y + 0.05f, activeCol.z + 0.05f, 1.00f);
}

void DrawNeumorphicProgressBar(float fraction, const ImVec2& size, ImU32 barBgColor, ImU32 fillColor) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), barBgColor, size.y / 2.0f);
    if (fraction > 0.01f) {
        float w = size.x * fraction;
        drawList->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + size.y), fillColor, size.y / 2.0f);
    }
    ImGui::Dummy(size);
}

ImVec4 GetThemeClearColor(Theme theme) {
    if (theme == THEME_GREEN) return ImVec4(0.88f, 0.92f, 0.90f, 1.00f);
    if (theme == THEME_PURPLE) return ImVec4(0.93f, 0.88f, 0.95f, 1.00f);
    if (theme == THEME_GRAY) return ImVec4(0.91f, 0.92f, 0.94f, 1.00f);
    if (theme == THEME_ORANGE) return ImVec4(0.95f, 0.91f, 0.88f, 1.00f);
    if (theme == THEME_RED) return ImVec4(0.96f, 0.88f, 0.89f, 1.00f);
    if (theme == THEME_PINK) return ImVec4(0.96f, 0.88f, 0.92f, 1.00f);
    if (theme == THEME_YELLOW) return ImVec4(0.96f, 0.95f, 0.88f, 1.00f);
    return ImVec4(0.88f, 0.90f, 0.93f, 1.00f); // Default Blue
}

ImU32 GetThemeSunkenTrackColor(Theme theme) {
    ImVec4 clearCol = GetThemeClearColor(theme);
    // Darken by 10% to create a sunken track effect
    return IM_COL32(clearCol.x * 230, clearCol.y * 230, clearCol.z * 230, 255);
}

ImU32 GetThemePillFillColor(Theme theme) {
    if (theme == THEME_GREEN) return IM_COL32(76, 175, 80, 255);     // Green
    if (theme == THEME_PURPLE) return IM_COL32(156, 39, 176, 255);   // Purple
    if (theme == THEME_GRAY) return IM_COL32(158, 158, 158, 255);     // Gray
    if (theme == THEME_ORANGE) return IM_COL32(255, 152, 0, 255);     // Orange
    if (theme == THEME_RED) return IM_COL32(244, 67, 54, 255);       // Red
    if (theme == THEME_PINK) return IM_COL32(233, 30, 99, 255);      // Pink
    if (theme == THEME_YELLOW) return IM_COL32(255, 235, 59, 255);    // Yellow
    return IM_COL32(33, 150, 243, 255);                              // Default Blue
}