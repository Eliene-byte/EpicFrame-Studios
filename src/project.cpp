#include "project.h"

#include "platform.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

using nlohmann::json;

namespace efs {

Track* findTrack(Project& project, uint64_t id) {
    for (auto& track : project.tracks) {
        if (track.id == id) return &track;
    }
    return nullptr;
}

const Track* findTrack(const Project& project, uint64_t id) {
    for (const auto& track : project.tracks) {
        if (track.id == id) return &track;
    }
    return nullptr;
}

Asset* findAsset(Project& project, uint64_t id) {
    for (auto& asset : project.assets) {
        if (asset.id == id) return &asset;
    }
    return nullptr;
}

const Asset* findAsset(const Project& project, uint64_t id) {
    for (const auto& asset : project.assets) {
        if (asset.id == id) return &asset;
    }
    return nullptr;
}

Clip* findClip(Project& project, uint64_t id) {
    for (auto& clip : project.clips) {
        if (clip.id == id) return &clip;
    }
    return nullptr;
}

const Clip* findClip(const Project& project, uint64_t id) {
    for (const auto& clip : project.clips) {
        if (clip.id == id) return &clip;
    }
    return nullptr;
}

bool isVideoTrack(const Project& project, uint64_t trackId) {
    const Track* track = findTrack(project, trackId);
    return track && track->video;
}

double contentEnd(const Project& project) {
    double end = 0.0;
    for (const auto& clip : project.clips) {
        end = std::max(end, clip.end());
    }
    return end;
}

std::vector<const Clip*> clipsOnTrack(const Project& project, uint64_t trackId) {
    std::vector<const Clip*> result;
    for (const auto& clip : project.clips) {
        if (clip.trackId == trackId) result.push_back(&clip);
    }
    std::sort(result.begin(), result.end(),
              [](const Clip* a, const Clip* b) { return a->start < b->start; });
    return result;
}

const Clip* clipAt(const Project& project, uint64_t trackId, double t) {
    for (const auto& clip : project.clips) {
        if (clip.trackId == trackId && clip.contains(t)) return &clip;
    }
    return nullptr;
}

double trackEnd(const Project& project, uint64_t trackId, uint64_t ignoreClip) {
    double end = 0.0;
    for (const auto& clip : project.clips) {
        if (clip.trackId == trackId && clip.id != ignoreClip) end = std::max(end, clip.end());
    }
    return end;
}

std::string defaultTrackName(const Project& project, bool video) {
    int count = 0;
    for (const auto& track : project.tracks) {
        if (track.video == video) ++count;
    }
    return (video ? "V" : "A") + std::to_string(count + 1);
}

void initDefaultProject(Project& project) {
    project = Project{};
    project.settings = SequenceSettings{};

    Track v1;
    v1.id = project.newId();
    v1.name = "V1";
    v1.video = true;
    v1.height = 58;
    project.tracks.push_back(v1);

    Track v2;
    v2.id = project.newId();
    v2.name = "V2";
    v2.video = true;
    v2.height = 58;
    project.tracks.push_back(v2);

    Track a1;
    a1.id = project.newId();
    a1.name = "A1";
    a1.video = false;
    a1.height = 48;
    project.tracks.push_back(a1);
}

uint64_t addTrack(Project& project, bool video) {
    Track track;
    track.id = project.newId();
    track.video = video;
    track.name = defaultTrackName(project, video);
    track.height = video ? 58 : 48;
    if (video) {
        auto pos = std::find_if(project.tracks.begin(), project.tracks.end(),
                                [](const Track& t) { return !t.video; });
        project.tracks.insert(pos, track);
    } else {
        project.tracks.push_back(track);
    }
    project.dirty = true;
    return track.id;
}

bool trackAcceptsKind(const Track& track, ClipKind kind) {
    if (track.video) return kind == ClipKind::Video || kind == ClipKind::Title;
    return kind == ClipKind::Audio;
}

bool assetFitsTrack(const Asset& asset, const Track& track) {
    if (track.video) return asset.info.hasVideo;
    return asset.info.hasAudio;
}

std::string uniqueAssetName(const Project& project, const std::string& name) {
    std::string base = name.empty() ? "Mídia" : name;
    bool taken = true;
    int suffix = 1;
    std::string candidate = base;
    while (taken) {
        taken = false;
        for (const auto& asset : project.assets) {
            if (asset.name == candidate) {
                taken = true;
                break;
            }
        }
        if (taken) {
            ++suffix;
            candidate = base + " (" + std::to_string(suffix) + ")";
        }
    }
    return candidate;
}

Clip makeMediaClip(const Project& project, uint64_t assetId, uint64_t trackId, double start,
                   double srcIn, double srcOut) {
    Clip clip;
    clip.id = 0;
    clip.assetId = assetId;
    clip.trackId = trackId;
    clip.start = std::max(0.0, start);

    const Asset* asset = findAsset(project, assetId);
    const Track* track = findTrack(project, trackId);
    if (!asset || !track) return clip;

    clip.kind = track->video ? ClipKind::Video : ClipKind::Audio;
    clip.srcIn = std::max(0.0, srcIn);
    double maxSource = asset->info.duration;
    if (asset->info.isImage && maxSource <= 0.05) maxSource = 10.0;
    if (maxSource <= 0.001) maxSource = 10.0;
    if (srcOut <= clip.srcIn + kEpsilon || srcOut > maxSource + 0.01) srcOut = maxSource;
    if (srcOut <= clip.srcIn + kEpsilon) srcOut = maxSource;
    clip.srcOut = std::max(clip.srcIn + 1.0 / std::max(1.0, project.settings.fps),
                           std::min(srcOut, maxSource));
    return clip;
}

Clip makeTitleClip(const Project& project, uint64_t trackId, double start, double duration) {
    Clip clip;
    clip.assetId = 0;
    clip.trackId = trackId;
    clip.kind = ClipKind::Title;
    clip.start = std::max(0.0, start);
    clip.srcIn = 0.0;
    clip.srcOut = std::max(1.0 / std::max(1.0, project.settings.fps), duration);
    clip.text = "Seu texto";
    clip.fontSize = 96;
    clip.color = 0xFFFFFFFFu;
    clip.align = 1;
    clip.bold = true;
    clip.shadow = true;
    return clip;
}

bool clipOverlaps(const Project& project, const Clip& clip, uint64_t ignoreClipId) {
    const double a0 = clip.start;
    const double a1 = clip.end();
    for (const auto& other : project.clips) {
        if (other.id == ignoreClipId || other.trackId != clip.trackId) continue;
        const double b0 = other.start;
        const double b1 = other.end();
        if (a0 < b1 - kEpsilon && a1 > b0 + kEpsilon) return true;
    }
    return false;
}

static void cutTrackRange(Project& project, uint64_t trackId, double start, double end) {
    std::vector<Clip> result;
    result.reserve(project.clips.size() + 2);
    for (const auto& clip : project.clips) {
        if (clip.trackId != trackId || clip.end() <= start + kEpsilon || clip.start >= end - kEpsilon) {
            result.push_back(clip);
            continue;
        }
        const double clipEnd = clip.end();
        if (clip.start < start - kEpsilon) {
            Clip left = clip;
            const double duration = start - clip.start;
            if (left.kind == ClipKind::Title) {
                left.srcIn = 0.0;
                left.srcOut = duration;
            } else {
                left.srcOut = clip.srcIn + duration;
            }
            result.push_back(left);
        }
        if (clipEnd > end + kEpsilon) {
            Clip right = clip;
            right.id = project.newId();
            right.start = end;
            if (right.kind == ClipKind::Title) {
                right.srcIn = 0.0;
                right.srcOut = clipEnd - end;
            } else {
                const double cut = end - clip.start;
                right.srcIn = clip.srcIn + cut;
            }
            result.push_back(right);
        }
    }
    project.clips = std::move(result);
}

static void rippleInsertAllTracks(Project& project, double start, double amount) {
    std::vector<Clip> result;
    result.reserve(project.clips.size() + 4);
    for (const auto& clip : project.clips) {
        const double clipEnd = clip.end();
        if (clipEnd <= start + kEpsilon) {
            result.push_back(clip);
        } else if (clip.start >= start - kEpsilon) {
            Clip moved = clip;
            moved.start += amount;
            result.push_back(moved);
        } else {
            Clip left = clip;
            const double leftDuration = start - clip.start;
            if (left.kind == ClipKind::Title) {
                left.srcIn = 0.0;
                left.srcOut = leftDuration;
            } else {
                left.srcOut = clip.srcIn + leftDuration;
            }
            result.push_back(left);

            Clip right = clip;
            right.id = project.newId();
            right.start = start + amount;
            if (right.kind == ClipKind::Title) {
                right.srcIn = 0.0;
                right.srcOut = clipEnd - start;
            } else {
                right.srcIn = left.srcOut;
            }
            result.push_back(right);
        }
    }
    project.clips = std::move(result);
}

bool addMediaToTrack(Project& project, uint64_t assetId, uint64_t trackId, double start,
                     double srcIn, double srcOut, bool ripple, std::string& error) {
    Asset* asset = findAsset(project, assetId);
    Track* track = findTrack(project, trackId);
    if (!asset) {
        error = "Mídia não encontrada.";
        return false;
    }
    if (!track) {
        error = "Faixa não encontrada.";
        return false;
    }
    if (!assetFitsTrack(*asset, *track)) {
        error = track->video ? "Esta mídia não possui vídeo." : "Esta mídia não possui áudio.";
        return false;
    }
    Clip clip = makeMediaClip(project, assetId, trackId, start, srcIn, srcOut);
    if (clip.duration() <= kEpsilon) {
        error = "Duração inválida.";
        return false;
    }
    clip.id = project.newId();

    if (ripple) {
        rippleInsertAllTracks(project, clip.start, clip.duration());
    } else {
        cutTrackRange(project, trackId, clip.start, clip.end());
    }
    project.clips.push_back(clip);
    sortClips(project);
    project.dirty = true;
    return true;
}

bool addTitleToTrack(Project& project, uint64_t trackId, double start, double duration,
                     std::string& error) {
    Track* track = findTrack(project, trackId);
    if (!track || !track->video) {
        error = "Escolha uma faixa de vídeo para o título.";
        return false;
    }
    if (duration <= kEpsilon) {
        error = "Duração inválida.";
        return false;
    }
    Clip clip = makeTitleClip(project, trackId, start, duration);
    clip.id = project.newId();
    cutTrackRange(project, trackId, clip.start, clip.end());
    project.clips.push_back(clip);
    sortClips(project);
    project.dirty = true;
    return true;
}

bool splitClipAt(Project& project, uint64_t clipId, double timelineTime) {
    Clip* original = findClip(project, clipId);
    if (!original) return false;
    const double minDuration = 1.0 / std::max(1.0, project.settings.fps);
    if (timelineTime <= original->start + minDuration - kEpsilon ||
        timelineTime >= original->end() - minDuration + kEpsilon) {
        return false;
    }

    Clip right = *original;
    right.id = project.newId();
    right.start = timelineTime;
    const double leftDuration = timelineTime - original->start;
    const double originalSrcOut = original->srcOut;
    if (original->kind == ClipKind::Title) {
        right.srcIn = 0.0;
        right.srcOut = original->end() - timelineTime;
        original->srcOut = leftDuration;
    } else {
        original->srcOut = original->srcIn + leftDuration;
        right.srcIn = original->srcOut;
        right.srcOut = originalSrcOut;
    }
    project.clips.push_back(right);
    sortClips(project);
    project.dirty = true;
    return true;
}

int splitClipsAt(Project& project, double timelineTime, const std::vector<uint64_t>& onlyThese) {
    std::vector<uint64_t> ids;
    for (const auto& clip : project.clips) {
        if (!onlyThese.empty() &&
            std::find(onlyThese.begin(), onlyThese.end(), clip.id) == onlyThese.end()) {
            continue;
        }
        if (clip.start < timelineTime - kEpsilon && clip.end() > timelineTime + kEpsilon) {
            ids.push_back(clip.id);
        }
    }
    int count = 0;
    for (uint64_t id : ids) {
        if (splitClipAt(project, id, timelineTime)) ++count;
    }
    return count;
}

bool removeClips(Project& project, const std::vector<uint64_t>& clipIds, bool ripple) {
    if (clipIds.empty()) return false;
    std::set<uint64_t> ids(clipIds.begin(), clipIds.end());
    int removed = 0;

    if (ripple) {
        std::map<uint64_t, double> removedPerTrack;
        for (const auto& clip : project.clips) {
            if (ids.count(clip.id)) {
                removedPerTrack[clip.trackId] += clip.duration();
                ++removed;
            }
        }
        std::vector<Clip> result;
        result.reserve(project.clips.size());
        std::map<uint64_t, double> shiftPerTrack;
        std::vector<Clip> ordered = project.clips;
        std::sort(ordered.begin(), ordered.end(), [](const Clip& a, const Clip& b) {
            if (a.trackId != b.trackId) return a.trackId < b.trackId;
            return a.start < b.start;
        });
        for (const auto& clip : ordered) {
            if (ids.count(clip.id)) {
                shiftPerTrack[clip.trackId] += clip.duration();
                continue;
            }
            Clip moved = clip;
            const double shift = shiftPerTrack[clip.trackId];
            if (shift > 0.0) moved.start = std::max(0.0, moved.start - shift);
            result.push_back(moved);
        }
        project.clips = std::move(result);
    } else {
        std::vector<Clip> result;
        result.reserve(project.clips.size());
        for (const auto& clip : project.clips) {
            if (ids.count(clip.id)) {
                ++removed;
            } else {
                result.push_back(clip);
            }
        }
        project.clips = std::move(result);
    }
    if (removed == 0) return false;
    project.dirty = true;
    return true;
}

bool moveClipTo(Project& project, uint64_t clipId, uint64_t trackId, double newStart) {
    Clip* clip = findClip(project, clipId);
    Track* track = findTrack(project, trackId);
    if (!clip || !track) return false;
    if (!trackAcceptsKind(*track, clip->kind)) return false;
    if (newStart < -kEpsilon) return false;
    if (clip->kind != ClipKind::Title) {
        const Asset* asset = findAsset(project, clip->assetId);
        if (!asset || !assetFitsTrack(*asset, *track)) return false;
    }
    if (clip->kind == ClipKind::Title && !track->video) return false;

    Clip test = *clip;
    test.trackId = trackId;
    test.start = std::max(0.0, newStart);
    if (clipOverlaps(project, test, clip->id)) return false;
    clip->trackId = trackId;
    clip->start = test.start;
    sortClips(project);
    project.dirty = true;
    return true;
}

bool trimClipEdge(Project& project, uint64_t clipId, bool leftEdge, double newTimelineTime) {
    Clip* clip = findClip(project, clipId);
    if (!clip) return false;
    const double fps = std::max(1.0, project.settings.fps);
    const double minDuration = 1.0 / fps;
    const double end = clip->end();
    const Asset* asset = clip->kind == ClipKind::Title ? nullptr : findAsset(project, clip->assetId);
    if (clip->kind != ClipKind::Title && !asset) return false;

    Clip modified = *clip;
    if (leftEdge) {
        double newStart = std::max(0.0, std::min(newTimelineTime, end - minDuration));
        if (modified.kind == ClipKind::Title) {
            modified.srcIn = 0.0;
            modified.srcOut = end - newStart;
        } else {
            const double delta = newStart - modified.start;
            const double newSrcIn = modified.srcIn + delta;
            if (newSrcIn < -kEpsilon) return false;
            if (newSrcIn > modified.srcOut - minDuration + kEpsilon) return false;
            if (asset->info.duration > 0.0 && newSrcIn > asset->info.duration) return false;
            modified.srcIn = std::max(0.0, newSrcIn);
            modified.srcOut = modified.srcIn + (end - newStart);
        }
        modified.start = newStart;
    } else {
        if (newTimelineTime <= modified.start + minDuration - kEpsilon) return false;
        double newEnd = newTimelineTime;
        if (modified.kind == ClipKind::Title) {
            modified.srcIn = 0.0;
            modified.srcOut = newEnd - modified.start;
        } else {
            const double maxEnd = asset->info.duration > 0.0
                                      ? modified.start + (asset->info.duration - modified.srcIn)
                                      : newEnd;
            newEnd = std::min(newEnd, maxEnd);
            modified.srcOut = modified.srcIn + (newEnd - modified.start);
        }
        if (modified.srcOut <= modified.srcIn + kEpsilon) return false;
    }

    if (clipOverlaps(project, modified, clip->id)) return false;
    *clip = modified;
    sortClips(project);
    project.dirty = true;
    return true;
}

std::pair<double, double> allowedMoveRange(const Project& project, const Clip& clip,
                                           uint64_t trackId) {
    const double duration = clip.duration();
    std::vector<std::pair<double, double>> occupied;
    for (const auto& other : project.clips) {
        if (other.trackId != trackId || other.id == clip.id) continue;
        if (other.end() > other.start + kEpsilon) occupied.emplace_back(other.start, other.end());
    }
    std::sort(occupied.begin(), occupied.end());
    std::vector<std::pair<double, double>> merged;
    for (const auto& interval : occupied) {
        if (merged.empty() || interval.first > merged.back().second + kEpsilon) {
            merged.push_back(interval);
        } else {
            merged.back().second = std::max(merged.back().second, interval.second);
        }
    }

    const double farEnd = std::max(contentEnd(project) + 600.0, 600.0);
    std::vector<std::pair<double, double>> gaps;
    double cursor = 0.0;
    for (const auto& interval : merged) {
        const double gapEnd = std::min(interval.first, farEnd);
        if (gapEnd > cursor + kEpsilon) gaps.emplace_back(cursor, gapEnd);
        cursor = std::max(cursor, interval.second);
        if (cursor >= farEnd) break;
    }
    if (cursor < farEnd) gaps.emplace_back(cursor, farEnd);

    std::pair<double, double> best = {clip.start, clip.start};
    double bestDistance = 1e30;
    bool found = false;
    for (const auto& gap : gaps) {
        if (gap.second - gap.first < duration - kEpsilon) continue;
        const double lo = gap.first;
        const double hi = gap.second - duration;
        const double candidate = std::max(lo, std::min(clip.start, hi));
        const double distance = std::abs(candidate - clip.start);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = {lo, hi};
            found = true;
        }
    }
    if (!found) return {clip.start, clip.start};
    return best;
}

