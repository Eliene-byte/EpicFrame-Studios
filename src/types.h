#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace efs {

constexpr double kEpsilon = 1e-6;

struct SequenceSettings {
    std::string name = "Sem título";
    int width = 1920;
    int height = 1080;
    double fps = 30.0;
};

struct MediaInfo {
    double duration = 0.0;
    int width = 0;
    int height = 0;
    double fps = 30.0;
    bool hasVideo = false;
    bool hasAudio = false;
    bool isImage = false;
    int sampleRate = 0;
    int channels = 0;
    std::string videoCodec;
    std::string audioCodec;
};

struct Asset {
    uint64_t id = 0;
    std::string path;
    std::string name;
    MediaInfo info;
    bool missing = false;
    bool peaksReady = false;
    std::vector<float> peaks; // 0..1, one value per time bucket
    bool thumbnailReady = false;
    int thumbWidth = 0;
    int thumbHeight = 0;
    void* thumbTexture = nullptr; // SDL_Texture*, owned by the app
    std::string error;
};

enum class ClipKind : int {
    Video = 0,
    Audio = 1,
    Title = 2
};

struct Clip {
    uint64_t id = 0;
    uint64_t assetId = 0;
    uint64_t trackId = 0;
    ClipKind kind = ClipKind::Video;
    double start = 0.0;
    double srcIn = 0.0;
    double srcOut = 0.0;
    double volume = 1.0;
    double opacity = 1.0;
    double fadeIn = 0.0;
    double fadeOut = 0.0;
    double brightness = 0.0; // -1..1
    double contrast = 1.0;   // 0..2
    double saturation = 1.0; // 0..2
    double scale = 1.0;
    double posX = 0.5;
    double posY = 0.5;
    std::string text;
    int fontSize = 72;
    uint32_t color = 0xFFFFFFFFu; // RGBA
    int align = 1;                // 0 left, 1 center, 2 right
    bool bold = true;
    bool shadow = true;

    double duration() const { return std::max(0.0, srcOut - srcIn); }
    double end() const { return start + duration(); }
    bool contains(double t) const { return t >= start - kEpsilon && t < end() - kEpsilon; }
    bool isTitle() const { return kind == ClipKind::Title; }
    bool isAudioOnly() const { return kind == ClipKind::Audio; }
};

struct Track {
    uint64_t id = 0;
    std::string name;
    bool video = true;
    bool muted = false;
    bool locked = false;
    bool hidden = false;
    int height = 58;
};

struct Project {
    SequenceSettings settings;
    std::string filePath;
    std::vector<Track> tracks;
    std::vector<Clip> clips;
    std::vector<Asset> assets;
    uint64_t nextId = 1;
    bool dirty = false;
    double playhead = 0.0;
    double inMark = -1.0;
    double outMark = -1.0;

    uint64_t newId() { return nextId++; }
};

inline double snapFrame(double t, double fps) {
    if (fps <= 0.0) return t;
    return std::round(t * fps) / fps;
}

inline double floorFrame(double t, double fps) {
    if (fps <= 0.0) return t;
    return std::floor(t * fps + 1e-9) / fps;
}

inline double ceilFrame(double t, double fps) {
    if (fps <= 0.0) return t;
    return std::ceil(t * fps - 1e-9) / fps;
}

inline std::string formatTimecode(double seconds, double fps, bool withFrames = true) {
    if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
    const int fpsi = std::max(1, (int)std::lround(fps));
    long long totalFrames = (long long)std::llround(seconds * (double)fpsi);
    long long frames = totalFrames % fpsi;
    long long secs = totalFrames / fpsi;
    char buf[40];
    if (withFrames) {
        std::snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld:%02lld",
                      secs / 3600, (secs / 60) % 60, secs % 60, frames);
    } else {
        std::snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld",
                      secs / 3600, (secs / 60) % 60, secs % 60);
    }
    return buf;
}

inline std::string formatDuration(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
    long long total = (long long)std::llround(seconds);
    char buf[32];
    if (total >= 3600) {
        std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", total / 3600, (total / 60) % 60, total % 60);
    } else {
        std::snprintf(buf, sizeof(buf), "%lld:%02lld", total / 60, total % 60);
    }
    return buf;
}

inline bool parseTimecode(const std::string& text, double fps, double& outSeconds) {
    std::string s;
    s.reserve(text.size());
    for (char c : text) {
        if (c == ';' || c == '.') {
            s.push_back(':');
        } else if (c != ' ' && c != '\t') {
            s.push_back(c);
        }
    }
    if (s.empty()) return false;

    std::vector<std::string> parts;
    size_t begin = 0;
    while (true) {
        size_t pos = s.find(':', begin);
        if (pos == std::string::npos) {
            parts.push_back(s.substr(begin));
            break;
        }
        parts.push_back(s.substr(begin, pos - begin));
        begin = pos + 1;
    }
    if (parts.size() > 4) return false;
    for (const auto& p : parts) {
        if (p.empty()) return false;
        for (char c : p) {
            if (!std::isdigit((unsigned char)c) && c != '-') return false;
        }
    }

    const int fpsi = std::max(1, (int)std::lround(fps));
    double value = 0.0;
    if (parts.size() == 1) {
        value = std::atof(parts[0].c_str());
    } else if (parts.size() == 2) {
        value = std::atof(parts[0].c_str()) * 60.0 + std::atof(parts[1].c_str());
    } else if (parts.size() == 3) {
        value = std::atof(parts[0].c_str()) * 3600.0 + std::atof(parts[1].c_str()) * 60.0 +
                std::atof(parts[2].c_str());
    } else {
        int frames = std::atoi(parts[3].c_str());
        if (frames < 0 || frames >= fpsi) return false;
        value = std::atof(parts[0].c_str()) * 3600.0 + std::atof(parts[1].c_str()) * 60.0 +
                std::atof(parts[2].c_str()) + (double)frames / (double)fpsi;
    }
    if (!std::isfinite(value) || value < 0.0) return false;
    outSeconds = value;
    return true;
}

inline uint32_t rgbaColor(int r, int g, int b, int a = 255) {
    return ((uint32_t)(r & 255) << 24) | ((uint32_t)(g & 255) << 16) | ((uint32_t)(b & 255) << 8) |
           (uint32_t)(a & 255);
}

} // namespace efs
