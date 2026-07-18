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