std::pair<double, double> allowedTrimRange(const Project& project, const Clip& clip, bool leftEdge) {
    const double fps = std::max(1.0, project.settings.fps);
    const double minDuration = 1.0 / fps;
    const Clip* prev = nullptr;
    const Clip* next = nullptr;
    for (const auto& other : project.clips) {
        if (other.trackId != clip.trackId || other.id == clip.id) continue;
        if (other.end() <= clip.start + kEpsilon) {
            if (!prev || other.end() > prev->end()) prev = &other;
        } else if (other.start >= clip.end() - kEpsilon) {
            if (!next || other.start < next->start) next = &other;
        }
    }
    const double prevEnd = prev ? prev->end() : 0.0;
    const double nextStart = next ? next->start : 1e12;
    const Asset* asset = clip.kind == ClipKind::Title ? nullptr : findAsset(project, clip.assetId);

    if (leftEdge) {
        double lo = prevEnd;
        if (asset && asset->info.duration > 0.0) {
            lo = std::max(lo, clip.start - clip.srcIn);
        }
        const double hi = clip.end() - minDuration;
        return {lo, std::max(lo, hi)};
    }

    double hi = nextStart;
    if (asset && asset->info.duration > 0.0) {
        hi = std::min(hi, clip.start + (asset->info.duration - clip.srcIn));
    }
    const double lo = clip.start + minDuration;
    return {lo, std::max(lo, hi)};
}

