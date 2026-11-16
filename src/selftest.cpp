#include "selftest.h"

#include "media.h"
#include "platform.h"
#include "project.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace efs {

namespace {

int gChecks = 0;
int gFailures = 0;

void check(bool condition, const char* description) {
    ++gChecks;
    if (condition) {
        std::printf("[OK]    %s\n", description);
    } else {
        std::printf("[FALHA] %s\n", description);
        ++gFailures;
    }
}

} // namespace

int runSelfTest(int argc, char** argv) {
    std::string mediaPath;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--media") == 0 && i + 1 < argc) {
            mediaPath = argv[++i];
        }
    }

    std::printf("EpicFrame Studios - autoteste %s\n", appVersion().c_str());
    std::printf("------------------------------------------------------------\n");

    check(!appVersion().empty(), "versão do aplicativo disponível");
    check(!appDir().empty(), "diretório do executável detectado");
    check(!tempDir().empty(), "diretório temporário disponível");

    Project project;
    initDefaultProject(project);
    check(project.tracks.size() == 3, "projeto padrão cria V1, V2 e A1");
    check(project.tracks[0].name == "V1" && project.tracks[2].name == "A1", "nomes das faixas corretos");

    uint64_t videoTrack = 0;
    uint64_t audioTrack = 0;
    for (const auto& track : project.tracks) {
        if (!videoTrack && track.video) videoTrack = track.id;
        if (!audioTrack && !track.video) audioTrack = track.id;
    }
    check(videoTrack != 0 && audioTrack != 0, "faixas de vídeo e áudio identificadas");

    Asset asset;
    asset.id = project.newId();
    asset.path = joinPath(tempDir(), "midia_de_teste.mp4");
    asset.name = "Mídia de teste";
    asset.info.duration = 12.0;
    asset.info.width = 1280;
    asset.info.height = 720;
    asset.info.fps = 30.0;
    asset.info.hasVideo = true;
    asset.info.hasAudio = true;
    asset.info.sampleRate = 48000;
    asset.info.channels = 2;
    project.assets.push_back(asset);

    std::string error;
    check(addMediaToTrack(project, asset.id, videoTrack, 0.0, 0.0, 12.0, false, error),
          "inserir mídia na linha do tempo");
    check(project.clips.size() == 1, "um clipe criado");
    check(project.assets.size() == 1, "uma mídia no projeto");

    const uint64_t firstClip = project.clips.front().id;
    check(splitClipAt(project, firstClip, 4.0), "dividir clipe no cursor");
    check(project.clips.size() == 2, "divisão produz dois clipes");
    double totalDuration = 0.0;
    for (const auto& clip : project.clips) totalDuration += clip.duration();
    check(std::abs(totalDuration - 12.0) < 1e-6, "duração preservada após dividir");

    check(addTitleToTrack(project, videoTrack, 20.0, 5.0, error), "adicionar título de texto");
    bool foundTitle = false;
    for (const auto& clip : project.clips) {
        if (clip.isTitle() && clip.trackId == videoTrack) foundTitle = true;
    }
    check(foundTitle, "título gravado na faixa de vídeo");

    const std::string json = projectToJson(project);
    check(json.find("epicframe-project") != std::string::npos, "JSON identifica o formato do projeto");

    Project roundtrip;
    check(projectFromJson(roundtrip, json, error), "ler projeto a partir do JSON");
    check(roundtrip.tracks.size() == project.tracks.size(), "faixas preservadas no JSON");
    check(roundtrip.assets.size() == project.assets.size(), "mídias preservadas no JSON");
    check(roundtrip.clips.size() == project.clips.size(), "clipes preservados no JSON");

    const std::string projectPath = joinPath(tempDir(), "epicframe_autoteste.efsproj");
    check(saveProject(project, projectPath, error), "salvar projeto em disco");
    Project loaded;
    check(loadProject(loaded, projectPath, error), "abrir projeto do disco");
    check(!loaded.dirty, "projeto carregado não fica marcado como alterado");
    check(loaded.clips.size() == project.clips.size(), "clipes sobrevivem ao salvar/abrir");

    uint64_t moveId = 0;
    for (const auto& clip : project.clips) {
        if (!clip.isTitle()) {
            moveId = clip.id;
            break;
        }
    }
    check(moveId != 0, "clipe de mídia localizado para mover");
    check(moveClipTo(project, moveId, videoTrack, 30.0), "mover clipe para outro ponto da faixa");
    Clip* moved = findClip(project, moveId);
    check(moved && std::abs(moved->start - 30.0) < 1e-6, "posição do clipe atualizada");
    check(trimClipEdge(project, moveId, false, 32.0), "apara a borda final do clipe");
    moved = findClip(project, moveId);
    check(moved && std::abs(moved->end() - 32.0) < 1e-6, "borda final aparada");
    duplicateClips(project, {moveId});
    check(project.clips.size() >= 4, "clipe duplicado é adicionado");

    double timecode = 0.0;
    check(parseTimecode("00:00:10:15", 30.0, timecode) && std::abs(timecode - 10.5) < 1e-6,
          "conversão de timecode com quadros");

    std::vector<uint64_t> allIds;
    for (const auto& clip : project.clips) allIds.push_back(clip.id);
    check(removeClips(project, allIds, false), "remover todos os clipes");
    check(project.clips.empty(), "linha do tempo fica vazia após remoção");

    if (!mediaPath.empty()) {
        initFFmpeg();
        const ProbeResult probe = probeMedia(mediaPath);
        check(probe.ok(), "FFmpeg abre a mídia de teste");
        if (probe.ok()) {
            check(probe.info.hasVideo, "mídia de teste contém vídeo");
            check(probe.info.hasAudio, "mídia de teste contém áudio");
            check(probe.info.duration > 0.5, "duração da mídia de teste detectada");
            check(probe.info.width > 0 && probe.info.height > 0, "resolução da mídia de teste detectada");
            std::printf("        mídia: %dx%d, %.2f fps, %.2f s, vídeo=%s, áudio=%s\n", probe.info.width,
                        probe.info.height, probe.info.fps, probe.info.duration,
                        probe.info.videoCodec.c_str(), probe.info.audioCodec.c_str());
        } else {
            std::printf("        erro do FFmpeg: %s\n", probe.error.c_str());
        }
    } else {
        std::printf("[INFO]  nenhuma mídia passada via --media; teste de decodificação ignorado\n");
    }

    std::printf("------------------------------------------------------------\n");
    std::printf("%d verificação(ões), %d falha(s).\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}

} // namespace efs
