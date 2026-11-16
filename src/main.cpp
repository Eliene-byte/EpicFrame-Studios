#include <windows.h>

#include <SDL.h>

#include "app.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    efs::App app;
    if (!app.init()) return 1;
    app.run();
    app.shutdown();
    return 0;
}