void closeGaps(Project& project, uint64_t trackId) {
    auto clips = clipsOnTrack(project, trackId);
    double cursor = 0.0;
    for (const Clip* c : clips) {
        Clip* clip = findClip(project, c->id);
        if (!clip) continue;
        clip->start = cursor;
        cursor += clip->duration();
    }
    project.dirty = true;
}

void duplicateClips(Project& project, const std::vector<uint64_t>& clipIds) {
    for (uint64_t id : clipIds) {
        const Clip* source = findClip(project, id);
        if (!source) continue;
        Clip copy = *source;
        copy.id = project.newId();
        const double preferred = copy.end();
        copy.start = preferred;
        const auto range = allowedMoveRange(project, copy, copy.trackId);
        copy.start = std::max(range.first, std::min(preferred, range.second));
        project.clips.push_back(copy);
    }
    sortClips(project);
    project.dirty = true;
}

void sortClips(Project& project) {
    std::unordered_map<uint64_t, int> trackOrder;
    for (size_t i = 0; i < project.tracks.size(); ++i) {
        trackOrder[project.tracks[i].id] = (int)i;
    }
    std::stable_sort(project.clips.begin(), project.clips.end(), [&](const Clip& a, const Clip& b) {
        const int ta = trackOrder.count(a.trackId) ? trackOrder[a.trackId] : 0;
        const int tb = trackOrder.count(b.trackId) ? trackOrder[b.trackId] : 0;
        if (ta != tb) return ta < tb;
        return a.start < b.start;
    });
}

