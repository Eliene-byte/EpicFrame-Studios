#include "app.h"

#include "media.h"
#include "platform.h"
#include "project.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include <SDL_syswm.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace efs {

bool App::init() {
    if (!initSDL()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "EpicFrame Studios",
                                 "Não foi possível iniciar a interface gráfica (SDL2).", nullptr);
        return false;
    }
    if (!initImGui()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "EpicFrame Studios",
                                 "Não foi possível iniciar a interface (ImGui).", nullptr);
        return false;
    }

    initFFmpeg();
    initDefaultProject(project_);
    layoutLoaded_ = fileExists(iniPath_);
    updateWindowTitle();
    return true;
}

bool App::initSDL() {
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0) {
        return false;
    }

    float ddpi = 96.0f;
    if (SDL_GetDisplayDPI(0, &ddpi, nullptr, nullptr) == 0 && ddpi > 0.0f) {
        uiScale_ = std::clamp(ddpi / 96.0f, 1.0f, 1.6f);
    }

    const int width = (int)std::lround(1500.0 * uiScale_);
    const int height = (int)std::lround(880.0 * uiScale_);
    window_ = SDL_CreateWindow("EpicFrame Studios", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               width, height,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window_) return false;

    SDL_SetWindowMinimumSize(window_, (int)(1024 * uiScale_), (int)(640 * uiScale_));

    renderer_ = SDL_CreateRenderer(window_, -1,
                                   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC |
                                       SDL_RENDERER_TARGETTEXTURE);
    if (!renderer_) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        softwareRenderer_ = true;
    }
    if (!renderer_) return false;

    SDL_RendererInfo info = {};
    if (SDL_GetRendererInfo(renderer_, &info) == 0) {
        softwareRenderer_ = (info.flags & SDL_RENDERER_SOFTWARE) != 0;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    const std::string config = configDir();
    const std::string base = config.empty() ? appDir() : config;
    ensureDirectory(base);
    iniPath_ = joinPath(base, "layout.ini");
    return true;
}

bool App::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = false;
    io.IniFilename = iniPath_.empty() ? nullptr : iniPath_.c_str();

    applyTheme(uiScale_, fonts_);
    if (fonts_.regular) io.FontDefault = fonts_.regular;

    if (!ImGui_ImplSDL2_InitForSDLRenderer(window_, renderer_)) return false;
    if (!ImGui_ImplSDLRenderer2_Init(renderer_)) return false;
    return true;
}

void App::run() {
    Uint64 last = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    const double targetFrame = 1.0 / 60.0;

    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            handleEvent(event);
            if (!running_) break;
        }
        if (!running_) break;

        updatePlayback();
        frame();

        const Uint64 now = SDL_GetPerformanceCounter();
        const double elapsed = (double)(now - last) / (double)frequency;
        last = now;
        if (elapsed < targetFrame - 0.001) {
            SDL_Delay((Uint32)std::max(1.0, (targetFrame - elapsed) * 1000.0));
        }
    }
}

void App::frame() {
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    handleShortcuts();
    draw();

    ImGui::Render();
    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer_, &width, &height);
    SDL_SetRenderDrawColor(renderer_, 15, 17, 20, 255);
    SDL_RenderClear(renderer_);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
    SDL_RenderPresent(renderer_);

    firstFrame_ = false;
}

void App::handleEvent(const SDL_Event& event) {
    ImGui_ImplSDL2_ProcessEvent(&event);
    switch (event.type) {
        case SDL_QUIT:
            requestQuit();
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window_)) {
                requestQuit();
            }
            break;
        case SDL_DROPFILE: {
            if (event.drop.file) {
                importPaths({event.drop.file});
                SDL_free(event.drop.file);
            }
            break;
        }
        default:
            break;
    }
}

