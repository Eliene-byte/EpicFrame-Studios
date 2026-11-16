Licenças de terceiros — EpicFrame Studios
==========================================

EpicFrame Studios é distribuído sob GPL-3.0-or-later (veja LICENSE).
Os componentes abaixo mantêm suas próprias licenças.

1. SDL2 (Simple DirectMedia Layer)
   Versão: 2.x (pacote mingw-w64-x86_64-SDL2 do MSYS2)
   Licença: zlib License
   https://www.libsdl.org/license.php
   Uso: janela, entrada, áudio e renderização por software.

2. Dear ImGui
   Versão: v1.91.8-docking
   Licença: MIT
   https://github.com/ocornut/imgui/blob/master/LICENSE.txt
   Uso: interface gráfica imediata e sistema de docks.

3. nlohmann/json
   Versão: 3.11.3
   Licença: MIT
   https://github.com/nlohmann/json/blob/develop/LICENSE.MIT
   Uso: leitura e gravação do formato de projeto .efsproj.

4. FFmpeg (libavformat, libavcodec, libavutil, libswscale, libswresample e ffmpeg.exe)
   Versão: 9.0.2 (pacote mingw-w64-x86_64-ffmpeg do MSYS2)
   Licença: LGPL-2.1-or-later, com componentes GPL habilitados
            (incluindo libx264), resultando em GPL-2.0-or-later.
   https://ffmpeg.org/legal.html
   https://packages.msys2.org/package/mingw-w64-x86_64-ffmpeg
   Uso: leitura de mídia, decodificação, conversão e exportação.

   O texto completo da licença GPL v3 que cobre o EpicFrame Studios
   também se aplica à distribuição conjunta. O código-fonte do FFmpeg
   usado no build e as instruções de compilação estão disponíveis no
   repositório de pacotes do MSYS2 e em https://ffmpeg.org/download.html.

5. MinGW-w64 GCC runtime (libgcc, libstdc++, libwinpthread)
   Licença: GPL com GCC Runtime Library Exception
   https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html
   Uso: runtime das DLLs geradas pelo compilador.

6. Componentes do Windows SDK / API do sistema
   Licença: termos da Microsoft (não redistribuídos como código-fonte).
   Uso: APIs Win32 (diálogos, arquivos, pastas e shell).

Dear ImGui, SDL2 e nlohmann/json são compatíveis com a GPL v3.
Nenhum componente proprietário da Adobe é usado ou redistribuído.
Adobe Premiere é marca registrada da Adobe Inc.; este projeto não tem
vínculo com a Adobe.