static json trackToJson(const Track& track) {
    return json{{"id", track.id},
                {"name", track.name},
                {"video", track.video},
                {"muted", track.muted},
                {"locked", track.locked},
                {"hidden", track.hidden},
                {"height", track.height}};
}

static json assetToJson(const Asset& asset) {
    const auto& info = asset.info;
    return json{{"id", asset.id},
                {"path", asset.path},
                {"name", asset.name},
                {"info",
                 {{"duration", info.duration},
                  {"width", info.width},
                  {"height", info.height},
                  {"fps", info.fps},
                  {"hasVideo", info.hasVideo},
                  {"hasAudio", info.hasAudio},
                  {"isImage", info.isImage},
                  {"sampleRate", info.sampleRate},
                  {"channels", info.channels},
                  {"videoCodec", info.videoCodec},
                  {"audioCodec", info.audioCodec}}}};
}

static json clipToJson(const Clip& clip) {
    return json{{"id", clip.id},
                {"assetId", clip.assetId},
                {"trackId", clip.trackId},
                {"kind", (int)clip.kind},
                {"start", clip.start},
                {"srcIn", clip.srcIn},
                {"srcOut", clip.srcOut},
                {"volume", clip.volume},
                {"opacity", clip.opacity},
                {"fadeIn", clip.fadeIn},
                {"fadeOut", clip.fadeOut},
                {"brightness", clip.brightness},
                {"contrast", clip.contrast},
                {"saturation", clip.saturation},
                {"scale", clip.scale},
                {"posX", clip.posX},
                {"posY", clip.posY},
                {"text", clip.text},
                {"fontSize", clip.fontSize},
                {"color", clip.color},
                {"align", clip.align},
                {"bold", clip.bold},
                {"shadow", clip.shadow}};
}

