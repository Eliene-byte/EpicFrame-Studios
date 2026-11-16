#include "app.h"

#include "platform.h"
#include "project.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace efs {

namespace {

const ImVec4 kAccent(0.24f, 0.55f, 1.0f, 1.0f);
const ImVec4 kAccentHover(0.33f, 0.63f, 1.0f, 1.0f);
const ImVec4 kMuted(0.47f, 0.50f, 0.55f, 1.0f);
const ImVec4 kWarn(1.0f, 0.66f, 0.24f, 1.0f);
const ImVec4 kDanger(1.0f, 0.40f, 0.40f, 1.0f);

void sectionLabel(const char* label) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();
}

bool accentButton(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.38f, 0.72f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.46f, 0.86f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.55f, 0.98f, 1.0f));
    const bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

void badge(const char* text, const ImVec4& color) {
    const ImVec2 size = ImGui::CalcTextSize(text);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 padding(6.0f, 2.0f);
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x + padding.x * 2, pos.y + size.y + padding.y * 2),
                      ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.22f)), 3.0f);
    dl->AddRect(pos, ImVec2(pos.x + size.x + padding.x * 2, pos.y + size.y + padding.y * 2),
                ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.55f)), 3.0f);
    dl->AddText(ImVec2(pos.x + padding.x, pos.y + padding.y), ImGui::GetColorU32(color), text);
    ImGui::Dummy(ImVec2(size.x + padding.x * 2, size.y + padding.y * 2));
}

std::string clipDisplayName(const Project& project, const Clip& clip) {
    if (clip.kind == ClipKind::Title) {
        return clip.text.empty() ? "Título" : clip.text;
    }
    const Asset* asset = findAsset(project, clip.assetId);
    return asset ? asset->name : "Mídia ausente";
}

const char* clipKindLabel(const Clip& clip) {
    if (clip.kind == ClipKind::Title) return "Título";
    if (clip.kind == ClipKind::Audio) return "Áudio";
    return "Vídeo";
}

std::string mediaMeta(const Asset& asset) {
    char buffer[256];
    const std::string duration = formatDuration(asset.info.duration);
    if (asset.info.hasVideo) {
        std::snprintf(buffer, sizeof(buffer), "%s  •  %dx%d  •  %.2f fps", duration.c_str(),
                      asset.info.width, asset.info.height, asset.info.fps);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%s  •  %d Hz  •  %d canal(is)", duration.c_str(),
                      asset.info.sampleRate, asset.info.channels);
    }
    return buffer;
}

std::string extensionBadge(const Asset& asset) {
    if (asset.info.isImage) return "IMG";
    if (!asset.info.hasVideo) return "AUD";
    std::string ext = fileExtensionLower(asset.path);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });
    if (ext.empty()) ext = "VÍD";
    if (ext.size() > 3) ext.resize(3);
    return ext;
}

ImU32 assetAccent(const Asset& asset) {
    if (asset.missing) return IM_COL32(220, 80, 80, 255);
    if (asset.info.isImage) return IM_COL32(230, 170, 65, 255);
    if (!asset.info.hasVideo) return IM_COL32(65, 190, 135, 255);
    return IM_COL32(80, 150, 255, 255);
}

void drawFilmIcon(ImDrawList* dl, ImVec2 center, float size, ImU32 color) {
    const ImVec2 half(size * 0.5f, size * 0.36f);
    dl->AddRect(ImVec2(center.x - half.x, center.y - half.y),
                ImVec2(center.x + half.x, center.y + half.y), color, 3.0f, 0, 2.0f);
    for (int i = 1; i < 3; ++i) {
        const float x = center.x - half.x + half.x * 2.0f * (float)i / 3.0f;
        dl->AddLine(ImVec2(x, center.y - half.y), ImVec2(x, center.y + half.y), color, 1.0f);
    }
    const float y = center.y - half.y + half.y * 0.5f;
    dl->AddLine(ImVec2(center.x - half.x, y), ImVec2(center.x + half.x, y), color, 1.0f);
    dl->AddTriangleFilled(ImVec2(center.x - size * 0.08f, center.y - size * 0.13f),
                          ImVec2(center.x - size * 0.08f, center.y + size * 0.13f),
                          ImVec2(center.x + size * 0.14f, center.y), color);
}

} // namespace

void App::draw() {
    updateWindowTitle();
    drawMenuBar();
    drawStatusBar();
    drawDockSpaceAndPanels();
    drawModals();
    drawToasts();
}