void App::handleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || anyModalOpen()) return;

    const bool ctrl = io.KeyCtrl;
    const bool shift = io.KeyShift;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) requestNew();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) requestOpen();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) saveProject(shift);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_I, false)) importMediaDialog();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) splitAtPlayhead();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) duplicateSelection();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) selectAll();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_0, false)) resetLayout_ = true;

    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_Space, false)) togglePlayback();
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) deleteSelection();
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) deleteSelection();
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) timelineTool_ = 0;
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) timelineTool_ = 1;
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_T, false)) addTitleAtPlayhead();
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) timelineSnapping_ = !timelineSnapping_;

    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, shift)) {
        if (shift) {
            seekTo(project_.playhead - 1.0);
        } else {
            stepFrame(-1);
        }
    }
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_RightArrow, shift)) {
        if (shift) {
            seekTo(project_.playhead + 1.0);
        } else {
            stepFrame(1);
        }
    }
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_Home, false)) seekTo(0.0);
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_End, false)) seekTo(std::max(0.0, contentEnd(project_)));

    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_Equal, true)) {
        timelineZoom_ = std::clamp(timelineZoom_ * 1.25f, 0.15f, 16.0f);
    }
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_Minus, true)) {
        timelineZoom_ = std::clamp(timelineZoom_ / 1.25f, 0.15f, 16.0f);
    }

    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_F11, false)) {
        cinemaMode_ = !cinemaMode_;
        SDL_SetWindowFullscreen(window_, cinemaMode_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }
}

void App::updatePlayback() {
    if (!playing_) return;
    const double end = std::max(0.0, contentEnd(project_));
    if (end <= 0.01) {
        playing_ = false;
        return;
    }
    const double elapsed = (double)(SDL_GetTicks64() - playStartTicks_) / 1000.0;
    double time = playStartTime_ + elapsed;
    if (time >= end) {
        time = end;
        playing_ = false;
    }
    project_.playhead = std::clamp(time, 0.0, end);
}

void App::togglePlayback() {
    if (playing_) {
        playing_ = false;
        return;
    }
    const double end = std::max(0.0, contentEnd(project_));
    if (end <= 0.01) {
        toast("Adicione mídia à linha do tempo para reproduzir.", 1);
        return;
    }
    if (project_.playhead >= end - 0.001) project_.playhead = 0.0;
    playStartTime_ = project_.playhead;
    playStartTicks_ = SDL_GetTicks64();
    playing_ = true;
}

void App::seekTo(double seconds) {
    const double end = std::max(0.0, contentEnd(project_));
    project_.playhead = std::clamp(snapFrame(seconds, project_.settings.fps), 0.0, std::max(end, 0.0));
    if (playing_) {
        playStartTime_ = project_.playhead;
        playStartTicks_ = SDL_GetTicks64();
    }
}

void App::stepFrame(int direction) {
    playing_ = false;
    const double frame = 1.0 / std::max(1.0, project_.settings.fps);
    seekTo(project_.playhead + frame * (double)direction);
}

double App::nowSeconds() const {
    return (double)SDL_GetTicks64() / 1000.0;
}

void App::updateWindowTitle() {
    static std::string lastTitle;
    std::string title = project_.settings.name.empty() ? "Sem título" : project_.settings.name;
    if (project_.dirty) title += " *";
    title += " — EpicFrame Studios";
    if (title != lastTitle) {
        SDL_SetWindowTitle(window_, title.c_str());
        lastTitle = title;
    }
}

void* App::nativeWindow() const {
    SDL_SysWMinfo info = {};
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window_, &info)) {
        if (info.subsystem == SDL_SYSWM_WINDOWS) return (void*)info.info.win.window;
    }
    return nullptr;
}

void App::newProject() {
    project_ = Project{};
    initDefaultProject(project_);
    selection_.clear();
    selectedAssetId_ = 0;
    sourceAssetId_ = 0;
    titleEditClipId_ = 0;
    titleEditBuffer_[0] = '\0';
    playing_ = false;
    timelineScroll_ = 0.0;
    timelineScrollY_ = 0.0f;
    timelineZoom_ = 1.0f;
    updateWindowTitle();
    toast("Novo projeto criado.");
}

void App::requestNew() {
    if (project_.dirty) {
        pendingAction_ = 1;
        showUnsaved_ = true;
        return;
    }
    pendingAction_ = 1;
    executePendingAction();
}

void App::requestOpen() {
    if (project_.dirty) {
        pendingAction_ = 2;
        showUnsaved_ = true;
        return;
    }
    pendingAction_ = 2;
    executePendingAction();
}

void App::requestQuit() {
    if (project_.dirty) {
        pendingAction_ = 3;
        showUnsaved_ = true;
        return;
    }
    running_ = false;
}