std::string projectToJson(const Project& project) {
    json j;
    j["format"] = "epicframe-project";
    j["version"] = 1;
    j["name"] = project.settings.name;
    j["width"] = project.settings.width;
    j["height"] = project.settings.height;
    j["fps"] = project.settings.fps;
    j["nextId"] = project.nextId;
    j["playhead"] = project.playhead;
    j["inMark"] = project.inMark;
    j["outMark"] = project.outMark;
    j["tracks"] = json::array();
    for (const auto& track : project.tracks) j["tracks"].push_back(trackToJson(track));
    j["assets"] = json::array();
    for (const auto& asset : project.assets) j["assets"].push_back(assetToJson(asset));
    j["clips"] = json::array();
    for (const auto& clip : project.clips) j["clips"].push_back(clipToJson(clip));
    return j.dump(2);
}

static double readNumber(const json& j, const char* key, double fallback) {
    if (!j.contains(key) || !j[key].is_number()) return fallback;
    return j[key].get<double>();
}

static int readInt(const json& j, const char* key, int fallback) {
    if (!j.contains(key) || !j[key].is_number()) return fallback;
    return j[key].get<int>();
}

static bool readBool(const json& j, const char* key, bool fallback) {
    if (!j.contains(key) || !j[key].is_boolean()) return fallback;
    return j[key].get<bool>();
}

