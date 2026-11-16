#include "app.h"

#include "platform.h"
#include "project.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace efs {

namespace {

const ImVec4 kAccent(0.24f, 0.55f, 1.0f, 1.0f);
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

bool dragDouble(const char* label, double* value, double minValue, double maxValue, float speed,
                const char* format = "%.3f s") {
    return ImGui::DragScalar(label, ImGuiDataType_Double, value, speed, &minValue, &maxValue, format);
}

std::string kindName(const Clip& clip) {
    if (clip.kind == ClipKind::Title) return "Título";
    if (clip.kind == ClipKind::Audio) return "Áudio";
    return "Vídeo";
}

void kindBadge(const Clip& clip) {
    ImVec4 color = kAccent;
    if (clip.kind == ClipKind::Audio) color = ImVec4(0.25f, 0.75f, 0.52f, 1.0f);
    if (clip.kind == ClipKind::Title) color = ImVec4(0.64f, 0.47f, 1.0f, 1.0f);
    const std::string label = kindName(clip);
    const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + textSize.x + 12.0f, pos.y + textSize.y + 4.0f),
                      ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.18f)), 3.0f);
    dl->AddRect(pos, ImVec2(pos.x + textSize.x + 12.0f, pos.y + textSize.y + 4.0f),
                ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.55f)), 3.0f);
    dl->AddText(ImVec2(pos.x + 6.0f, pos.y + 2.0f), ImGui::GetColorU32(color), label.c_str());
    ImGui::Dummy(ImVec2(textSize.x + 12.0f, textSize.y + 4.0f));
}

uint32_t packColor(const float col[4]) {
    auto channel = [](float value) { return (uint32_t)std::clamp((int)std::lround(value * 255.0f), 0, 255); };
    return (channel(col[0]) << 24) | (channel(col[1]) << 16) | (channel(col[2]) << 8) | channel(col[3]);
}

void unpackColor(uint32_t color, float out[4]) {
    out[0] = (float)((color >> 24) & 0xFF) / 255.0f;
    out[1] = (float)((color >> 16) & 0xFF) / 255.0f;
    out[2] = (float)((color >> 8) & 0xFF) / 255.0f;
    out[3] = (float)(color & 0xFF) / 255.0f;
}

} // namespace

