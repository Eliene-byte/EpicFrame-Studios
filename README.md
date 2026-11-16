# EpicFrame Studios

Editor de vídeo não linear (NLE) leve, em português do Brasil, pensado para **notebooks antigos e PCs fracos**. Interface no estilo dos editores profissionais, mas com consumo baixo de memória e CPU.

- **Portátil:** não tem instalador, não pede privilégios de administrador e não grava no registro do Windows.
- **Sem build no seu PC:** o código é compilado no GitHub Actions; você baixa apenas o ZIP pronto.
- **Gratuito e aberto:** licença GPL-3.0-or-later.

> **Status:** versão **0.1.0** em desenvolvimento ativo. A base do editor (projeto, mídia, linha do tempo, edição e layout) já está funcional. A prévia decodificada e a exportação MP4 estão no roadmap abaixo.

---

## O que já funciona

| Área | Recursos |
| --- | --- |
| Projeto | Novo, abrir, salvar e "salvar como" (`.efsproj`, JSON legível), alterações não salvas, autosave de layout |
| Mídia | Importar vídeo, áudio e imagem pelo diálogo nativo ou arrastando para a janela; busca na bin; remoção de mídia |
| Linha do tempo | Trilhas V1/V2/A1 (e novas trilhas), zoom, rolagem, ímã, régua, cursor, ferramenta de seleção e de corte |
| Edição | Inserir no cursor, mover clipes entre trilhas, aparar as duas bordas, dividir, excluir, duplicar, fechar lacunas |
| Títulos | Clipes de texto com conteúdo editável no inspetor |
| Efeitos | Fade de entrada/saída, opacidade, presets de cor (preto e branco, sépia, quente, frio, contraste) |
| Monitores | Origem e programa com informações de mídia e marcadores de posição |
| Interface | Dockspace redimensionável, tema escuro azul, fontes Segoe UI, layout restaurável, modo cinema (F11), escala por DPI |
| Desempenho | Renderização por software (SDL Renderer), sem dependência de GPU dedicada; prévia em qualidade Cheia/Metade/Um quarto |
| Diagnóstico | Autoteste de projeto, JSON, edição e leitura de mídia com FFmpeg |

## Ainda em desenvolvimento

- Decodificação real de vídeo nos monitores de origem/programa.
- Reprodução de áudio sincronizada com a linha do tempo.
- Exportação H.264/AAC via FFmpeg (presets rápidos para PCs fracos).
- Miniaturas de mídia e formas de onda de áudio.
- Autosave do projeto e proxies de baixa resolução.
- Ícone do aplicativo e manifesto visual.

O objetivo não é copiar 100% do Adobe Premiere. O foco é entregar o fluxo essencial de montagem — importar, cortar, organizar, aplicar títulos e efeitos simples, reproduzir e exportar — com uma interface familiar e leve.

---

## Baixar e usar

O aplicativo é gerado automaticamente pelo GitHub Actions a cada envio para `main`.

1. Abra a aba **Actions** do repositório e entre na execução mais recente do workflow **build**.
2. Baixe o artefato **EpicFrame-Studios-windows-x64**.
3. Extraia o ZIP em qualquer pasta (por exemplo, `C:\EpicFrame`).
4. Execute **`EpicFrameStudios.exe`**.

O pacote já inclui `ffmpeg.exe` e todas as DLLs necessárias. Não é preciso instalar Codecs, Visual Studio, MSYS2 ou FFmpeg.

Requisitos: **Windows 7 ou superior, 64 bits**. Em notebooks antigos, recomenda-se:

- Qualidade da prévia em **Metade** ou **Um quarto** (menu *Exibição*).
- Fechar navegadores e outros programas pesados durante a reprodução/exportação.
- Manter o projeto e a mídia no mesmo disco, de preferência um SSD.

> O executável não possui assinatura digital comercial. Na primeira execução, o Windows SmartScreen pode exibir um aviso: escolha **Mais informações → Executar assim mesmo**.

### Fluxo básico

1. **Arquivo → Importar mídia** (ou arraste os arquivos para a janela).
2. Arraste a mídia da bin para a linha do tempo, ou selecione e use **Inserir no cursor**.
3. Arraste as bordas dos clipes para aparar; use a ferramenta de corte (ou `Ctrl+K`) para dividir.
4. Selecione um clipe e ajuste opacidade, fades e cor no **Inspetor**.
5. Use **Arquivo → Salvar** para gravar o `.efsproj`.

