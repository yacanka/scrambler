#include <stdio.h>

#include "raylib.h"

#include "app.h"

#define INITIAL_WINDOW_WIDTH 920
#define INITIAL_WINDOW_HEIGHT 680
#define MIN_WINDOW_WIDTH 640
#define MIN_WINDOW_HEIGHT 540

int main(int argc, char *argv[]) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI |
                   FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT,
               "Scrambler - ZIP Donusturucu");

    if (!IsWindowReady()) {
        fprintf(stderr, "Pencere olusturulamadi.\n");
        return 1;
    }

    SetWindowMinSize(MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
    SetTargetFPS(60);

    int result = app_run(argc, argv);
    CloseWindow();
    return result;
}