void App::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("Arquivo")) {
        if (ImGui::MenuItem("Novo projeto", "Ctrl+N")) requestNew();
        if (ImGui::MenuItem("Abrir projeto...", "Ctrl+O")) requestOpen();
        ImGui::Separator();
        if (ImGui::MenuItem("Salvar", "Ctrl+S")) saveProject(false);
        if (ImGui::MenuItem("Salvar como...", "Ctrl+Shift+S")) saveProject(true);
        ImGui::Separator();
        if (ImGui::MenuItem("Importar mídia...", "Ctrl+I")) importMediaDialog();
        if (ImGui::MenuItem("Abrir pasta do projeto", nullptr, false, !project_.filePath.empty())) {
            openFolder(project_.filePath);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Sair")) requestQuit();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Editar")) {
        const bool hasSelection = !selection_.empty();
        if (ImGui::MenuItem("Dividir no cursor", "Ctrl+K")) splitAtPlayhead();
        if (ImGui::MenuItem("Excluir seleção", "Del", false, hasSelection)) deleteSelection();
        if (ImGui::MenuItem("Duplicar seleção", "Ctrl+D", false, hasSelection)) duplicateSelection();
        if (ImGui::MenuItem("Selecionar tudo", "Ctrl+A")) selectAll();
        ImGui::Separator();
        if (ImGui::MenuItem("Fade de entrada", nullptr, false, hasSelection)) applyFade(true, false);
        if (ImGui::MenuItem("Fade de saída", nullptr, false, hasSelection)) applyFade(false, true);
        if (ImGui::MenuItem("Fechar lacunas da faixa", nullptr, false, hasSelection)) {
            Clip* clip = selectedClip();
            if (clip) closeGaps(project_, clip->trackId);
            markDirty();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Sequência")) {
        if (ImGui::MenuItem("Adicionar faixa de vídeo")) {
            addTrack(project_, true);
            markDirty();
        }
        if (ImGui::MenuItem("Adicionar faixa de áudio")) {
            addTrack(project_, false);
            markDirty();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Configurações da sequência...")) showSequenceSettings_ = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Exibição")) {
        if (ImGui::BeginMenu("Qualidade da prévia")) {
            if (ImGui::MenuItem("Cheia", nullptr, previewQuality_ == 0)) previewQuality_ = 0;
            if (ImGui::MenuItem("Metade (recomendado)", nullptr, previewQuality_ == 1)) previewQuality_ = 1;
            if (ImGui::MenuItem("Um quarto", nullptr, previewQuality_ == 2)) previewQuality_ = 2;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Painéis")) {
            if (ImGui::MenuItem("Mídia")) ImGui::SetWindowFocus("Mídia###midia");
            if (ImGui::MenuItem("Monitor de origem")) ImGui::SetWindowFocus("Origem###origem");
            if (ImGui::MenuItem("Monitor de programa")) ImGui::SetWindowFocus("Programa###programa");
            if (ImGui::MenuItem("Inspetor")) ImGui::SetWindowFocus("Inspetor###inspetor");
            if (ImGui::MenuItem("Efeitos")) ImGui::SetWindowFocus("Efeitos###efeitos");
            if (ImGui::MenuItem("Linha do tempo")) ImGui::SetWindowFocus("Linha do tempo###linhaDoTempo");
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Redefinir layout", "Ctrl+0")) resetLayout_ = true;
        if (ImGui::MenuItem("Modo cinema", "F11", cinemaMode_)) {
            cinemaMode_ = !cinemaMode_;
            SDL_SetWindowFullscreen(window_, cinemaMode_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Ajuda")) {
        if (ImGui::MenuItem("Atalhos de teclado")) showShortcuts_ = true;
        if (ImGui::MenuItem("Sobre o EpicFrame Studios")) showAbout_ = true;
        ImGui::EndMenu();
    }

    const std::string right = "v" + appVersion();
    const float rightWidth = ImGui::CalcTextSize(right.c_str()).x + 20.0f;
    ImGui::SameLine(std::max(0.0f, ImGui::GetWindowWidth() - rightWidth));
    ImGui::TextDisabled("%s", right.c_str());
    ImGui::EndMainMenuBar();
}

void App::drawStatusBar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float height = ImGui::GetFrameHeight() + 2.0f;
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    const bool open = ImGui::BeginViewportSideBar("##BarraStatus", viewport, ImGuiDir_Down, height, flags);
    if (open) {
        ImGui::SetCursorPosY((height - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::TextColored(kAccent, "EF");
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::TextUnformatted(project_.settings.name.empty() ? "Sem título" : project_.settings.name.c_str());
        ImGui::SameLine(0.0f, 10.0f);
        if (project_.dirty) {
            ImGui::TextColored(kWarn, "• alterações não salvas");
        } else {
            ImGui::TextDisabled("• salvo");
        }
        ImGui::SameLine(0.0f, 16.0f);
        ImGui::TextDisabled("%d x %d  •  %.2f fps  •  %d clipe(s)", project_.settings.width,
                            project_.settings.height, project_.settings.fps, (int)project_.clips.size());

        const char* right = softwareRenderer_ ? "software  •  %d%%" : "acelerado  •  %d%%";
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), right, (int)std::lround(timelineZoom_ * 100.0f));
        const float width = ImGui::CalcTextSize(buffer).x + 16.0f;
        ImGui::SameLine(std::max(0.0f, ImGui::GetWindowWidth() - width));
        ImGui::TextDisabled("%s", buffer);
    }
    ImGui::End();
}

void App::drawDockSpaceAndPanels() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiID dockspaceId =
        ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
    if (!layoutLoaded_ || resetLayout_) {
        buildDefaultLayout(dockspaceId);
        layoutLoaded_ = true;
        resetLayout_ = false;
    }

    drawMediaBin();
    drawSourceMonitor();
    drawProgramMonitor();
    drawInspector();
    drawEffects();
    drawTimeline();
}

void App::buildDefaultLayout(unsigned int dockspaceId) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->Size);

    ImGuiID top = 0;
    ImGuiID timeline = 0;
    ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Down, 0.40f, &timeline, &top);

    ImGuiID left = 0;
    ImGuiID rest = 0;
    ImGui::DockBuilderSplitNode(top, ImGuiDir_Left, 0.20f, &left, &rest);

    ImGuiID right = 0;
    ImGuiID center = 0;
    ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.28f, &right, &center);

    ImGuiID media = 0;
    ImGuiID source = 0;
    ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.45f, &source, &media);

    ImGuiID inspector = 0;
    ImGuiID effects = 0;
    ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.52f, &effects, &inspector);

    ImGui::DockBuilderDockWindow("Mídia###midia", media);
    ImGui::DockBuilderDockWindow("Origem###origem", source);
    ImGui::DockBuilderDockWindow("Programa###programa", center);
    ImGui::DockBuilderDockWindow("Inspetor###inspetor", inspector);
    ImGui::DockBuilderDockWindow("Efeitos###efeitos", effects);
    ImGui::DockBuilderDockWindow("Linha do tempo###linhaDoTempo", timeline);
    ImGui::DockBuilderFinish(dockspaceId);
}

