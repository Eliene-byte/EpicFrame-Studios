#include "app.h"

#include "platform.h"
#include "project.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace efs {

namespace {

const ImVec4 kAccent(0.24f, 0.55f, 1.0f, 1.0f);
const ImVec4 kMuted(0.47f, 0.50f, 0.55f, 1.0f);
const ImVec4 kDanger(1.0f, 0.40f, 0.40f, 1.0f);

struct TrackRow {
    Track* track = nullptr;
    float y = 0.0f;
    float height = 0.0f;
};

struct DragState {
    int mode = 0; // 1 move, 2 trim left, 3 trim right, 4 scrub
    uint64_t clipId = 0;
    double grabOffset = 0.0;
    bool moved = false;
};

DragState gDrag;
uint64_t gContextClipId = 0;

std::vector<Track*> orderedTracks(Project& project) {
    std::vector<Track*> videos;
    std::vector<Track*> audios;
    for (auto& track : project.tracks) {
        if (track.video) {
            videos.push_back(&track);
        } else {
            audios.push_back(&track);
        }
    }
    std::reverse(videos.begin(), videos.end());
    std::vector<Track*> result = videos;
    result.insert(result.end(), audios.begin(), audios.end());
    return result;
}

std::string clipLabel(const Project& project, const Clip& clip) {
    if (clip.kind == ClipKind::Title) {
        return clip.text.empty() ? "Título" : clip.text;
    }
    const Asset* asset = findAsset(project, clip.assetId);
    return asset ? asset->name : "Mídia ausente";
}

bool selectionHas(const std::vector<uint64_t>& selection, uint64_t clipId) {
    return std::find(selection.begin(), selection.end(), clipId) != selection.end();
}

double snapTimelineTime(const Project& project, double time, uint64_t ignoreClip, bool snapping,
                        double pixelsPerSecond) {
    if (!snapping || pixelsPerSecond <= 0.0) return time;
    const double threshold = 9.0 / pixelsPerSecond;
    double best = time;
    double bestDistance = threshold;
    auto consider = [&](double candidate) {
        const double distance = std::abs(candidate - time);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    };
    consider(0.0);
    consider(project.playhead);
    for (const auto& clip : project.clips) {
        if (clip.id == ignoreClip) continue;
        consider(clip.start);
        consider(clip.end());
    }
    if (bestDistance < threshold) return snapFrame(best, project.settings.fps);
    return time;
}

void drawEye(ImDrawList* dl, ImVec2 center, bool open, ImU32 color) {
    const float radius = 6.0f;
    dl->AddCircle(center, radius, color, 12, 1.4f);
    dl->AddCircleFilled(center, 2.3f, color, 8);
    if (!open) {
        dl->AddLine(ImVec2(center.x - 8, center.y + 8), ImVec2(center.x + 8, center.y - 8), color, 1.6f);
    }
}

void drawLock(ImDrawList* dl, ImVec2 center, bool locked, ImU32 color) {
    dl->AddRect(ImVec2(center.x - 5.5f, center.y - 1.0f), ImVec2(center.x + 5.5f, center.y + 7.0f), color,
                1.5f, 0, 1.3f);
    dl->AddCircle(ImVec2(center.x, center.y - 2.0f), 4.0f, color, 8, 1.3f);
    if (locked) {
        dl->AddRectFilled(ImVec2(center.x - 5.5f, center.y - 1.0f), ImVec2(center.x + 5.5f, center.y + 7.0f),
                          ImGui::GetColorU32(ImVec4(1.0f, 0.72f, 0.30f, 0.45f)), 1.5f);
    }
}

void drawSpeaker(ImDrawList* dl, ImVec2 center, bool muted, ImU32 color) {
    dl->AddTriangleFilled(ImVec2(center.x - 8.0f, center.y - 3.0f), ImVec2(center.x - 8.0f, center.y + 3.0f),
                          ImVec2(center.x - 2.5f, center.y), color);
    dl->AddRectFilled(ImVec2(center.x - 3.0f, center.y - 6.0f), ImVec2(center.x + 0.5f, center.y + 6.0f), color,
                      1.0f);
    if (muted) {
        dl->AddLine(ImVec2(center.x + 3.0f, center.y - 7.0f), ImVec2(center.x + 8.0f, center.y + 7.0f), color,
                    1.6f);
    } else {
        dl->PathArcTo(ImVec2(center.x - 1.0f, center.y), 8.0f, -0.9f, 0.9f, 10);
        dl->PathStroke(color, 0, 1.4f);
        dl->PathArcTo(ImVec2(center.x - 1.0f, center.y), 5.0f, -0.9f, 0.9f, 8);
        dl->PathStroke(color, 0, 1.2f);
    }
}

} // namespace

