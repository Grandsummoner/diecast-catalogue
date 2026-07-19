#pragma once

#include "types.hpp"

// Themes Apply Styles
void ApplyTheme(Theme theme);

// Neumorphic progress bar
void DrawNeumorphicProgressBar(float fraction, const ImVec2& size, ImU32 barBgColor, ImU32 fillColor);

// Custom color helpers matching Image 2 specifications
ImVec4 GetThemeClearColor(Theme theme);
ImU32 GetThemeSunkenTrackColor(Theme theme);
ImU32 GetThemePillFillColor(Theme theme);