void App::drawMediaBin() {
    if (!ImGui::Begin("Mídia###midia")) {
        ImGui::End();
        return;
    }

    if (accentButton("+ Importar", ImVec2(104.0f, 0.0f))) importMediaDialog();
    ImGui::SameLine();
    if (ImGui::Button("Revelar pasta", ImVec2(106.0f, 0.0f))) {
        if (!project_.assets.empty()) {
            revealInExplorer(project_.assets.front().path);
        } else {
            toast("Nenhuma mídia importada.", 1);
        }
    }
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##busca", "Buscar mídia...", mediaSearch_, IM_ARRAYSIZE(mediaSearch_));

    double total = 0.0;
    for (const auto& asset : project_.assets) total += asset.info.duration;
    ImGui::TextDisabled("%d item(ns)  •  %s", (int)project_.assets.size(), formatDuration(total).c_str());
    ImGui::Separator();

    const float rowHeight = 46.0f * uiScale_;
    ImGui::BeginChild("##listaMidia", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    uint64_t pendingRemove = 0;
    int visible = 0;
    for (auto& asset : project_.assets) {
        const std::string haystack = lowerAscii(asset.name + " " + asset.path);
        if (mediaSearch_[0] != '\0' && haystack.find(lowerAscii(mediaSearch_)) == std::string::npos) {
            continue;
        }
        ++visible;
        ImGui::PushID((int)asset.id);
        const bool selected = selectedAssetId_ == asset.id;
        if (ImGui::Selectable("##midiaLinha", selected, ImGuiSelectableFlags_AllowDoubleClick,
                              ImVec2(0.0f, rowHeight))) {
            selectedAssetId_ = asset.id;
            selection_.clear();
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                sourceAssetId_ = asset.id;
                ImGui::SetWindowFocus("Origem###origem");
            }
        }
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (hovered && !selected) {
            dl->AddRectFilled(rowMin, rowMax, IM_COL32(255, 255, 255, 8));
        }

        const ImVec2 thumbMin(rowMin.x + 6.0f, rowMin.y + 6.0f);
        const ImVec2 thumbMax(thumbMin.x + 48.0f, rowMax.y - 6.0f);
        dl->AddRectFilled(thumbMin, thumbMax, IM_COL32(12, 14, 18, 255), 3.0f);
        dl->AddRect(thumbMin, thumbMax, ImGui::GetColorU32(ImVec4(1, 1, 1, 0.10f)), 3.0f);
        const std::string ext = extensionBadge(asset);
        const ImVec2 extSize = fonts_.mono ? fonts_.mono->CalcTextSizeA(11.0f * uiScale_, FLT_MAX, 0.0f,
                                                                       ext.c_str())
                                           : ImGui::CalcTextSize(ext.c_str());
        dl->AddText(fonts_.mono, 11.0f * uiScale_,
                    ImVec2((thumbMin.x + thumbMax.x - extSize.x) * 0.5f,
                           (thumbMin.y + thumbMax.y - extSize.y) * 0.5f),
                    assetAccent(asset), ext.c_str());

        const float textX = thumbMax.x + 9.0f;
        const ImVec4 clip(textX, rowMin.y + 4.0f, rowMax.x - 4.0f, rowMax.y - 2.0f);
        dl->PushClipRect(ImVec2(clip.x, clip.y), ImVec2(clip.z, clip.w), true);
        const ImU32 nameColor = asset.missing ? ImGui::GetColorU32(kDanger) : ImGui::GetColorU32(ImGuiCol_Text);
        dl->AddText(fonts_.regular, 14.0f * uiScale_, ImVec2(textX, rowMin.y + 5.0f), nameColor,
                    asset.name.c_str());
        std::string meta = mediaMeta(asset);
        if (asset.missing) meta = "Arquivo ausente — " + meta;
        dl->AddText(fonts_.regular, 11.0f * uiScale_, ImVec2(textX, rowMin.y + 25.0f),
                    ImGui::GetColorU32(asset.missing ? kWarn : kMuted), meta.c_str());
        dl->PopClipRect();

        if (ImGui::BeginDragDropSource()) {
            const uint64_t id = asset.id;
            ImGui::SetDragDropPayload("EFS_ASSET", &id, sizeof(id));
            ImGui::Text("Inserir \"%s\" na linha do tempo", asset.name.c_str());
            ImGui::EndDragDropSource();
        }
        if (hovered) {
            ImGui::SetTooltip("%s\n%s\n%s", asset.name.c_str(), mediaMeta(asset).c_str(), asset.path.c_str());
        }
        if (ImGui::BeginPopupContextItem("##midiaContexto")) {
            if (ImGui::MenuItem("Inserir no cursor")) insertAssetAtPlayhead(asset.id);
            if (ImGui::MenuItem("Abrir no monitor de origem")) {
                sourceAssetId_ = asset.id;
                ImGui::SetWindowFocus("Origem###origem");
            }
            if (ImGui::MenuItem("Revelar no Explorer")) revealInExplorer(asset.path);
            ImGui::Separator();
            if (ImGui::MenuItem("Remover do projeto")) pendingRemove = asset.id;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (pendingRemove) removeAsset(pendingRemove);

    if (visible == 0) {
        ImGui::Dummy(ImVec2(0.0f, 28.0f));
        const char* title = project_.assets.empty() ? "Nenhuma mídia importada" : "Nada encontrado";
        const char* subtitle = project_.assets.empty()
                                   ? "Importe vídeos, áudios ou imagens\nou arraste arquivos para a janela."
                                   : "Tente outro termo de busca.";
        const ImVec2 titleSize = ImGui::CalcTextSize(title);
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetContentRegionAvail().x - titleSize.x) * 0.5f));
        ImGui::TextUnformatted(title);
        const ImVec2 subtitleSize = ImGui::CalcTextSize(subtitle);
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetContentRegionAvail().x - subtitleSize.x) * 0.5f));
        ImGui::TextDisabled("%s", subtitle);
        if (project_.assets.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetContentRegionAvail().x - 140.0f) * 0.5f));
            if (accentButton("Importar mídia", ImVec2(140.0f, 0.0f))) importMediaDialog();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void App::drawSourceMonitor() {
    if (!ImGui::Begin("Origem###origem")) {
        ImGui::End();
        return;
    }

    Asset* asset = sourceAssetId_ ? findAsset(project_, sourceAssetId_) : nullptr;
    if (!asset) {
        ImGui::TextDisabled("MONITOR DE ORIGEM");
        ImVec2 avail = ImGui::GetContentRegionAvail();
        drawPreviewPlaceholder("Monitor de origem", "Dê um duplo clique em uma mídia da lista", avail);
        ImGui::Spacing();
        ImGui::TextWrapped("Use o monitor de origem para conferir o material antes de inserir na sequência.");
        ImGui::End();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextUnformatted("ORIGEM");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextUnformatted(asset->name.c_str());
    ImGui::SameLine();
    badge(asset->info.isImage ? "IMAGEM" : (asset->info.hasVideo ? "VÍDEO" : "ÁUDIO"), kAccent);

    const float controlsHeight = 42.0f * uiScale_;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 previewSize(std::max(40.0f, avail.x), std::max(60.0f, avail.y - controlsHeight));
    drawPreviewPlaceholder(asset->name.c_str(), mediaMeta(*asset).c_str(), previewSize);

    ImGui::Spacing();
    if (accentButton("Inserir no cursor", ImVec2(150.0f, 0.0f))) insertAssetAtPlayhead(asset->id);
    ImGui::SameLine();
    if (ImGui::Button("Revelar no Explorer", ImVec2(150.0f, 0.0f))) revealInExplorer(asset->path);
    ImGui::SameLine();
    if (ImGui::Button("Remover", ImVec2(90.0f, 0.0f))) removeAsset(asset->id);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove a mídia e os clipes que a usam.");

    ImGui::Spacing();
    ImGui::TextDisabled("Duração: %s   •   %s", formatDuration(asset->info.duration).c_str(),
                        asset->info.videoCodec.empty() ? (asset->info.audioCodec.empty() ? "—" : asset->info.audioCodec.c_str())
                                                       : asset->info.videoCodec.c_str());
    ImGui::TextWrapped("%s", asset->path.c_str());
    ImGui::End();
}

void App::drawProgramMonitor() {
    if (!ImGui::Begin("Programa###programa")) {
        ImGui::End();
        return;
    }

    const bool hasContent = !project_.clips.empty();
    ImGui::PushFont(fonts_.monoBig);
    ImGui::TextColored(kAccent, "%s", formatTimecode(project_.playhead, project_.settings.fps).c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("/ %s", formatTimecode(std::max(0.0, contentEnd(project_)), project_.settings.fps,
                                               false)
                                 .c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Ajustar")) {
        const double end = std::max(1.0, contentEnd(project_));
        timelineZoom_ = (float)std::clamp(60.0 / end, 0.15, 16.0);
        timelineScroll_ = 0.0;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ajusta o zoom da linha do tempo ao conteúdo.");

    const float barHeight = 46.0f * uiScale_;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float maxH = std::max(80.0f, avail.y - barHeight);
    const double aspect = (double)project_.settings.width / (double)std::max(1, project_.settings.height);
    float previewW = std::min(avail.x, (float)(maxH * aspect));
    float previewH = previewW / (float)aspect;
    if (previewH > maxH) {
        previewH = maxH;
        previewW = previewH * (float)aspect;
    }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 previewPos(origin.x + (avail.x - previewW) * 0.5f, origin.y + 2.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(previewPos.x - 2, previewPos.y - 2),
                      ImVec2(previewPos.x + previewW + 2, previewPos.y + previewH + 2),
                      IM_COL32(0, 0, 0, 255), 3.0f);
    dl->AddRectFilled(previewPos, ImVec2(previewPos.x + previewW, previewPos.y + previewH),
                      IM_COL32(7, 8, 10, 255), 2.0f);

    dl->PushClipRect(previewPos, ImVec2(previewPos.x + previewW, previewPos.y + previewH), true);
    if (!hasContent) {
        const ImVec2 center(previewPos.x + previewW * 0.5f, previewPos.y + previewH * 0.5f);
        drawFilmIcon(dl, ImVec2(center.x, center.y - 16.0f * uiScale_), 46.0f * uiScale_,
                     IM_COL32(255, 255, 255, 46));
        const char* line1 = "A prévia do programa aparece aqui";
        const char* line2 = "Importe mídia e monte sua sequência";
        const ImVec2 s1 = ImGui::CalcTextSize(line1);
        const ImVec2 s2 = ImGui::CalcTextSize(line2);
        dl->AddText(ImVec2(center.x - s1.x * 0.5f, center.y + 18.0f * uiScale_),
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.50f)), line1);
        dl->AddText(ImVec2(center.x - s2.x * 0.5f, center.y + 18.0f * uiScale_ + s1.y + 3.0f),
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.28f)), line2);
    } else {
        const Clip* titleClip = nullptr;
        for (auto it = project_.tracks.rbegin(); it != project_.tracks.rend(); ++it) {
            if (!it->video || it->hidden) continue;
            for (const auto& clip : project_.clips) {
                if (clip.trackId == it->id && clip.isTitle() && clip.contains(project_.playhead)) {
                    titleClip = &clip;
                    break;
                }
            }
            if (titleClip) break;
        }
        if (titleClip && !titleClip->text.empty()) {
            const float fontPx = std::max(8.0f, (float)titleClip->fontSize * (previewH / (float)project_.settings.height) *
                                                    (float)titleClip->scale);
            ImFont* font = titleClip->bold ? fonts_.bold : fonts_.regular;
            if (!font) font = fonts_.regular;
            const ImVec2 textSize = font->CalcTextSizeA(fontPx, FLT_MAX, 0.0f, titleClip->text.c_str());
            float x = previewPos.x + (float)titleClip->posX * previewW;
            if (titleClip->align == 1) x -= textSize.x * 0.5f;
            if (titleClip->align == 2) x -= textSize.x;
            const float y = previewPos.y + (float)titleClip->posY * previewH - textSize.y * 0.5f;
            if (titleClip->shadow) {
                dl->AddText(font, fontPx, ImVec2(x + 3.0f, y + 3.0f), IM_COL32(0, 0, 0, 160),
                            titleClip->text.c_str());
            }
            dl->AddText(font, fontPx, ImVec2(x, y), colorU32(titleClip->color), titleClip->text.c_str());
        }

        const Clip* active = nullptr;
        for (auto it = project_.tracks.rbegin(); it != project_.tracks.rend(); ++it) {
            if (!it->video || it->hidden) continue;
            for (const auto& clip : project_.clips) {
                if (clip.trackId == it->id && !clip.isTitle() && clip.contains(project_.playhead)) {
                    active = &clip;
                    break;
                }
            }
            if (active) break;
        }
        if (active) {
            const std::string name = clipDisplayName(project_, *active);
            const ImVec2 ts = ImGui::CalcTextSize(name.c_str());
            const ImVec2 p(previewPos.x + 10.0f, previewPos.y + previewH - ts.y - 16.0f);
            dl->AddRectFilled(p, ImVec2(p.x + ts.x + 12.0f, p.y + ts.y + 8.0f),
                              IM_COL32(0, 0, 0, 150), 3.0f);
            dl->AddText(ImVec2(p.x + 6.0f, p.y + 4.0f), IM_COL32(255, 255, 255, 220), name.c_str());
        }
    }

    char resolution[80];
    std::snprintf(resolution, sizeof(resolution), "%d x %d", project_.settings.width, project_.settings.height);
    const ImVec2 resSize = ImGui::CalcTextSize(resolution);
    dl->AddText(ImVec2(previewPos.x + previewW - resSize.x - 10.0f, previewPos.y + 8.0f),
                IM_COL32(255, 255, 255, 80), resolution);
    if (playing_) {
        dl->AddRectFilled(previewPos, ImVec2(previewPos.x + previewW, previewPos.y + 3.0f),
                          ImGui::GetColorU32(kAccent));
    }
    dl->PopClipRect();

    ImGui::SetCursorScreenPos(ImVec2(origin.x, previewPos.y + previewH + 8.0f));
    const float btnH = 26.0f * uiScale_;
    if (ImGui::Button("|<", ImVec2(34.0f, btnH))) seekTo(0.0);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ir para o início (Home)");
    ImGui::SameLine();
    if (ImGui::Button("<", ImVec2(32.0f, btnH))) stepFrame(-1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Quadro anterior (←)");
    ImGui::SameLine();
    if (accentButton(playing_ ? "Pausar" : "Reproduzir", ImVec2(92.0f, btnH))) togglePlayback();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reproduzir / pausar (Espaço)");
    ImGui::SameLine();
    if (ImGui::Button(">", ImVec2(32.0f, btnH))) stepFrame(1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Próximo quadro (→)");
    ImGui::SameLine();
    if (ImGui::Button(">|", ImVec2(34.0f, btnH))) seekTo(std::max(0.0, contentEnd(project_)));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ir para o fim (End)");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(118.0f * uiScale_);
    const char* qualities[] = {"Cheia", "Metade", "Um quarto"};
    int quality = std::clamp(previewQuality_, 0, 2);
    if (ImGui::Combo("##qualidade", &quality, qualities, IM_ARRAYSIZE(qualities))) {
        previewQuality_ = quality;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Qualidade da prévia (afeta o desempenho)");

    ImGui::End();
}

void App::drawPreviewPlaceholder(const char* title, const char* subtitle, ImVec2 size) {
    if (size.x < 20.0f || size.y < 20.0f) size = ImVec2(40.0f, 40.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(8, 9, 11, 255), 3.0f);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(255, 255, 255, 14), 3.0f);

    const ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
    drawFilmIcon(dl, ImVec2(center.x, center.y - 20.0f * uiScale_), 44.0f * uiScale_,
                 IM_COL32(255, 255, 255, 42));
    const ImVec2 titleSize = ImGui::CalcTextSize(title ? title : "");
    dl->AddText(ImVec2(center.x - titleSize.x * 0.5f, center.y + 12.0f * uiScale_),
                ImGui::GetColorU32(ImVec4(1, 1, 1, 0.55f)), title ? title : "");
    if (subtitle && subtitle[0]) {
        const ImVec2 subSize = ImGui::CalcTextSize(subtitle);
        dl->AddText(ImVec2(center.x - subSize.x * 0.5f, center.y + 12.0f * uiScale_ + titleSize.y + 3.0f),
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.30f)), subtitle);
    }
}