void App::drawTimeline() {
    if (!ImGui::Begin("Linha do tempo###linhaDoTempo")) {
        ImGui::End();
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const float scale = uiScale_;

    // ---------------------------------------------------------------------
    // Toolbar
    // ---------------------------------------------------------------------
    auto toolButton = [&](const char* label, int tool) {
        const bool active = timelineTool_ == tool;
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.78f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.50f, 0.90f, 1.0f));
        }
        const bool clicked = ImGui::Button(label, ImVec2(78.0f * scale, 0.0f));
        if (active) ImGui::PopStyleColor(2);
        return clicked;
    };

    if (toolButton("Seleção (V)", 0)) timelineTool_ = 0;
    ImGui::SameLine();
    if (toolButton("Faca (C)", 1)) timelineTool_ = 1;
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Checkbox("Ímã", &timelineSnapping_);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Encaixa clipes nas bordas, no cursor e no início (S)");
    ImGui::SameLine();
    if (ImGui::Button("+ Título", ImVec2(82.0f * scale, 0.0f))) addTitleAtPlayhead();
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::PushFont(fonts_.mono);
    ImGui::TextColored(kAccent, "%s", formatTimecode(project_.playhead, project_.settings.fps).c_str());
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("/ %s", formatDuration(std::max(0.0, contentEnd(project_))).c_str());

    const float zoomGroupWidth = 300.0f * scale;
    ImGui::SameLine(std::max(220.0f * scale, ImGui::GetWindowWidth() - zoomGroupWidth));
    ImGui::TextDisabled("Zoom");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f * scale);
    if (ImGui::SliderFloat("##zoom", &timelineZoom_, 0.15f, 16.0f, "%.0f%%",
                           ImGuiSliderFlags_Logarithmic)) {
        timelineZoom_ = std::clamp(timelineZoom_, 0.15f, 16.0f);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Ajustar")) {
        const float contentWidth = std::max(200.0f, ImGui::GetWindowWidth() - 210.0f * scale);
        const double end = std::max(5.0, contentEnd(project_) * 1.05);
        timelineZoom_ = (float)std::clamp((contentWidth / end) / 70.0, 0.15, 16.0);
        timelineScroll_ = 0.0;
        timelineScrollY_ = 0.0f;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ajustar zoom ao conteúdo inteiro");

    // ---------------------------------------------------------------------
    // Layout
    // ---------------------------------------------------------------------
    ImVec2 bodyMin = ImGui::GetCursorScreenPos();
    ImVec2 bodySize = ImGui::GetContentRegionAvail();
    const float footerHeight = 20.0f * scale;
    if (bodySize.y > footerHeight + 90.0f) bodySize.y -= footerHeight;
    bodySize.y = std::max(bodySize.y, 80.0f);
    ImGui::Dummy(ImVec2(bodySize.x, bodySize.y + footerHeight));

    const ImVec2 bodyMax(bodyMin.x + bodySize.x, bodyMin.y + bodySize.y);
    const ImVec2 footerMin(bodyMin.x, bodyMax.y);
    const ImVec2 footerMax(bodyMax.x, bodyMax.y + footerHeight);
    const float headerWidth = std::min(180.0f * scale, bodySize.x * 0.28f);
    const float rulerHeight = 27.0f * scale;
    const float contentLeft = bodyMin.x + headerWidth;
    const float contentRight = bodyMax.x - 8.0f;
    const float tracksTop = bodyMin.y + rulerHeight;
    const float tracksBottom = bodyMax.y;
    const float visibleWidth = std::max(40.0f, contentRight - contentLeft);
    double pixelsPerSecond = 70.0 * (double)timelineZoom_;

    std::vector<Track*> tracks = orderedTracks(project_);
    float totalTracksHeight = 0.0f;
    for (const Track* track : tracks) totalTracksHeight += (float)track->height * scale;
    const float maxScrollY = std::max(0.0f, totalTracksHeight - (tracksBottom - tracksTop));

    // Wheel: zoom, horizontal scroll and vertical scroll.
    ImVec2 mouse = io.MousePos;
    const bool mouseInBody = mouse.x >= bodyMin.x && mouse.x <= bodyMax.x && mouse.y >= bodyMin.y &&
                             mouse.y <= bodyMax.y;
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && mouseInBody) {
        if (io.MouseWheel != 0.0f) {
            if (io.KeyCtrl) {
                const double anchorTime = timelineScroll_ + (double)(mouse.x - contentLeft) / pixelsPerSecond;
                const float factor = io.MouseWheel > 0.0f ? 1.16f : 1.0f / 1.16f;
                timelineZoom_ = std::clamp(timelineZoom_ * factor, 0.15f, 16.0f);
                pixelsPerSecond = 70.0 * (double)timelineZoom_;
                timelineScroll_ = anchorTime - (double)(mouse.x - contentLeft) / pixelsPerSecond;
            } else if (io.KeyShift || io.MouseWheelH != 0.0f) {
                timelineScroll_ -= (double)io.MouseWheel * (visibleWidth / pixelsPerSecond) * 0.12;
            } else {
                timelineScrollY_ -= io.MouseWheel * 42.0f * scale;
            }
        }
        if (io.MouseWheelH != 0.0f && !io.KeyCtrl) {
            timelineScroll_ -= (double)io.MouseWheelH * (visibleWidth / pixelsPerSecond) * 0.12;
        }
    }

    const double contentEndTime = std::max(0.0, contentEnd(project_));
    const double maxScroll = std::max(0.0, contentEndTime + 30.0 - (visibleWidth / pixelsPerSecond) * 0.25);
    timelineScroll_ = std::clamp(timelineScroll_, 0.0, maxScroll);
    timelineScrollY_ = std::clamp(timelineScrollY_, 0.0f, maxScrollY);

    if (playing_) {
        const double visibleStart = timelineScroll_;
        const double visibleEnd = timelineScroll_ + visibleWidth / pixelsPerSecond;
        if (project_.playhead < visibleStart || project_.playhead > visibleEnd - 0.25) {
            timelineScroll_ = std::max(0.0, project_.playhead - (visibleWidth / pixelsPerSecond) * 0.15);
        }
    }

    auto timeToX = [&](double time) {
        return contentLeft + (float)((time - timelineScroll_) * pixelsPerSecond);
    };
    auto xToTime = [&](float x) {
        return timelineScroll_ + (double)(x - contentLeft) / pixelsPerSecond;
    };

    // Build rows after scroll clamping so hit-testing and drawing agree.
    std::vector<TrackRow> rows;
    rows.reserve(tracks.size());
    float rowY = tracksTop - timelineScrollY_;
    for (Track* track : tracks) {
        TrackRow row;
        row.track = track;
        row.y = rowY;
        row.height = (float)track->height * scale;
        rows.push_back(row);
        rowY += row.height;
    }

    auto rowAtY = [&](float y) -> TrackRow* {
        for (auto& row : rows) {
            if (y >= row.y && y < row.y + row.height) return &row;
        }
        return nullptr;
    };
    auto clipRect = [&](const Clip& clip, const TrackRow& row, ImVec2& pmin, ImVec2& pmax) {
        const float x1 = timeToX(clip.start);
        const float x2 = timeToX(clip.end());
        pmin = ImVec2(std::max(contentLeft, x1), row.y + 3.0f * scale);
        pmax = ImVec2(std::min(contentRight, x2), row.y + row.height - 3.0f * scale);
        return pmax.x > pmin.x && pmax.y > pmin.y;
    };
    auto clipAtMouse = [&](ImVec2 point, TrackRow*& outRow, Clip*& outClip) {
        outRow = nullptr;
        outClip = nullptr;
        if (point.x < contentLeft || point.x > contentRight) return;
        TrackRow* row = rowAtY(point.y);
        if (!row) return;
        for (auto& clip : project_.clips) {
            if (clip.trackId != row->track->id) continue;
            ImVec2 pmin;
            ImVec2 pmax;
            if (!clipRect(clip, *row, pmin, pmax)) continue;
            if (point.x >= pmin.x && point.x <= pmax.x) {
                outRow = row;
                outClip = &clip;
                return;
            }
        }
        outRow = row;
    };

    const bool anyPopup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    const bool hoveredWindow = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);

    // ---------------------------------------------------------------------
    // Input: ruler scrubbing and clip editing
    // ---------------------------------------------------------------------
    if (!anyPopup && hoveredWindow) {
        const bool inRuler = mouse.x >= contentLeft && mouse.x <= bodyMax.x && mouse.y >= bodyMin.y &&
                             mouse.y < tracksTop;
        if (inRuler && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            playing_ = false;
            scrubbing_ = true;
            gDrag.mode = 4;
            gDrag.clipId = 0;
            seekTo(std::max(0.0, xToTime(mouse.x)));
        } else if (gDrag.mode == 4 && mouseDown) {
            seekTo(std::max(0.0, xToTime(mouse.x)));
        }

        if (gDrag.mode == 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !inRuler &&
            mouse.y >= tracksTop && mouse.y <= tracksBottom) {
            TrackRow* row = nullptr;
            Clip* clip = nullptr;
            clipAtMouse(mouse, row, clip);
            if (row && clip) {
                const bool locked = row->track->locked;
                const bool ctrl = io.KeyCtrl;
                if (!selectionHas(selection_, clip->id) || !ctrl) selectClip(clip->id, ctrl);
                if (!locked) {
                    ImVec2 cmin;
                    ImVec2 cmax;
                    clipRect(*clip, *row, cmin, cmax);
                    const float edge = 7.0f * scale;
                    if (timelineTool_ == 1) {
                        const double cutTime = snapFrame(xToTime(mouse.x), project_.settings.fps);
                        if (splitClipAt(project_, clip->id, cutTime)) {
                            markDirty();
                            toast("Clipe dividido.");
                        }
                    } else if (selectionHas(selection_, clip->id)) {
                        playing_ = false;
                        gDrag.clipId = clip->id;
                        gDrag.moved = false;
                        if (mouse.x <= cmin.x + edge) {
                            gDrag.mode = 2;
                        } else if (mouse.x >= cmax.x - edge) {
                            gDrag.mode = 3;
                        } else {
                            gDrag.mode = 1;
                            gDrag.grabOffset = xToTime(mouse.x) - clip->start;
                        }
                    }
                }
            } else if (row) {
                clearSelection();
                playing_ = false;
                seekTo(std::max(0.0, xToTime(mouse.x)));
            }
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && mouse.y >= tracksTop && mouse.y <= tracksBottom) {
            TrackRow* row = nullptr;
            Clip* clip = nullptr;
            clipAtMouse(mouse, row, clip);
            if (clip) {
                if (!selectionHas(selection_, clip->id)) selectClip(clip->id, false);
                gContextClipId = clip->id;
                ImGui::OpenPopup("##contextoClipe");
            }
        }

        if (gDrag.mode == 1 && mouseDown && gDrag.clipId) {
            Clip* clip = findClip(project_, gDrag.clipId);
            if (clip) {
                TrackRow* targetRow = rowAtY(mouse.y);
                for (auto& row : rows) {
                    if (row.track->id == clip->trackId) {
                        if (!targetRow) targetRow = &row;
                        break;
                    }
                }
                if (targetRow && !targetRow->track->locked &&
                    trackAcceptsKind(*targetRow->track, clip->kind)) {
                    bool fits = true;
                    if (!clip->isTitle()) {
                        const Asset* asset = findAsset(project_, clip->assetId);
                        fits = asset && assetFitsTrack(*asset, *targetRow->track);
                    }
                    if (fits) {
                        double desired = xToTime(mouse.x) - gDrag.grabOffset;
                        desired = std::max(0.0, desired);
                        if (!io.KeyAlt) {
                            desired = snapTimelineTime(project_, desired, clip->id, timelineSnapping_,
                                                       pixelsPerSecond);
                        }
                        const auto range = allowedMoveRange(project_, *clip, targetRow->track->id);
                        desired = std::clamp(desired, range.first, range.second);
                        desired = snapFrame(desired, project_.settings.fps);
                        if (std::abs(desired - clip->start) > kEpsilon ||
                            targetRow->track->id != clip->trackId) {
                            if (moveClipTo(project_, clip->id, targetRow->track->id, desired)) {
                                gDrag.moved = true;
                                markDirty();
                            }
                        }
                    }
                }
            } else {
                gDrag.mode = 0;
            }
        } else if ((gDrag.mode == 2 || gDrag.mode == 3) && mouseDown && gDrag.clipId) {
            Clip* clip = findClip(project_, gDrag.clipId);
            if (clip) {
                const bool leftEdge = gDrag.mode == 2;
                double desired = xToTime(mouse.x);
                desired = snapTimelineTime(project_, desired, clip->id, timelineSnapping_ && !io.KeyAlt,
                                           pixelsPerSecond);
                const auto range = allowedTrimRange(project_, *clip, leftEdge);
                desired = std::clamp(desired, range.first, range.second);
                desired = snapFrame(desired, project_.settings.fps);
                if (trimClipEdge(project_, clip->id, leftEdge, desired)) {
                    gDrag.moved = true;
                    markDirty();
                }
            } else {
                gDrag.mode = 0;
            }
        }

        if (gDrag.mode != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (gDrag.mode == 4) scrubbing_ = false;
            gDrag.mode = 0;
            gDrag.clipId = 0;
        }
    }

    if (gDrag.mode != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        scrubbing_ = false;
        gDrag.mode = 0;
        gDrag.clipId = 0;
    }

    // ---------------------------------------------------------------------
    // Drag and drop from the media bin
    // ---------------------------------------------------------------------
    const ImRect dropRect(ImVec2(contentLeft, tracksTop), ImVec2(bodyMax.x, tracksBottom));
    if (ImGui::BeginDragDropTargetCustom(dropRect, ImGui::GetID("##linhaDoTempoDrop"))) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                "EFS_ASSET", ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
            if (payload->DataSize == (int)sizeof(uint64_t)) {
                uint64_t assetId = 0;
                std::memcpy(&assetId, payload->Data, sizeof(assetId));
                TrackRow* row = rowAtY(mouse.y);
                if (!row) {
                    for (auto& candidate : rows) {
                        if (candidate.track->video) {
                            row = &candidate;
                            break;
                        }
                    }
                }
                if (row && payload->IsDelivery()) {
                    Asset* asset = findAsset(project_, assetId);
                    if (row->track->locked) {
                        toast("A faixa está bloqueada.", 1);
                    } else if (asset) {
                        const double start = snapFrame(std::max(0.0, xToTime(std::max(mouse.x, contentLeft + 2.0f))),
                                                       project_.settings.fps);
                        const double duration = asset->info.duration > 0.0 ? asset->info.duration : 10.0;
                        std::string error;
                        if (addMediaToTrack(project_, assetId, row->track->id, start, 0.0, duration, false,
                                            error)) {
                            selectNewestClipOnTrack(row->track->id);
                            markDirty();
                            toast("Mídia inserida em " + row->track->name + ".");
                        } else {
                            toast("Não foi possível inserir: " + error, 2);
                        }
                    }
                }
            }
            ImGui::GetWindowDrawList()->AddRect(dropRect.Min, dropRect.Max, ImGui::GetColorU32(kAccent), 3.0f,
                                               0, 2.0f);
        }
        ImGui::EndDragDropTarget();
    }

    // ---------------------------------------------------------------------
    // Drawing
    // ---------------------------------------------------------------------
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(ImVec2(bodyMin.x, bodyMin.y), ImVec2(footerMax.x, footerMax.y), true);

    dl->AddRectFilled(bodyMin, bodyMax, IM_COL32(18, 20, 24, 255));
    dl->AddRectFilled(ImVec2(bodyMin.x, bodyMin.y), ImVec2(bodyMax.x, tracksTop), IM_COL32(21, 23, 28, 255));
    dl->AddRectFilled(footerMin, footerMax, IM_COL32(16, 18, 22, 255));
    dl->AddLine(ImVec2(bodyMin.x, tracksTop - 1.0f), ImVec2(bodyMax.x, tracksTop - 1.0f),
                IM_COL32(255, 255, 255, 24));

    // Track backgrounds and headers.
    for (size_t i = 0; i < rows.size(); ++i) {
        TrackRow& row = rows[i];
        const Track& track = *row.track;
        if (row.y + row.height < tracksTop || row.y > tracksBottom) continue;
        const ImU32 background = (i % 2 == 0) ? IM_COL32(24, 27, 32, 255) : IM_COL32(21, 24, 29, 255);
        dl->AddRectFilled(ImVec2(contentLeft, std::max(row.y, tracksTop)),
                          ImVec2(bodyMax.x, std::min(row.y + row.height, tracksBottom)), background);
        const ImU32 headerColor = track.video ? IM_COL32(28, 34, 45, 255) : IM_COL32(26, 38, 33, 255);
        dl->AddRectFilled(ImVec2(bodyMin.x, std::max(row.y, tracksTop)),
                          ImVec2(contentLeft, std::min(row.y + row.height, tracksBottom)), headerColor);
        dl->AddRectFilled(ImVec2(bodyMin.x, std::max(row.y, tracksTop)),
                          ImVec2(bodyMin.x + 3.0f * scale, std::min(row.y + row.height, tracksBottom)),
                          track.video ? IM_COL32(55, 120, 235, 255) : IM_COL32(44, 165, 112, 255));
        dl->AddLine(ImVec2(bodyMin.x, row.y + row.height - 1.0f),
                    ImVec2(bodyMax.x, row.y + row.height - 1.0f), IM_COL32(255, 255, 255, 12));
        if (row.y >= tracksTop && row.y + row.height <= tracksBottom) {
            dl->AddText(ImVec2(bodyMin.x + 12.0f * scale, row.y + 7.0f * scale),
                        track.locked ? IM_COL32(255, 190, 90, 200) : IM_COL32(225, 228, 235, 230),
                        track.name.c_str());
            dl->AddText(fonts_.regular, 10.5f * scale,
                        ImVec2(bodyMin.x + 40.0f * scale, row.y + 9.0f * scale),
                        track.video ? IM_COL32(120, 155, 210, 130) : IM_COL32(100, 180, 140, 130),
                        track.video ? "VÍDEO" : "ÁUDIO");
        }
    }

    // Vertical guide lines.
    const double visibleStart = timelineScroll_;
    double tickStep = 1.0;
    {
        const double target = 88.0 * scale / pixelsPerSecond;
        static const double steps[] = {1.0 / 30.0, 1.0 / 15.0, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0,
                                       15.0, 30.0, 60.0, 120.0, 300.0, 600.0, 900.0, 1800.0, 3600.0, 7200.0};
        tickStep = steps[sizeof(steps) / sizeof(steps[0]) - 1];
        for (double step : steps) {
            if (step >= target) {
                tickStep = step;
                break;
            }
        }
    }
    const long long firstTick = (long long)std::floor(visibleStart / tickStep);
    for (int i = 0; i < 2000; ++i) {
        const double time = (double)(firstTick + i) * tickStep;
        const float x = timeToX(time);
        if (x > contentRight) break;
        if (x < contentLeft - 1.0f) continue;
        const bool major = (std::llround(time / tickStep) % 1) == 0;
        dl->AddLine(ImVec2(x, tracksTop), ImVec2(x, tracksBottom),
                    major ? IM_COL32(255, 255, 255, 16) : IM_COL32(255, 255, 255, 8));
        dl->AddLine(ImVec2(x, bodyMin.y + rulerHeight - 7.0f * scale), ImVec2(x, tracksTop),
                    major ? IM_COL32(160, 168, 182, 180) : IM_COL32(120, 128, 145, 110),
                    major ? 1.3f : 1.0f);
        if (major) {
            const std::string label = tickStep < 1.0
                                          ? formatTimecode(time, project_.settings.fps, true)
                                          : formatTimecode(time, project_.settings.fps, false);
            dl->AddText(fonts_.mono, 10.0f * scale, ImVec2(x + 3.0f, bodyMin.y + 4.0f),
                        IM_COL32(160, 167, 180, 220), label.c_str());
        }
        for (int m = 1; m < 5; ++m) {
            const float mx = timeToX(time + tickStep * m / 5.0);
            if (mx < contentLeft || mx > contentRight) continue;
            dl->AddLine(ImVec2(mx, tracksTop), ImVec2(mx, tracksBottom), IM_COL32(255, 255, 255, 7));
        }
    }

    // Clips.
    for (const auto& clip : project_.clips) {
        TrackRow* row = nullptr;
        for (auto& candidate : rows) {
            if (candidate.track->id == clip.trackId) {
                row = &candidate;
                break;
            }
        }
        if (!row) continue;
        ImVec2 pmin;
        ImVec2 pmax;
        if (!clipRect(clip, *row, pmin, pmax)) continue;
        if (pmax.x < contentLeft || pmin.x > contentRight) continue;
        if (pmax.y < tracksTop || pmin.y > tracksBottom) continue;

        const bool selected = selectionHas(selection_, clip.id);
        const Track& track = *row->track;
        ImU32 base = IM_COL32(35, 88, 184, 255);
        ImU32 accent = IM_COL32(76, 146, 255, 255);
        if (clip.kind == ClipKind::Audio) {
            base = IM_COL32(25, 112, 78, 255);
            accent = IM_COL32(58, 185, 132, 255);
        } else if (clip.kind == ClipKind::Title) {
            base = IM_COL32(100, 63, 184, 255);
            accent = IM_COL32(157, 112, 255, 255);
        }
        if (track.hidden && track.video) {
            base = (base & 0x00FFFFFF) | (100u << 24);
            accent = (accent & 0x00FFFFFF) | (100u << 24);
        }
        if (track.locked) {
            base = (base & 0x00FFFFFF) | (120u << 24);
            accent = (accent & 0x00FFFFFF) | (120u << 24);
        }

        ImVec2 topMin(std::max(pmin.x, contentLeft), pmin.y);
        ImVec2 topMax(std::min(pmax.x, contentRight), pmax.y);
        dl->AddRectFilled(topMin, topMax, base, 4.0f);
        dl->AddRectFilled(topMin, ImVec2(topMax.x, std::min(topMax.y, topMin.y + 3.0f * scale)), accent, 4.0f,
                          ImDrawFlags_RoundCornersTop);
        if (selected) {
            dl->AddRect(topMin, topMax, IM_COL32(255, 255, 255, 230), 4.0f, 0, 2.0f);
        } else {
            dl->AddRect(topMin, topMax, IM_COL32(255, 255, 255, 26), 4.0f, 0, 1.0f);
        }

        // Fades as diagonal lines.
        if (clip.fadeIn > 0.001) {
            const float fx = timeToX(clip.start + clip.fadeIn);
            dl->AddLine(ImVec2(topMin.x, topMin.y), ImVec2(std::min(fx, topMax.x), topMax.y),
                        IM_COL32(0, 0, 0, 95), 2.0f);
        }
        if (clip.fadeOut > 0.001) {
            const float fx = timeToX(clip.end() - clip.fadeOut);
            dl->AddLine(ImVec2(std::max(fx, topMin.x), topMin.y), ImVec2(topMax.x, topMax.y),
                        IM_COL32(0, 0, 0, 95), 2.0f);
        }

        // Audio peaks (rendered when the media engine has filled them).
        if (clip.kind == ClipKind::Audio) {
            const Asset* asset = findAsset(project_, clip.assetId);
            const float centerY = (topMin.y + topMax.y) * 0.5f;
            if (asset && !asset->peaks.empty()) {
                const int columns = (int)std::min(400.0f, std::max(1.0f, topMax.x - topMin.x));
                for (int c = 0; c < columns; ++c) {
                    const double frac = (double)c / (double)columns;
                    const double src = clip.srcIn + clip.duration() * frac;
                    const double assetDuration = std::max(0.001, asset->info.duration);
                    size_t index = (size_t)((src / assetDuration) * (double)asset->peaks.size());
                    index = std::min(index, asset->peaks.size() - 1);
                    const float peak = std::clamp(asset->peaks[index], 0.0f, 1.0f);
                    const float x = topMin.x + (topMax.x - topMin.x) * (float)c / (float)columns;
                    dl->AddLine(ImVec2(x, centerY - peak * (topMax.y - topMin.y) * 0.4f),
                                ImVec2(x, centerY + peak * (topMax.y - topMin.y) * 0.4f),
                                IM_COL32(180, 255, 220, 110), 1.0f);
                }
            } else {
                dl->AddLine(ImVec2(topMin.x + 4.0f, centerY), ImVec2(topMax.x - 4.0f, centerY),
                            IM_COL32(200, 255, 225, 55), 1.0f);
            }
        }

        const float textTop = topMin.y + 5.0f * scale;
        dl->PushClipRect(ImVec2(topMin.x + 4.0f, topMin.y), ImVec2(topMax.x - 4.0f, topMax.y), true);
        const std::string label = clipLabel(project_, clip);
        dl->AddText(fonts_.bold, 12.0f * scale, ImVec2(topMin.x + 7.0f, textTop),
                    IM_COL32(255, 255, 255, 235), label.c_str());
        if (topMax.x - topMin.x > 72.0f && topMax.y - topMin.y > 30.0f) {
            const std::string meta = formatDuration(clip.duration());
            dl->AddText(fonts_.regular, 10.0f * scale, ImVec2(topMin.x + 7.0f, textTop + 16.0f * scale),
                        IM_COL32(255, 255, 255, 140), meta.c_str());
        }
        dl->PopClipRect();

        if (selected) {
            const float handleW = 4.0f * scale;
            dl->AddRectFilled(ImVec2(topMin.x, topMin.y), ImVec2(topMin.x + handleW, topMax.y),
                              IM_COL32(255, 255, 255, 130));
            dl->AddRectFilled(ImVec2(topMax.x - handleW, topMin.y), ImVec2(topMax.x, topMax.y),
                              IM_COL32(255, 255, 255, 130));
        }
    }

    // Empty state.
    if (project_.clips.empty()) {
        const char* text = "Arraste mídia para cá ou importe pelo painel Mídia";
        const ImVec2 size = ImGui::CalcTextSize(text);
        dl->AddText(ImVec2((contentLeft + contentRight - size.x) * 0.5f, (tracksTop + tracksBottom) * 0.5f - 8.0f),
                    IM_COL32(255, 255, 255, 48), text);
    }

    // Playhead.
    const float playheadX = timeToX(project_.playhead);
    if (playheadX >= contentLeft - 1.0f && playheadX <= contentRight + 1.0f) {
        dl->AddLine(ImVec2(playheadX, bodyMin.y + 2.0f), ImVec2(playheadX, tracksBottom),
                    ImGui::GetColorU32(kAccent), 1.6f);
        dl->AddTriangleFilled(ImVec2(playheadX - 5.0f, bodyMin.y + 2.0f),
                              ImVec2(playheadX + 5.0f, bodyMin.y + 2.0f),
                              ImVec2(playheadX, bodyMin.y + 9.0f), ImGui::GetColorU32(kAccent));
    }

    dl->PopClipRect();

    // Footer hints.
    dl->AddText(ImVec2(footerMin.x + 8.0f, footerMin.y + 3.0f), IM_COL32(120, 128, 142, 200),
                "Shift+roda: rolar  •  Ctrl+roda: zoom  •  C: faca  •  V: seleção  •  S: ímã  •  Alt: ignora ímã");
    char info[128];
    std::snprintf(info, sizeof(info), "%d faixa(s)  •  %d clipe(s)", (int)rows.size(), (int)project_.clips.size());
    const ImVec2 infoSize = ImGui::CalcTextSize(info);
    dl->AddText(ImVec2(footerMax.x - infoSize.x - 8.0f, footerMin.y + 3.0f), IM_COL32(120, 128, 142, 200),
                info);

    // ---------------------------------------------------------------------
    // Track header controls
    // ---------------------------------------------------------------------
    const float controlY = 30.0f * scale;
    for (auto& row : rows) {
        if (row.y < tracksTop - row.height || row.y > tracksBottom) continue;
        Track& track = *row.track;
        ImGui::PushID((int)track.id);
        const ImVec2 buttonPos(bodyMin.x + 10.0f * scale, row.y + controlY);
        ImGui::SetCursorScreenPos(buttonPos);
        const bool toggleVisible = ImGui::InvisibleButton("##visibilidade", ImVec2(22.0f * scale, 18.0f * scale));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(track.video ? (track.hidden ? "Mostrar vídeo" : "Ocultar vídeo")
                                          : (track.muted ? "Ativar áudio" : "Silenciar áudio"));
        }
        const ImU32 iconColor = track.video
                                    ? (track.hidden ? IM_COL32(120, 126, 140, 180) : IM_COL32(210, 216, 228, 220))
                                    : (track.muted ? IM_COL32(220, 110, 110, 210) : IM_COL32(210, 216, 228, 220));
        if (track.video) {
            drawEye(dl, ImVec2(buttonPos.x + 10.0f * scale, buttonPos.y + 9.0f * scale), !track.hidden,
                    iconColor);
        } else {
            drawSpeaker(dl, ImVec2(buttonPos.x + 10.0f * scale, buttonPos.y + 9.0f * scale), track.muted,
                        iconColor);
        }
        if (toggleVisible) {
            if (track.video) {
                track.hidden = !track.hidden;
            } else {
                track.muted = !track.muted;
            }
            markDirty();
        }

        ImGui::SetCursorScreenPos(ImVec2(buttonPos.x + 28.0f * scale, buttonPos.y));
        const bool toggleLock = ImGui::InvisibleButton("##bloqueio", ImVec2(22.0f * scale, 18.0f * scale));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(track.locked ? "Desbloquear faixa" : "Bloquear faixa");
        drawLock(dl, ImVec2(buttonPos.x + 39.0f * scale, buttonPos.y + 8.0f * scale), track.locked,
                 track.locked ? IM_COL32(255, 190, 90, 230) : IM_COL32(160, 168, 182, 190));
        if (toggleLock) {
            track.locked = !track.locked;
            markDirty();
        }
        ImGui::PopID();
    }

    // Cursor feedback.
    if (hoveredWindow && !anyPopup && mouse.x >= contentLeft && mouse.y >= tracksTop && mouse.y <= tracksBottom) {
        TrackRow* row = nullptr;
        Clip* clip = nullptr;
        clipAtMouse(mouse, row, clip);
        if (clip && row && !row->track->locked) {
            ImVec2 pmin;
            ImVec2 pmax;
            clipRect(*clip, *row, pmin, pmax);
            const float edge = 7.0f * scale;
            if (mouse.x <= pmin.x + edge || mouse.x >= pmax.x - edge) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            } else if (timelineTool_ == 1) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            } else {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
        } else if (timelineTool_ == 1) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
    }

    // Clip context menu.
    if (ImGui::BeginPopup("##contextoClipe")) {
        if (Clip* clip = findClip(project_, gContextClipId)) {
            ImGui::TextDisabled("%s", clipLabel(project_, *clip).c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Dividir no cursor", "Ctrl+K")) {
                if (splitClipAt(project_, clip->id, project_.playhead)) {
                    markDirty();
                    toast("Clipe dividido.");
                } else {
                    toast("O cursor precisa estar dentro do clipe.", 1);
                }
            }
            if (ImGui::MenuItem("Duplicar", "Ctrl+D")) {
                duplicateClips(project_, {clip->id});
                markDirty();
            }
            if (ImGui::MenuItem("Excluir", "Del")) {
                removeClips(project_, {clip->id}, false);
                clearSelection();
                markDirty();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Fade de entrada")) {
                clip->fadeIn = std::min(0.5, clip->duration() * 0.5);
                markDirty();
            }
            if (ImGui::MenuItem("Fade de saída")) {
                clip->fadeOut = std::min(0.5, clip->duration() * 0.5);
                markDirty();
            }
            if (ImGui::MenuItem("Centralizar transformação")) {
                clip->scale = 1.0;
                clip->posX = 0.5;
                clip->posY = 0.5;
                markDirty();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Abrir no Inspetor")) ImGui::SetWindowFocus("Inspetor###inspetor");
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace efs
