#pragma once

#include "types.h"

#include <string>
#include <vector>

namespace efs {

Track* findTrack(Project& project, uint64_t id);
const Track* findTrack(const Project& project, uint64_t id);
Asset* findAsset(Project& project, uint64_t id);
const Asset* findAsset(const Project& project, uint64_t id);
Clip* findClip(Project& project, uint64_t id);
const Clip* findClip(const Project& project, uint64_t id);
bool isVideoTrack(const Project& project, uint64_t trackId);
double contentEnd(const Project& project);
std::vector<const Clip*> clipsOnTrack(const Project& project, uint64_t trackId);
const Clip* clipAt(const Project& project, uint64_t trackId, double t);
double trackEnd(const Project& project, uint64_t trackId, uint64_t ignoreClip = 0);

void initDefaultProject(Project& project);
uint64_t addTrack(Project& project, bool video);
std::string defaultTrackName(const Project& project, bool video);
bool trackAcceptsKind(const Track& track, ClipKind kind);
bool assetFitsTrack(const Asset& asset, const Track& track);

std::string uniqueAssetName(const Project& project, const std::string& name);
std::string projectToJson(const Project& project);
bool projectFromJson(Project& project, const std::string& json, std::string& error);
bool saveProject(const Project& project, const std::string& path, std::string& error);
bool loadProject(Project& project, const std::string& path, std::string& error);

Clip makeMediaClip(const Project& project, uint64_t assetId, uint64_t trackId, double start,
                   double srcIn, double srcOut);
Clip makeTitleClip(const Project& project, uint64_t trackId, double start, double duration);
bool clipOverlaps(const Project& project, const Clip& clip, uint64_t ignoreClipId = 0);
bool addMediaToTrack(Project& project, uint64_t assetId, uint64_t trackId, double start,
                     double srcIn, double srcOut, bool ripple, std::string& error);
bool addTitleToTrack(Project& project, uint64_t trackId, double start, double duration,
                     std::string& error);
bool splitClipAt(Project& project, uint64_t clipId, double timelineTime);
int splitClipsAt(Project& project, double timelineTime, const std::vector<uint64_t>& onlyThese);
bool removeClips(Project& project, const std::vector<uint64_t>& clipIds, bool ripple);
bool moveClipTo(Project& project, uint64_t clipId, uint64_t trackId, double newStart);
bool trimClipEdge(Project& project, uint64_t clipId, bool leftEdge, double newTimelineTime);
std::pair<double, double> allowedMoveRange(const Project& project, const Clip& clip, uint64_t trackId);
std::pair<double, double> allowedTrimRange(const Project& project, const Clip& clip, bool leftEdge);
void closeGaps(Project& project, uint64_t trackId);
void duplicateClips(Project& project, const std::vector<uint64_t>& clipIds);
void sortClips(Project& project);

} // namespace efs