void App::drawModals() {
    if (showUnsaved_) {
        ImGui::OpenPopup("Alterações não salvas");
    }
    if (ImGui::BeginPopupModal("Alterações não salvas", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted("Deseja salvar as alterações antes de continuar?");
        ImGui::Spacing();
        if (accentButton("Salvar", ImVec2(110.0f, 0.0f))) {
            if (saveProject(false)) {
                ImGui::CloseCurrentPopup();
                showUnsaved_ = false;
                executePendingAction();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Descartar", ImVec2(110.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
            showUnsaved_ = false;
            executePendingAction();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(110.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
            showUnsaved_ = false;
            pendingAction_ = 0;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
            showUnsaved_ = false;
            pendingAction_ = 0;
        }
        ImGui::EndPopup();
    }

    if (showAbout_) {
        ImGui::OpenPopup("Sobre o EpicFrame Studios");
        showAbout_ = false;
    }
    if (ImGui::BeginPopupModal("Sobre o EpicFrame Studios", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PushFont(fonts_.bold);
        ImGui::TextUnformatted("EpicFrame Studios");
        ImGui::PopFont();
        ImGui::TextDisabled("Versão %s", appVersion().c_str());
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Editor de vídeo não linear leve, feito para notebooks antigos. "
            "Interface em português, projeto portátil e renderização otimizada.");
        ImGui::Spacing();
        ImGui::TextWrapped("Licenciado sob GPL-3.0-or-later. Usa SDL2, Dear ImGui e FFmpeg.");
        ImGui::Spacing();
        ImGui::TextDisabled("© 2026 EpicFrame Studios");
        ImGui::Spacing();
        if (ImGui::Button("Fechar", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (showShortcuts_) {
        ImGui::OpenPopup("Atalhos de teclado");
        showShortcuts_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(520.0f * uiScale_, 440.0f * uiScale_), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Atalhos de teclado", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        if (ImGui::BeginTable("##atalhos", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                                  ImGuiTableFlags_ScrollY,
                              ImVec2(0, 330.0f * uiScale_))) {
            ImGui::TableSetupColumn("Tecla", ImGuiTableColumnFlags_WidthFixed, 150.0f * uiScale_);
            ImGui::TableSetupColumn("Ação", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            static const char* rows[][2] = {
                {"Espaço", "Reproduzir / pausar"},
                {"← / →", "Quadro anterior / próximo"},
                {"Shift + ← / →", "Voltar / avançar 1 segundo"},
                {"Home / End", "Início / fim da sequência"},
                {"Ctrl + N", "Novo projeto"},
                {"Ctrl + O", "Abrir projeto"},
                {"Ctrl + S", "Salvar projeto"},
                {"Ctrl + Shift + S", "Salvar como"},
                {"Ctrl + I", "Importar mídia"},
                {"Ctrl + K", "Dividir no cursor"},
                {"Ctrl + D", "Duplicar seleção"},
                {"Ctrl + A", "Selecionar tudo"},
                {"Del", "Excluir seleção"},
                {"V", "Ferramenta de seleção"},
                {"C", "Ferramenta de corte (faca)"},
                {"T", "Adicionar título no cursor"},
                {"S", "Ativar / desativar ímã"},
                {"+ / -", "Zoom na linha do tempo"},
                {"Shift + roda", "Rolar a linha do tempo"},
                {"Ctrl + roda", "Zoom na linha do tempo"},
                {"Ctrl + 0", "Redefinir layout"},
                {"F11", "Modo cinema"},
            };
            for (const auto& row : rows) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(kAccent, "%s", row[0]);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(row[1]);
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
        if (ImGui::Button("Fechar", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    static bool seqWasOpen = false;
    if (showSequenceSettings_ && !seqWasOpen) {
        std::snprintf(sequenceNameEdit_, sizeof(sequenceNameEdit_), "%s", project_.settings.name.c_str());
        sequenceWidthEdit_ = project_.settings.width;
        sequenceHeightEdit_ = project_.settings.height;
        sequenceFpsEdit_ = project_.settings.fps;
        seqWasOpen = true;
        ImGui::OpenPopup("Configurações da sequência");
    }
    if (!showSequenceSettings_) seqWasOpen = false;
    if (ImGui::BeginPopupModal("Configurações da sequência", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SetNextItemWidth(300.0f * uiScale_);
        ImGui::InputText("Nome", sequenceNameEdit_, IM_ARRAYSIZE(sequenceNameEdit_));
        ImGui::SetNextItemWidth(140.0f * uiScale_);
        ImGui::InputInt("Largura", &sequenceWidthEdit_);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f * uiScale_);
        ImGui::InputInt("Altura", &sequenceHeightEdit_);
        ImGui::SetNextItemWidth(120.0f * uiScale_);
        ImGui::InputDouble("FPS", &sequenceFpsEdit_, 1.0, 5.0, "%.3f");
        ImGui::TextDisabled("Padrões: 1920 x 1080 a 30 fps.");
        ImGui::Spacing();
        if (accentButton("Aplicar", ImVec2(110.0f, 0.0f))) {
            project_.settings.name = sequenceNameEdit_[0] ? sequenceNameEdit_ : "Sem título";
            project_.settings.width = std::clamp(sequenceWidthEdit_, 16, 7680);
            project_.settings.height = std::clamp(sequenceHeightEdit_, 16, 4320);
            project_.settings.fps = std::clamp(sequenceFpsEdit_, 1.0, 240.0);
            markDirty();
            toast("Configurações da sequência atualizadas.");
            ImGui::CloseCurrentPopup();
            showSequenceSettings_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(110.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
            showSequenceSettings_ = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
            showSequenceSettings_ = false;
        }
        ImGui::EndPopup();
    }
}

void App::drawToasts() {
    const double now = nowSeconds();
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(),
                                 [now](const Toast& item) { return now - item.createdAt > 5.0; }),
                  toasts_.end());
    if (toasts_.empty()) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float width = 360.0f * uiScale_;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - width - 12.0f,
                                   viewport->WorkPos.y + 12.0f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.95f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDocking;
    ImGui::Begin("##notificacoes", nullptr, flags);
    for (const auto& item : toasts_) {
        const float age = (float)(now - item.createdAt);
        const float alpha = age > 4.5f ? std::clamp((5.0f - age) / 0.5f, 0.0f, 1.0f) : 1.0f;
        ImVec4 color = kAccent;
        if (item.level == 1) color = kWarn;
        if (item.level == 2) color = kDanger;
        color.w = alpha;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(item.level == 2 ? "!" : (item.level == 1 ? "•" : "✓"));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.90f, 0.93f, alpha));
        ImGui::TextWrapped("%s", item.text.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

} // namespace efs