### Atalhos

| Atalho | Ação |
| --- | --- |
| `Ctrl+N` | Novo projeto |
| `Ctrl+O` | Abrir projeto |
| `Ctrl+S` / `Ctrl+Shift+S` | Salvar / Salvar como |
| `Ctrl+I` | Importar mídia |
| `Ctrl+K` | Dividir no cursor |
| `Del` | Excluir seleção |
| `Ctrl+D` | Duplicar seleção |
| `Ctrl+A` | Selecionar tudo |
| `Espaço` | Reproduzir / pausar |
| `←` / `→` | Um quadro para trás / frente |
| `Home` / `End` | Início / fim do conteúdo |
| `Ctrl+0` | Redefinir layout |
| `F11` | Modo cinema |
| `Ctrl` + roda do mouse | Zoom na linha do tempo |

---

## Como o build funciona

Não é necessário compilar nada na sua máquina. Todo o processo roda no GitHub Actions, em um runner `windows-latest` com MSYS2 MINGW64.

```
push em main ──► MSYS2 instala GCC/CMake/SDL2/FFmpeg
             ──► CMake + Ninja compilam o EpicFrameStudios.exe
             ──► FFmpeg gera um clipe de teste
             ──► EpicFrameSelftest.exe valida projeto, JSON e probe de mídia
             ──► scripts/package.sh copia o EXE, o ffmpeg.exe e a closure de DLLs
             ──► ZIP portátil é publicado como artefato
```

Tags `v*` também publicam o ZIP em **Releases**.

### Para desenvolvedores

O build local é opcional. Se quiser reproduzir o ambiente:

```bash
# MSYS2 MINGW64
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-SDL2 mingw-w64-x86_64-ffmpeg zip
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/EpicFrameSelftest.exe
```

Stack:

- C++17, SDL2 e SDL_Renderer (renderização por software).
- [Dear ImGui](https://github.com/ocornut/imgui) `v1.91.8-docking` para a interface.
- [nlohmann/json](https://github.com/nlohmann/json) para o formato do projeto.
- FFmpeg (libavformat/libavcodec/libavutil/swscale/swresample) para leitura de mídia e exportação.
- CMake + Ninja; nenhuma dependência de GPU ou de runtime da Microsoft.

### Estrutura

```
src/
  main.cpp           entrada WinMain
  app.h/.cpp         ciclo de vida, projeto, edição e reprodução
  app_ui.cpp         menus, dockspace, monitores, modais e toasts
  timeline_ui.cpp    desenho e interação da linha do tempo
  inspector_ui.cpp   inspetor de clipes/mídia e painel de efeitos
  project.h/.cpp     modelo, JSON e operações de edição
  media.h/.cpp       probe de mídia com FFmpeg
  platform.h/.cpp    janelas, diálogos nativos, arquivos e ffmpeg.exe
  theme.h/.cpp       tema, fontes e escala de DPI
  selftest.*         autoteste executado no CI
.github/workflows/   build, autoteste, empacotamento e release
scripts/package.sh   monta a pasta portátil e o ZIP
```

O arquivo de layout da interface (`imgui.ini`) fica em `%APPDATA%\EpicFrame Studios`.

---

## Limitações conhecidas

- Somente **Windows x64**. Não há build para 32 bits, Linux ou macOS.
- A prévia atual exibe informações e marcadores; a decodificação de vídeo chega na próxima etapa.
- A exportação MP4 ainda não está disponível nesta versão.
- Não há aceleração por GPU; a renderização é feita por software para funcionar em qualquer PC.
- Projetos usam caminhos absolutos para a mídia. Se os arquivos forem movidos, será necessário reimportá-los.

## Licença

Distribuído sob a **GNU General Public License v3.0 ou posterior** (`GPL-3.0-or-later`). Veja [LICENSE](LICENSE).

O motivo é o FFmpeg empacotado: o build do MSYS2 inclui componentes GPL (como libx264). O código-fonte completo do EpicFrame Studios está neste repositório, e o FFmpeg correspondente pode ser obtido no repositório de pacotes do MSYS2.

Créditos e licenças de terceiros estão em [THIRD_PARTY.md](THIRD_PARTY.md).

Adobe Premiere é marca registrada da Adobe Inc. Este projeto não possui vínculo com a Adobe e usa apenas referências visuais genéricas de editores NLE.