void App::executePendingAction() {
    const int action = pendingAction_;
    pendingAction_ = 0;
    showUnsaved_ = false;
    if (action == 1) {
        newProject();
    } else if (action == 2) {
        openProjectDialog();
    } else if (action == 3) {
        running_ = false;
    }
}

void App::openProjectDialog() {
    const std::string path = efs::openProjectDialog(nativeWindow());
    if (!path.empty()) openProjectPath(path);
}

bool App::openProjectPath(const std::string& path) {
    std::string error;
    if (!fileExists(path)) {
        toast("Projeto não encontrado: " + fileName(path), 2);
        return false;
    }
    if (!efs::loadProject(project_, path, error)) {
        toast("Não foi possível abrir o projeto: " + error, 2);
        return false;
    }
    selection_.clear();
    selectedAssetId_ = 0;
    sourceAssetId_ = 0;
    titleEditClipId_ = 0;
    titleEditBuffer_[0] = '\0';
    playing_ = false;
    timelineScroll_ = 0.0;
    timelineScrollY_ = 0.0f;
    int missing = 0;
    for (const auto& asset : project_.assets) {
        if (asset.missing) ++missing;
    }
    if (missing > 0) {
        toast(std::to_string(missing) + " mídia(s) não encontrada(s) no disco.", 1);
    }
    updateWindowTitle();
    toast("Projeto aberto: " + project_.settings.name);
    return true;
}

bool App::saveProject(bool saveAs) {
    std::string path = project_.filePath;
    if (saveAs || path.empty()) {
        std::string suggested = project_.settings.name;
        if (suggested.empty() || suggested == "Sem título") suggested = "Meu projeto";
        path = efs::saveProjectDialog(nativeWindow(), suggested);
        if (path.empty()) return false;
    }

    std::string error;
    if (!efs::saveProject(project_, path, error)) {
        toast("Não foi possível salvar: " + error, 2);
        return false;
    }

    project_.filePath = path;
    if (project_.settings.name.empty() || project_.settings.name == "Sem título") {
        project_.settings.name = fileBaseName(path);
    }
    project_.dirty = false;
    updateWindowTitle();
    toast("Projeto salvo em " + fileName(path));
    return true;
}

void App::importMediaDialog() {
    const std::vector<std::string> paths = efs::openMediaDialog(nativeWindow());
    if (!paths.empty()) importPaths(paths);
}

void App::importPaths(const std::vector<std::string>& paths) {
    if (paths.empty()) return;
    const size_t before = project_.assets.size();
    for (const auto& path : paths) addAssetFromPath(path, true);
    const size_t added = project_.assets.size() - before;
    if (added > 0) {
        toast(std::to_string(added) + " mídia(s) importada(s).");
    }
}

void App::addAssetFromPath(const std::string& path, bool showErrors) {
    if (path.empty()) return;
    if (!fileExists(path)) {
        if (showErrors) toast("Arquivo não encontrado: " + fileName(path), 2);
        return;
    }
    const std::string normalized = normalizePathForCompare(path);
    for (auto& asset : project_.assets) {
        if (normalizePathForCompare(asset.path) == normalized) {
            selectedAssetId_ = asset.id;
            sourceAssetId_ = asset.id;
            if (showErrors) toast("Esta mídia já está no projeto.", 1);
            return;
        }
    }

    const ProbeResult probe = probeMedia(path);
    if (!probe.ok()) {
        if (showErrors) toast("Falha ao importar " + fileName(path) + ": " + probe.error, 2);
        return;
    }

    Asset asset;
    asset.id = project_.newId();
    asset.path = path;
    std::string base = fileBaseName(path);
    if (base.empty()) base = fileName(path);
    asset.name = uniqueAssetName(project_, base);
    asset.info = probe.info;
    project_.assets.push_back(asset);
    selectedAssetId_ = asset.id;
    sourceAssetId_ = asset.id;
    project_.dirty = true;
    updateWindowTitle();
}

void App::removeAsset(uint64_t assetId) {
    const Asset* asset = findAsset(project_, assetId);
    if (!asset) return;
    const std::string name = asset->name;
    project_.clips.erase(
        std::remove_if(project_.clips.begin(), project_.clips.end(),
                       [assetId](const Clip& clip) { return clip.assetId == assetId; }),
        project_.clips.end());
    project_.assets.erase(
        std::remove_if(project_.assets.begin(), project_.assets.end(),
                       [assetId](const Asset& a) { return a.id == assetId; }),
        project_.assets.end());
    if (selectedAssetId_ == assetId) selectedAssetId_ = 0;
    if (sourceAssetId_ == assetId) sourceAssetId_ = 0;
    selection_.clear();
    project_.dirty = true;
    updateWindowTitle();
    toast("Mídia removida: " + name, 1);
}

