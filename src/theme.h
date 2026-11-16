#pragma once

#include <imgui.h>

#include <cstdint>
#include <string>

namespace efs {

struct UiFonts {
    ImFont* regular = nullptr;
    ImFont* bold = nullptr;
    ImFont* mono = nullptr;
    ImFont* monoBig = nullptr;
};

void applyTheme(float scale, UiFonts& fonts);
ImU32 colorU32(uint32_t rgba);
ImU32 colorU32(uint32_t rgba, float alphaMultiplier);

} // namespace efs
