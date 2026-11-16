#pragma once

#include "types.h"

#include <string>

namespace efs {

struct ProbeResult {
    MediaInfo info;
    std::string error;
    bool ok() const { return error.empty(); }
};

void initFFmpeg();
std::string ffmpegErrorString(int code);
ProbeResult probeMedia(const std::string& path);

} // namespace efs