void App::drawInspector() {
    if (!ImGui::Begin("Inspetor###inspetor")) {
        ImGui::End();
        return;
    }

    Clip* clip = selectedClip();
    if (clip) {
        const uint64_t clipId = clip->id;
        ImGui::PushFont(fonts_.bold);
        const Asset* asset = clip->assetId ? findAsset(project_, clip->assetId) : nullptr;
        const std::string name = clip->isTitle() ? "Título de texto" : (asset ? asset->name : "Mídia ausente");
        ImGui::TextWrapped("%s", name.c_str());
        ImGui::PopFont();
        ImGui::SameLine();
        kindBadge(*clip);
        const Track* track = findTrack(project_, clip->trackId);
        if (track) ImGui::TextDisabled("Faixa %s", track->name.c_str());

        ImGui::Spacing();
        if (ImGui::Button("Dividir", ImVec2(80.0f, 0.0f))) {
            ImGui::End();
            splitAtPlayhead();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicar", ImVec2(80.0f, 0.0f))) {
            ImGui::End();
            duplicateSelection();
            return;
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
        const bool remove = ImGui::Button("Excluir", ImVec2(72.0f, 0.0f));
        ImGui::PopStyleColor();
        if (remove) {
            ImGui::End();
            deleteSelection();
            return;
        }

        sectionLabel("TEMPO");
        double start = clip->start;
        if (dragDouble("Início", &start, 0.0, 24.0 * 3600.0, 0.01f)) {
            if (moveClipTo(project_, clipId, clip->trackId, snapFrame(start, project_.settings.fps))) {
                markDirty();
                clip = selectedClip();
                if (!clip) {
                    ImGui::End();
                    return;
                }
            }
        }
        const double minimum = 1.0 / std::max(1.0, project_.settings.fps);
        double end = clip->end();
        if (dragDouble("Fim", &end, clip->start + minimum, 24.0 * 3600.0, 0.01f)) {
            if (trimClipEdge(project_, clipId, false, snapFrame(end, project_.settings.fps))) {
                markDirty();
                clip = selectedClip();
                if (!clip) {
                    ImGui::End();
                    return;
                }
            }
        }
        ImGui::TextDisabled("Duração: %s", formatTimecode(clip->duration(), project_.settings.fps).c_str());
        if (!clip->isTitle()) {
            const Asset* source = findAsset(project_, clip->assetId);
            if (source) {
                ImGui::TextDisabled("Fonte: %s de %s",
                                    formatTimecode(clip->srcIn, project_.settings.fps).c_str(),
                                    formatTimecode(source->info.duration, project_.settings.fps).c_str());
            }
        }

        sectionLabel("MIXAGEM");
        if (!clip->isTitle()) {
            float volume = (float)clip->volume;
            if (ImGui::SliderFloat("Volume", &volume, 0.0f, 2.0f, "%.2f")) {
                clip->volume = volume;
                markDirty();
            }
        }
        if (!clip->isAudioOnly()) {
            float opacity = (float)clip->opacity;
            if (ImGui::SliderFloat("Opacidade", &opacity, 0.0f, 1.0f, "%.2f")) {
                clip->opacity = opacity;
                markDirty();
            }
        }
        const float maxFade = (float)std::max(0.0, std::min(clip->duration() * 0.5, 30.0));
        float fadeIn = (float)clip->fadeIn;
        float fadeOut = (float)clip->fadeOut;
        if (ImGui::SliderFloat("Fade in", &fadeIn, 0.0f, maxFade, "%.2f s")) {
            clip->fadeIn = fadeIn;
            markDirty();
        }
        if (ImGui::SliderFloat("Fade out", &fadeOut, 0.0f, maxFade, "%.2f s")) {
            clip->fadeOut = fadeOut;
            markDirty();
        }

        if (clip->isTitle()) {
            sectionLabel("TEXTO");
            if (titleEditClipId_ != clip->id) {
                std::snprintf(titleEditBuffer_, sizeof(titleEditBuffer_), "%s", clip->text.c_str());
                titleEditClipId_ = clip->id;
            }
            if (ImGui::InputTextMultiline("##textoTitulo", titleEditBuffer_, sizeof(titleEditBuffer_),
                                          ImVec2(-1.0f, 84.0f * uiScale_))) {
                clip->text = titleEditBuffer_;
                markDirty();
            }
            float color[4];
            unpackColor(clip->color, color);
            if (ImGui::ColorEdit4("Cor", color, ImGuiColorEditFlags_AlphaBar)) {
                clip->color = packColor(color);
                markDirty();
            }
            int fontSize = clip->fontSize;
            if (ImGui::SliderInt("Tamanho", &fontSize, 8, 300)) {
                clip->fontSize = fontSize;
                markDirty();
            }
            const char* aligns[] = {"Esquerda", "Centro", "Direita"};
            int align = clip->align;
            if (ImGui::Combo("Alinhamento", &align, aligns, IM_ARRAYSIZE(aligns))) {
                clip->align = align;
                markDirty();
            }
            if (ImGui::Checkbox("Negrito", &clip->bold)) markDirty();
            ImGui::SameLine();
            if (ImGui::Checkbox("Sombra", &clip->shadow)) markDirty();
        }

        sectionLabel("TRANSFORMAÇÃO");
        float scale = (float)clip->scale;
        if (ImGui::SliderFloat("Escala", &scale, 0.05f, 4.0f, "%.2f")) {
            clip->scale = scale;
            markDirty();
        }
        float posX = (float)clip->posX;
        if (ImGui::SliderFloat("Posição X", &posX, -0.5f, 1.5f, "%.2f")) {
            clip->posX = posX;
            markDirty();
        }
        float posY = (float)clip->posY;
        if (ImGui::SliderFloat("Posição Y", &posY, -0.5f, 1.5f, "%.2f")) {
            clip->posY = posY;
            markDirty();
        }

        sectionLabel("COR");
        float brightness = (float)clip->brightness;
        if (ImGui::SliderFloat("Brilho", &brightness, -1.0f, 1.0f, "%.2f")) {
            clip->brightness = brightness;
            markDirty();
        }
        float contrast = (float)clip->contrast;
        if (ImGui::SliderFloat("Contraste", &contrast, 0.0f, 2.0f, "%.2f")) {
            clip->contrast = contrast;
            markDirty();
        }
        float saturation = (float)clip->saturation;
        if (ImGui::SliderFloat("Saturação", &saturation, 0.0f, 2.0f, "%.2f")) {
            clip->saturation = saturation;
            markDirty();
        }
        if (ImGui::Button("Restaurar cor", ImVec2(-1.0f, 0.0f))) {
            clip->brightness = 0.0;
            clip->contrast = 1.0;
            clip->saturation = 1.0;
            markDirty();
        }
    } else if (Asset* asset = selectedAsset()) {
        ImGui::PushFont(fonts_.bold);
        ImGui::TextWrapped("%s", asset->name.c_str());
        ImGui::PopFont();
        if (asset->missing) {
            ImGui::TextColored(kDanger, "Arquivo ausente no disco");
        } else if (asset->info.isImage) {
            ImGui::TextColored(kWarn, "Imagem");
        } else if (asset->info.hasVideo) {
            ImGui::TextColored(kAccent, "Vídeo");
        } else {
            ImGui::TextColored(ImVec4(0.25f, 0.75f, 0.52f, 1.0f), "Áudio");
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Duração: %s", formatDuration(asset->info.duration).c_str());
        if (asset->info.hasVideo) {
            ImGui::TextDisabled("Resolução: %d x %d", asset->info.width, asset->info.height);
            ImGui::TextDisabled("FPS: %.2f", asset->info.fps);
            if (!asset->info.videoCodec.empty()) {
                ImGui::TextDisabled("Vídeo: %s", asset->info.videoCodec.c_str());
            }
        }
        if (asset->info.hasAudio) {
            ImGui::TextDisabled("Áudio: %s  •  %d Hz  •  %d canais",
                                asset->info.audioCodec.empty() ? "—" : asset->info.audioCodec.c_str(),
                                asset->info.sampleRate, asset->info.channels);
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Caminho");
        ImGui::TextWrapped("%s", asset->path.c_str());
        ImGui::Spacing();
        if (ImGui::Button("Inserir no cursor", ImVec2(-1.0f, 0.0f))) insertAssetAtPlayhead(asset->id);
        if (ImGui::Button("Revelar no Explorer", ImVec2(-1.0f, 0.0f))) revealInExplorer(asset->path);
        ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
        if (ImGui::Button("Remover do projeto", ImVec2(-1.0f, 0.0f))) removeAsset(asset->id);
        ImGui::PopStyleColor();
    } else {
        ImGui::PushFont(fonts_.bold);
        ImGui::TextUnformatted("Inspetor");
        ImGui::PopFont();
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Selecione um clipe na linha do tempo para ajustar tempo, volume, opacidade, fades, "
            "cor e transformação.");
        ImGui::Spacing();
        ImGui::TextDisabled("Dicas");
        ImGui::BulletText("Duplo clique em uma mídia abre o monitor de origem.");
        ImGui::BulletText("Arraste uma mídia da lista para uma faixa.");
        ImGui::BulletText("T para criar um título no cursor.");
        ImGui::BulletText("C para cortar; V para selecionar.");
    }

    ImGui::End();
}

void App::drawEffects() {
    if (!ImGui::Begin("Efeitos###efeitos")) {
        ImGui::End();
        return;
    }

    Clip* clip = selectedClip();
    if (clip) {
        ImGui::TextDisabled("Aplicar em: %s", clip->isTitle() ? "Título" : "Clipe selecionado");
    } else {
        ImGui::TextDisabled("Selecione um clipe para aplicar efeitos.");
    }
    ImGui::Spacing();

    sectionLabel("TÍTULOS");
    if (ImGui::Button("+ Adicionar título no cursor", ImVec2(-1.0f, 0.0f))) addTitleAtPlayhead();
    ImGui::TextDisabled("Cria um título de 5 segundos na faixa de vídeo livre.");

    sectionLabel("AJUSTES DE COR");
    ImGui::BeginDisabled(!clip);
    const char* presets[] = {"Natural", "Quente", "Frio", "Preto e branco", "Cinema",
                             "Vívido", "Suave", "Alto contraste"};
    const float available = ImGui::GetContentRegionAvail().x;
    const float buttonWidth = std::max(80.0f, (available - ImGui::GetStyle().ItemSpacing.x) * 0.5f);
    for (int i = 0; i < 8; ++i) {
        if (i % 2 == 0) {
            // first column
        } else {
            ImGui::SameLine();
        }
        if (ImGui::Button(presets[i], ImVec2(buttonWidth, 0.0f))) applyColorPreset(i);
    }
    if (ImGui::Button("Restaurar ajustes", ImVec2(-1.0f, 0.0f))) {
        if (clip) {
            clip->brightness = 0.0;
            clip->contrast = 1.0;
            clip->saturation = 1.0;
            markDirty();
        }
    }
    ImGui::EndDisabled();

    sectionLabel("FADES RÁPIDOS");
    ImGui::BeginDisabled(!clip);
    if (ImGui::Button("Fade de entrada (0,5 s)", ImVec2(buttonWidth, 0.0f))) applyFade(true, false);
    ImGui::SameLine();
    if (ImGui::Button("Fade de saída (0,5 s)", ImVec2(buttonWidth, 0.0f))) applyFade(false, true);
    ImGui::EndDisabled();

    sectionLabel("TRANSFORMAÇÃO");
    ImGui::BeginDisabled(!clip);
    if (ImGui::Button("Centralizar escala 100%", ImVec2(-1.0f, 0.0f))) {
        if (clip) {
            clip->scale = 1.0;
            clip->posX = 0.5;
            clip->posY = 0.5;
            markDirty();
        }
    }
    ImGui::EndDisabled();

    sectionLabel("DESEMPENHO");
    ImGui::TextWrapped(
        "Em notebooks antigos, use a prévia em Metade ou Um quarto. A exportação usa o FFmpeg e "
        "preserva a resolução final da sequência.");
    ImGui::Spacing();
    ImGui::TextDisabled("Renderizador atual: %s", softwareRenderer_ ? "software" : "acelerado");

    ImGui::End();
}

} // namespace efs