void App::markDirty() {
    project_.dirty = true;
    updateWindowTitle();
}

void App::clearSelection() {
    selection_.clear();
    titleEditClipId_ = 0;
    titleEditBuffer_[0] = '\0';
}

void App::selectAll() {
    selection_.clear();
    for (const auto& clip : project_.clips) selection_.push_back(clip.id);
    selectedAssetId_ = 0;
    titleEditClipId_ = 0;
}

Clip* App::selectedClip() {
    for (uint64_t id : selection_) {
        Clip* clip = findClip(project_, id);
        if (clip) return clip;
    }
    return nullptr;
}

const Clip* App::selectedClip() const {
    for (uint64_t id : selection_) {
        const Clip* clip = findClip(project_, id);
        if (clip) return clip;
    }
    return nullptr;
}

Asset* App::selectedAsset() {
    return selectedAssetId_ ? findAsset(project_, selectedAssetId_) : nullptr;
}

void App::selectClip(uint64_t clipId, bool toggle) {
    if (clipId == 0) {
        clearSelection();
        return;
    }
    auto it = std::find(selection_.begin(), selection_.end(), clipId);
    if (toggle) {
        if (it != selection_.end()) {
            selection_.erase(it);
        } else {
            selection_.push_back(clipId);
        }
    } else {
        selection_.clear();
        selection_.push_back(clipId);
    }
    selectedAssetId_ = 0;
    titleEditClipId_ = 0;
}

void App::insertAssetAtPlayhead(uint64_t assetId) {
    Asset* asset = findAsset(project_, assetId);
    if (!asset) return;
    if (asset->missing) {
        toast("A mídia está fora do disco. Reconecte o arquivo e tente novamente.", 2);
        return;
    }

    Track* target = nullptr;
    if (!asset->info.hasVideo) {
        for (auto& track : project_.tracks) {
            if (!track.video) {
                target = &track;
                break;
            }
        }
        if (!target) {
            const uint64_t id = addTrack(project_, false);
            target = findTrack(project_, id);
        }
    } else {
        const Clip* current = selectedClip();
        if (current) {
            Track* selectedTrack = findTrack(project_, current->trackId);
            if (selectedTrack && selectedTrack->video) target = selectedTrack;
        }
        if (!target) {
            for (auto& track : project_.tracks) {
                if (!track.video) continue;
                if (!clipAt(project_, track.id, project_.playhead)) {
                    target = &track;
                    break;
                }
            }
        }
        if (!target) {
            for (auto& track : project_.tracks) {
                if (track.video) {
                    target = &track;
                    break;
                }
            }
        }
        if (!target) {
            const uint64_t id = addTrack(project_, true);
            target = findTrack(project_, id);
        }
    }
    if (!target) return;

    const double start = snapFrame(std::max(0.0, project_.playhead), project_.settings.fps);
    const double duration = asset->info.duration > 0.0 ? asset->info.duration : 10.0;
    std::string error;
    if (!addMediaToTrack(project_, assetId, target->id, start, 0.0,
                         std::min(duration, asset->info.duration > 0.0 ? asset->info.duration : 10.0),
                         false, error)) {
        toast("Não foi possível inserir: " + error, 2);
        return;
    }
    selectNewestClipOnTrack(target->id);
    updateWindowTitle();
    toast("Mídia inserida em " + target->name + ".");
}

void App::addSelectedAssetAtEnd() {
    insertAssetAtPlayhead(selectedAssetId_);
}

