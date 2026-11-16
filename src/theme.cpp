#include "theme.h"

#include "platform.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>

namespace efs {

ImU32 colorU32(uint32_t rgba) {
    return colorU32(rgba, 1.0f);
}

ImU32 colorU32(uint32_t rgba, float alphaMultiplier) {
    const int r = (int)((rgba >> 24) & 0xFF);
    const int g = (int)((rgba >> 16) & 0xFF);
    const int b = (int)((rgba >> 8) & 0xFF);
    const int a = (int)std::clamp((int)std::lround(((rgba & 0xFF) / 255.0f) * alphaMultiplier * 255.0f), 0, 255);
    return IM_COL32(r, g, b, a);
}

static std::string windowsFontPath(const wchar_t* file) {
    wchar_t dir[32768] = {};
    const UINT length = GetWindowsDirectoryW(dir, (UINT)std::size(dir));
    if (length == 0 || length >= std::size(dir)) return std::string();
    std::wstring path(dir, length);
    if (!path.empty() && path.back() != L'\\') path.push_back(L'\\');
    path += L"Fonts\\";
    path += file;
    return pathToUtf8(path);
}

void applyTheme(float scale, UiFonts& fonts) {
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    ImGui::StyleColorsDark();

    auto c = [](int r, int g, int b, int a = 255) {
        return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    };

    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.WindowRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowMinSize = ImVec2(40.0f, 40.0f);
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.ChildRounding = 3.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 4.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.FrameRounding = 3.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(7.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.CellPadding = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 18.0f;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 12.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 3.0f;
    style.LogSliderDeadzone = 4.0f;
    style.TabRounding = 3.0f;
    style.TabBorderSize = 0.0f;
    style.TabBarBorderSize = 1.0f;
    style.TabBarOverlineSize = 2.0f;
    style.TabMinWidthForCloseButton = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
    style.SeparatorTextPadding = ImVec2(18.0f, 3.0f);
    style.DisplayWindowPadding = ImVec2(19.0f, 19.0f);
    style.DisplaySafeAreaPadding = ImVec2(3.0f, 3.0f);
    style.MouseCursorScale = 1.0f;
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = true;
    style.AntiAliasedFill = true;
    style.CurveTessellationTol = 1.25f;
    style.CircleTessellationMaxError = 0.30f;

    style.Colors[ImGuiCol_Text] = c(223, 226, 232);
    style.Colors[ImGuiCol_TextDisabled] = c(119, 125, 137);
    style.Colors[ImGuiCol_WindowBg] = c(23, 25, 29);
    style.Colors[ImGuiCol_ChildBg] = c(27, 30, 35);
    style.Colors[ImGuiCol_PopupBg] = c(32, 35, 41, 250);
    style.Colors[ImGuiCol_Border] = c(44, 48, 57);
    style.Colors[ImGuiCol_BorderShadow] = c(0, 0, 0, 0);
    style.Colors[ImGuiCol_FrameBg] = c(36, 40, 48);
    style.Colors[ImGuiCol_FrameBgHovered] = c(45, 50, 60);
    style.Colors[ImGuiCol_FrameBgActive] = c(52, 58, 70);
    style.Colors[ImGuiCol_TitleBg] = c(20, 22, 26);
    style.Colors[ImGuiCol_TitleBgActive] = c(27, 31, 38);
    style.Colors[ImGuiCol_TitleBgCollapsed] = c(20, 22, 26);
    style.Colors[ImGuiCol_MenuBarBg] = c(17, 19, 23);
    style.Colors[ImGuiCol_ScrollbarBg] = c(18, 20, 24);
    style.Colors[ImGuiCol_ScrollbarGrab] = c(54, 60, 71);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = c(65, 72, 85);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = c(77, 85, 100);
    style.Colors[ImGuiCol_CheckMark] = c(77, 159, 255);
    style.Colors[ImGuiCol_SliderGrab] = c(61, 139, 255);
    style.Colors[ImGuiCol_SliderGrabActive] = c(105, 166, 255);
    style.Colors[ImGuiCol_Button] = c(38, 43, 52);
    style.Colors[ImGuiCol_ButtonHovered] = c(49, 56, 70);
    style.Colors[ImGuiCol_ButtonActive] = c(58, 67, 82);
    style.Colors[ImGuiCol_Header] = c(39, 49, 63);
    style.Colors[ImGuiCol_HeaderHovered] = c(47, 61, 80);
    style.Colors[ImGuiCol_HeaderActive] = c(55, 80, 108);
    style.Colors[ImGuiCol_Separator] = c(42, 46, 54);
    style.Colors[ImGuiCol_SeparatorHovered] = c(77, 159, 255, 180);
    style.Colors[ImGuiCol_SeparatorActive] = c(77, 159, 255);
    style.Colors[ImGuiCol_ResizeGrip] = c(61, 139, 255, 60);
    style.Colors[ImGuiCol_ResizeGripHovered] = c(77, 159, 255, 160);
    style.Colors[ImGuiCol_ResizeGripActive] = c(77, 159, 255);
    style.Colors[ImGuiCol_Tab] = c(28, 32, 38);
    style.Colors[ImGuiCol_TabHovered] = c(47, 57, 71);
    style.Colors[ImGuiCol_TabSelected] = c(38, 44, 54);
    style.Colors[ImGuiCol_TabSelectedOverline] = c(61, 139, 255);
    style.Colors[ImGuiCol_TabDimmed] = c(24, 27, 32);
    style.Colors[ImGuiCol_TabDimmedSelected] = c(32, 36, 44);
    style.Colors[ImGuiCol_TabDimmedSelectedOverline] = c(61, 139, 255, 120);
    style.Colors[ImGuiCol_DockingPreview] = c(61, 139, 255, 150);
    style.Colors[ImGuiCol_DockingEmptyBg] = c(16, 18, 21);
    style.Colors[ImGuiCol_PlotLines] = c(77, 159, 255);
    style.Colors[ImGuiCol_PlotLinesHovered] = c(255, 159, 67);
    style.Colors[ImGuiCol_PlotHistogram] = c(255, 159, 67);
    style.Colors[ImGuiCol_PlotHistogramHovered] = c(255, 180, 100);
    style.Colors[ImGuiCol_TableHeaderBg] = c(30, 34, 40);
    style.Colors[ImGuiCol_TableBorderStrong] = c(49, 54, 64);
    style.Colors[ImGuiCol_TableBorderLight] = c(38, 42, 49);
    style.Colors[ImGuiCol_TableRowBg] = c(0, 0, 0, 0);
    style.Colors[ImGuiCol_TableRowBgAlt] = c(255, 255, 255, 8);
    style.Colors[ImGuiCol_TextSelectedBg] = c(48, 93, 155, 190);
    style.Colors[ImGuiCol_DragDropTarget] = c(77, 159, 255);
    style.Colors[ImGuiCol_NavCursor] = c(77, 159, 255);
    style.Colors[ImGuiCol_NavWindowingHighlight] = c(255, 255, 255, 180);
    style.Colors[ImGuiCol_NavWindowingDimBg] = c(10, 12, 15, 170);
    style.Colors[ImGuiCol_ModalWindowDimBg] = c(6, 8, 10, 190);

    style.ScaleAllSizes(scale);

    ImGuiIO& io = ImGui::GetIO();
    static ImVector<ImWchar> ranges;
    if (ranges.empty()) {
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        static const ImWchar extraRanges[] = {0x2000, 0x206F, 0x2190, 0x21FF,
                                              0x25A0, 0x25FF, 0x2600, 0x26FF, 0};
        builder.AddRanges(extraRanges);
        builder.AddText("▶◀■●✂↶↷—…•→←↑↓✓✕⏵⏸⏹");
        builder.BuildRanges(&ranges);
    }

    auto addFont = [&](const std::string& path, float size) -> ImFont* {
        if (!path.empty() && fileExists(path)) {
            ImFontConfig cfg;
            cfg.OversampleH = 2;
            cfg.OversampleV = 2;
            cfg.PixelSnapH = true;
            cfg.GlyphRanges = ranges.Data;
            ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), size, &cfg);
            if (font) return font;
        }
        return nullptr;
    };

    const std::string segoe = windowsFontPath(L"segoeui.ttf");
    const std::string segoeSemibold = windowsFontPath(L"seguisb.ttf");
    const std::string consolas = windowsFontPath(L"consola.ttf");

    fonts.regular = addFont(segoe, 15.0f * scale);
    fonts.bold = addFont(segoeSemibold, 15.0f * scale);
    fonts.mono = addFont(consolas, 14.0f * scale);
    fonts.monoBig = addFont(consolas, 18.0f * scale);

    if (!fonts.regular) {
        fonts.regular = io.Fonts->AddFontDefault();
    }
    if (!fonts.bold) fonts.bold = fonts.regular;
    if (!fonts.mono) fonts.mono = fonts.regular;
    if (!fonts.monoBig) fonts.monoBig = fonts.mono;
}

} // namespace efs
