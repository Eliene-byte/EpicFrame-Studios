#include "media.h"

#include "platform.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>

namespace efs {

void initFFmpeg() {
    static std::once_flag once;
    std::call_once(once, []() {
        av_log_set_level(AV_LOG_ERROR);
        avformat_network_init();
    });
}

std::string ffmpegErrorString(int code) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    if (av_strerror(code, buffer, sizeof(buffer)) < 0) {
        return "erro " + std::to_string(code);
    }
    return std::string(buffer);
}

static bool isImageExtension(const std::string& path) {
    static const char* extensions[] = {"png", "jpg", "jpeg", "bmp", "webp", "avif", "tif", "tiff"};
    const std::string ext = fileExtensionLower(path);
    for (const char* candidate : extensions) {
        if (ext == candidate) return true;
    }
    return false;
}

ProbeResult probeMedia(const std::string& path) {
    initFFmpeg();
    ProbeResult result;

    AVFormatContext* format = nullptr;
    int ret = avformat_open_input(&format, path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        result.error = "Não foi possível abrir a mídia: " + ffmpegErrorString(ret);
        return result;
    }
    ret = avformat_find_stream_info(format, nullptr);
    if (ret < 0) {
        result.error = "Não foi possível ler as informações da mídia: " + ffmpegErrorString(ret);
        avformat_close_input(&format);
        return result;
    }

    const int videoIndex = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    const int audioIndex = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    MediaInfo info;
    if (videoIndex >= 0) {
        AVStream* stream = format->streams[videoIndex];
        AVCodecParameters* codec = stream->codecpar;
        info.hasVideo = true;
        info.width = std::max(0, codec->width);
        info.height = std::max(0, codec->height);
        const char* name = avcodec_get_name(codec->codec_id);
        info.videoCodec = name ? name : "";

        AVRational rate = av_guess_frame_rate(format, stream, nullptr);
        if (rate.num <= 0 || rate.den <= 0) rate = stream->avg_frame_rate;
        if (rate.num > 0 && rate.den > 0) {
            info.fps = std::clamp(av_q2d(rate), 1.0, 240.0);
        }
    }
    if (audioIndex >= 0) {
        AVStream* stream = format->streams[audioIndex];
        AVCodecParameters* codec = stream->codecpar;
        info.hasAudio = true;
        info.sampleRate = std::max(0, codec->sample_rate);
        info.channels = std::max(0, codec->ch_layout.nb_channels);
        const char* name = avcodec_get_name(codec->codec_id);
        info.audioCodec = name ? name : "";
    }

    if (!info.hasVideo && !info.hasAudio) {
        result.error = "O arquivo não contém faixas de vídeo ou áudio.";
        avformat_close_input(&format);
        return result;
    }

    double duration = 0.0;
    if (format->duration != AV_NOPTS_VALUE && format->duration > 0) {
        duration = (double)format->duration / (double)AV_TIME_BASE;
    } else {
        for (unsigned i = 0; i < format->nb_streams; ++i) {
            AVStream* stream = format->streams[i];
            if (stream->duration != AV_NOPTS_VALUE && stream->duration > 0) {
                duration = std::max(duration, (double)stream->duration * av_q2d(stream->time_base));
            }
        }
    }

    const std::string formatName = format->iformat && format->iformat->name ? format->iformat->name : "";
    const bool imageFormat = formatName.find("image2") != std::string::npos ||
                             formatName.find("_pipe") != std::string::npos ||
                             formatName.find("image") != std::string::npos;
    const bool imageExtension = isImageExtension(path);
    if (info.hasVideo && !info.hasAudio && imageExtension && (duration <= 0.5 || imageFormat)) {
        info.isImage = true;
        info.duration = 10.0;
        info.fps = 30.0;
    } else {
        if (!std::isfinite(duration) || duration < 0.0) duration = 0.0;
        info.duration = duration;
    }

    if (info.hasVideo && info.width <= 0) {
        result.error = "Não foi possível identificar a resolução do vídeo.";
        avformat_close_input(&format);
        return result;
    }

    avformat_close_input(&format);
    result.info = info;
    return result;
}

} // namespace efs