void App::addTitleAtPlayhead() {
    Track* target = nullptr;
    const double start = snapFrame(std::max(0.0, project_.playhead), project_.settings.fps);
    const double duration = 5.0;
    for (auto& track : project_.tracks) {
        if (!track.video) continue;
        bool free = true;
        for (const auto& clip : project_.clips) {
            if (clip.trackId != track.id) continue;
            if (clip.start < start + duration - kEpsilon && clip.end() > start + kEpsilon) {
                free = false;
                break;
            }
        }
        if (free) {
            target = &track;
            break;
        }
    }
    if (!target) {
        for (auto& track : project_.tracks) {
            if (track.video) target = &track;
        }
    }
    if (!target) {
        const uint64_t id = addTrack(project_, true);
        target = findTrack(project_, id);
    }
    if (!target) return;

    std::string error;
    if (!addTitleToTrack(project_, target->id, start, duration, error)) {
        toast("Não foi possível adicionar o título: " + error, 2);
        return;
    }
    selectNewestClipOnTrack(target->id);
    titleEditClipId_ = 0;
    updateWindowTitle();
    toast("Título adicionado. Edite o texto no Inspetor.");
}

void App::splitAtPlayhead() {
    std::vector<uint64_t> only;
    if (!selection_.empty()) only = selection_;
    const int count = splitClipsAt(project_, project_.playhead, only);
    if (count > 0) {
        updateWindowTitle();
        toast(std::to_string(count) + " clipe(s) dividido(s).");
    } else {
        toast("Posicione o cursor dentro de um clipe para dividir.", 1);
    }
}

void App::deleteSelection() {
    if (selection_.empty()) return;
    if (removeClips(project_, selection_, false)) {
        clearSelection();
        updateWindowTitle();
        toast("Seleção excluída.");
    }
}

void App::duplicateSelection() {
    if (selection_.empty()) return;
    duplicateClips(project_, selection_);
    updateWindowTitle();
    toast("Clipes duplicados.");
}

void App::applyFade(bool in, bool out) {
    Clip* clip = selectedClip();
    if (!clip) {
        toast("Selecione um clipe para aplicar o fade.", 1);
        return;
    }
    const double length = std::min(0.5, clip->duration() * 0.5);
    if (in) clip->fadeIn = length;
    if (out) clip->fadeOut = length;
    markDirty();
    toast(in && out ? "Fades aplicados." : (in ? "Fade de entrada aplicado." : "Fade de saída aplicado."));
}

void App::applyColorPreset(int preset) {
    Clip* clip = selectedClip();
    if (!clip) {
        toast("Selecione um clipe para aplicar o ajuste de cor.", 1);
        return;
    }
    switch (preset) {
        case 0: clip->brightness = 0.0; clip->contrast = 1.0; clip->saturation = 1.0; break;
        case 1: clip->brightness = 0.04; clip->contrast = 1.05; clip->saturation = 1.10; break;
        case 2: clip->brightness = -0.02; clip->contrast = 1.03; clip->saturation = 0.92; break;
        case 3: clip->brightness = 0.01; clip->contrast = 1.08; clip->saturation = 0.0; break;
        case 4: clip->brightness = -0.04; clip->contrast = 1.15; clip->saturation = 0.88; break;
        case 5: clip->brightness = 0.02; clip->contrast = 1.12; clip->saturation = 1.28; break;
        case 6: clip->brightness = 0.05; clip->contrast = 0.92; clip->saturation = 0.90; break;
        case 7: clip->brightness = 0.0; clip->contrast = 1.32; clip->saturation = 1.02; break;
        default: break;
    }
    markDirty();
    static const char* names[] = {"Natural", "Quente", "Frio", "Preto e branco", "Cinema",
                                  "Vívido", "Suave", "Alto contraste"};
    toast(std::string("Ajuste aplicado: ") + names[std::clamp(preset, 0, 7)]);
}

void App::selectNewestClipOnTrack(uint64_t trackId) {
    uint64_t best = 0;
    for (const auto& clip : project_.clips) {
        if (clip.trackId == trackId && clip.id > best) best = clip.id;
    }
    if (best) selectClip(best, false);
}

void App::toast(const std::string& text, int level) {
    if (text.empty()) return;
    if (toasts_.size() >= 5) toasts_.erase(toasts_.begin());
    Toast item;
    item.text = text;
    item.level = std::clamp(level, 0, 2);
    item.createdAt = nowSeconds();
    toasts_.push_back(item);
}

bool App::anyModalOpen() const {
    return showUnsaved_ || showAbout_ || showShortcuts_ || showSequenceSettings_;
}

void App::shutdown() {
    playing_ = false;
    if (ImGui::GetCurrentContext()) {
        ImGui::SaveIniSettingsToDisk(iniPath_.c_str());
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

} // namespace efs