static std::string readString(const json& j, const char* key, const std::string& fallback) {
    if (!j.contains(key) || !j[key].is_string()) return fallback;
    return j[key].get<std::string>();
}

bool projectFromJson(Project& project, const std::string& text, std::string& error) {
    json j;
    try {
        j = json::parse(text);
    } catch (const std::exception& ex) {
        error = std::string("JSON inválido: ") + ex.what();
        return false;
    }
    if (!j.is_object()) {
        error = "O arquivo de projeto não é um objeto válido.";
        return false;
    }
    const std::string format = readString(j, "format", "epicframe-project");
    if (format != "epicframe-project") {
        error = "Este arquivo não é um projeto do EpicFrame Studios.";
        return false;
    }

    Project p;
    p.settings.name = readString(j, "name", "Sem título");
    p.settings.width = std::max(16, readInt(j, "width", 1920));
    p.settings.height = std::max(16, readInt(j, "height", 1080));
    p.settings.fps = std::clamp(readNumber(j, "fps", 30.0), 1.0, 240.0);
    p.nextId = std::max<uint64_t>(1, (uint64_t)std::max<int64_t>(1, j.value("nextId", (int64_t)1)));
    p.playhead = std::max(0.0, readNumber(j, "playhead", 0.0));
    p.inMark = readNumber(j, "inMark", -1.0);
    p.outMark = readNumber(j, "outMark", -1.0);

    if (j.contains("tracks") && j["tracks"].is_array()) {
        for (const auto& t : j["tracks"]) {
            if (!t.is_object()) continue;
            Track track;
            track.id = (uint64_t)std::max<int64_t>(1, t.value("id", (int64_t)1));
            track.name = readString(t, "name", "Faixa");
            track.video = readBool(t, "video", true);
            track.muted = readBool(t, "muted", false);
            track.locked = readBool(t, "locked", false);
            track.hidden = readBool(t, "hidden", false);
            track.height = std::clamp(readInt(t, "height", track.video ? 58 : 48), 32, 160);
            p.tracks.push_back(track);
        }
    }
    if (p.tracks.empty()) {
        initDefaultProject(p);
    }
    bool hasVideoTrack = false;
    for (const auto& t : p.tracks) hasVideoTrack = hasVideoTrack || t.video;
    if (!hasVideoTrack) {
        Track v;
        v.id = p.newId();
        v.name = "V1";
        v.video = true;
        v.height = 58;
        p.tracks.insert(p.tracks.begin(), v);
    }

    if (j.contains("assets") && j["assets"].is_array()) {
        for (const auto& a : j["assets"]) {
            if (!a.is_object()) continue;
            Asset asset;
            asset.id = (uint64_t)std::max<int64_t>(1, a.value("id", (int64_t)1));
            asset.path = readString(a, "path", "");
            if (asset.path.empty()) continue;
            asset.name = readString(a, "name", fileName(asset.path));
            if (a.contains("info") && a["info"].is_object()) {
                const auto& info = a["info"];
                asset.info.duration = std::max(0.0, readNumber(info, "duration", 0.0));
                asset.info.width = std::max(0, readInt(info, "width", 0));
                asset.info.height = std::max(0, readInt(info, "height", 0));
                asset.info.fps = std::max(1.0, readNumber(info, "fps", 30.0));
                asset.info.hasVideo = readBool(info, "hasVideo", true);
                asset.info.hasAudio = readBool(info, "hasAudio", false);
                asset.info.isImage = readBool(info, "isImage", false);
                asset.info.sampleRate = readInt(info, "sampleRate", 0);
                asset.info.channels = readInt(info, "channels", 0);
                asset.info.videoCodec = readString(info, "videoCodec", "");
                asset.info.audioCodec = readString(info, "audioCodec", "");
            }
            asset.missing = !fileExists(asset.path);
            p.assets.push_back(asset);
        }
    }

    if (j.contains("clips") && j["clips"].is_array()) {
        for (const auto& c : j["clips"]) {
            if (!c.is_object()) continue;
            Clip clip;
            clip.id = (uint64_t)std::max<int64_t>(1, c.value("id", (int64_t)1));
            clip.assetId = (uint64_t)std::max<int64_t>(0, c.value("assetId", (int64_t)0));
            clip.trackId = (uint64_t)std::max<int64_t>(0, c.value("trackId", (int64_t)0));
            clip.kind = (ClipKind)std::clamp(readInt(c, "kind", 0), 0, 2);
            clip.start = std::max(0.0, readNumber(c, "start", 0.0));
            clip.srcIn = std::max(0.0, readNumber(c, "srcIn", 0.0));
            clip.srcOut = std::max(clip.srcIn, readNumber(c, "srcOut", clip.srcIn));
            clip.volume = std::clamp(readNumber(c, "volume", 1.0), 0.0, 4.0);
            clip.opacity = std::clamp(readNumber(c, "opacity", 1.0), 0.0, 1.0);
            clip.fadeIn = std::max(0.0, readNumber(c, "fadeIn", 0.0));
            clip.fadeOut = std::max(0.0, readNumber(c, "fadeOut", 0.0));
            clip.brightness = std::clamp(readNumber(c, "brightness", 0.0), -1.0, 1.0);
            clip.contrast = std::clamp(readNumber(c, "contrast", 1.0), 0.0, 2.0);
            clip.saturation = std::clamp(readNumber(c, "saturation", 1.0), 0.0, 2.0);
            clip.scale = std::clamp(readNumber(c, "scale", 1.0), 0.05, 4.0);
            clip.posX = std::clamp(readNumber(c, "posX", 0.5), -1.0, 2.0);
            clip.posY = std::clamp(readNumber(c, "posY", 0.5), -1.0, 2.0);
            clip.text = readString(c, "text", "Seu texto");
            clip.fontSize = std::clamp(readInt(c, "fontSize", 72), 8, 400);
            clip.color = (uint32_t)std::max<int64_t>(0, c.value("color", (int64_t)0xFFFFFFFF));
            clip.align = std::clamp(readInt(c, "align", 1), 0, 2);
            clip.bold = readBool(c, "bold", true);
            clip.shadow = readBool(c, "shadow", true);

            if (!findTrack(p, clip.trackId)) continue;
            if (clip.kind != ClipKind::Title && !findAsset(p, clip.assetId)) continue;
            if (clip.duration() <= kEpsilon) continue;
            if (!trackAcceptsKind(*findTrack(p, clip.trackId), clip.kind)) continue;
            p.clips.push_back(clip);
            p.nextId = std::max(p.nextId, clip.id + 1);
        }
    }

    for (const auto& t : p.tracks) p.nextId = std::max(p.nextId, t.id + 1);
    for (const auto& a : p.assets) p.nextId = std::max(p.nextId, a.id + 1);
    for (const auto& c : p.clips) p.nextId = std::max(p.nextId, c.id + 1);

    sortClips(p);
    const double end = contentEnd(p);
    p.playhead = std::clamp(p.playhead, 0.0, end);
    if (p.inMark < 0.0 || p.inMark > end) p.inMark = -1.0;
    if (p.outMark <= p.inMark || p.outMark > end) p.outMark = -1.0;
    p.dirty = false;
    project = std::move(p);
    error.clear();
    return true;
}

bool saveProject(const Project& project, const std::string& path, std::string& error) {
    try {
        const std::string text = projectToJson(project);
        return writeTextFileAtomic(path, text, error);
    } catch (const std::exception& ex) {
        error = std::string("Falha ao salvar: ") + ex.what();
        return false;
    }
}

bool loadProject(Project& project, const std::string& path, std::string& error) {
    std::string text;
    if (!readTextFile(path, text, error)) return false;
    if (!projectFromJson(project, text, error)) return false;
    project.filePath = path;
    if (project.settings.name.empty() || project.settings.name == "Sem título") {
        project.settings.name = fileBaseName(path);
    }
    project.dirty = false;
    return true;
}

} // namespace efs
