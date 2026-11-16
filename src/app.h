#pragma once

#include "project.h"
#include "theme.h"

#include <SDL.h>

#include <string>
#include <vector>

namespace efs {

class App {
public:
    bool init();
    void run();
    void shutdown();

private:
    // lifecycle / io
    bool initSDL();
    bool initImGui();
    void frame();
    void handleEvent(const SDL_Event& event);
    void handleShortcuts();
    void updateWindowTitle();
    void updatePlayback();
    void* nativeWindow() const;
    double nowSeconds() const;

    // project actions
    void newProject();
    void requestNew();
    void requestOpen();
    void requestQuit();
    void executePendingAction();
    void openProjectDialog();
    bool openProjectPath(const std::string& path);
    bool saveProject(bool saveAs);
    void importMediaDialog();
    void importPaths(const std::vector<std::string>& paths);
    void addAssetFromPath(const std::string& path, bool showErrors);
    void removeAsset(uint64_t assetId);
    void markDirty();
    void clearSelection();
    void selectAll();

    // editing
    Clip* selectedClip();
    const Clip* selectedClip() const;
    Asset* selectedAsset();
    void selectClip(uint64_t clipId, bool toggle);
    void insertAssetAtPlayhead(uint64_t assetId);
    void addSelectedAssetAtEnd();
    void addTitleAtPlayhead();
    void splitAtPlayhead();
    void deleteSelection();
    void duplicateSelection();
    void applyFade(bool in, bool out);
    void applyColorPreset(int preset);
    void selectNewestClipOnTrack(uint64_t trackId);
    void togglePlayback();
    void seekTo(double seconds);
    void stepFrame(int direction);

    // UI
    void draw();
    void drawMenuBar();
    void drawDockSpaceAndPanels();
    void buildDefaultLayout(unsigned int dockspaceId);
    void drawMediaBin();
    void drawSourceMonitor();
    void drawProgramMonitor();
    void drawInspector();
    void drawEffects();
    void drawTimeline();
    void drawStatusBar();
    void drawModals();
    void drawToasts();
    void drawPreviewPlaceholder(const char* title, const char* subtitle, ImVec2 size);
    bool anyModalOpen() const;
    void toast(const std::string& text, int level = 0);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    UiFonts fonts_;
    float uiScale_ = 1.0f;
    std::string iniPath_;
    bool layoutLoaded_ = false;
    bool resetLayout_ = false;
    bool running_ = true;
    bool firstFrame_ = true;

    Project project_;
    std::vector<uint64_t> selection_;
    uint64_t selectedAssetId_ = 0;
    uint64_t sourceAssetId_ = 0;
    uint64_t titleEditClipId_ = 0;
    char titleEditBuffer_[2048] = {};
    char mediaSearch_[128] = {};

    int timelineTool_ = 0; // 0 select, 1 razor
    bool timelineSnapping_ = true;
    float timelineZoom_ = 1.0f;
    double timelineScroll_ = 0.0;
    float timelineScrollY_ = 0.0f;
    bool scrubbing_ = false;
    int previewQuality_ = 1; // 0 full, 1 half, 2 quarter
    bool useProxies_ = false;
    bool softwareRenderer_ = false;

    bool playing_ = false;
    double playStartTime_ = 0.0;
    Uint64 playStartTicks_ = 0;

    struct Toast {
        std::string text;
        int level = 0;
        double createdAt = 0.0;
    };
    std::vector<Toast> toasts_;

    bool showAbout_ = false;
    bool showShortcuts_ = false;
    bool showUnsaved_ = false;
    bool showSequenceSettings_ = false;
    int pendingAction_ = 0; // 1 new, 2 open, 3 quit
    bool cinemaMode_ = false;

    char sequenceNameEdit_[128] = {};
    int sequenceWidthEdit_ = 1920;
    int sequenceHeightEdit_ = 1080;
    double sequenceFpsEdit_ = 30.0;
};

} // namespace efs
