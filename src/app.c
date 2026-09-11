#include "app.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#include "scrambler/zip_utils.h"
#include "system_font.h"
#include "ui.h"

#define TOAST_DURATION_SECONDS 3.4

typedef struct {
    UiHistoryItem history[UI_HISTORY_LIMIT];
    int history_count;
    char toast_message[UI_MESSAGE_CAPACITY];
    int toast_success;
    double toast_started_at;
    int toast_active;
} AppState;

static Font load_ui_font(void) {
    char font_path[1024];
    if (!system_font_find(font_path, sizeof(font_path))) return (Font){0};

    enum { FIRST_CODEPOINT = 32, LAST_CODEPOINT = 383 };
    int codepoints[LAST_CODEPOINT - FIRST_CODEPOINT + 1];
    for (int i = 0; i < (int)(sizeof(codepoints) / sizeof(codepoints[0])); i++) {
        codepoints[i] = FIRST_CODEPOINT + i;
    }

    Font font = LoadFontEx(font_path, 48, codepoints,
                           (int)(sizeof(codepoints) / sizeof(codepoints[0])));
    if (!IsFontValid(font)) return (Font){0};
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}

static const char *path_basename(const char *path) {
    const char *last_forward = strrchr(path, '/');
    const char *last_backward = strrchr(path, '\\');
    const char *last_separator = last_forward;

    if (last_backward && (!last_separator || last_backward > last_separator)) {
        last_separator = last_backward;
    }
    return last_separator ? last_separator + 1 : path;
}

static void show_result(AppState *state, const char *message, int success) {
    int last_index = state->history_count;
    if (last_index >= UI_HISTORY_LIMIT) last_index = UI_HISTORY_LIMIT - 1;

    for (int i = last_index; i > 0; i--) {
        state->history[i] = state->history[i - 1];
    }
    if (state->history_count < UI_HISTORY_LIMIT) state->history_count++;

    snprintf(state->history[0].message, sizeof(state->history[0].message),
             "%s", message);
    state->history[0].success = success;

    snprintf(state->toast_message, sizeof(state->toast_message), "%s",
             message);
    state->toast_success = success;
    state->toast_started_at = GetTime();
    state->toast_active = 1;
}

static void process_path(AppState *state, const char *input_path) {
    size_t input_length = strlen(input_path);
    if (input_length > SIZE_MAX - sizeof("_scrambled")) {
        show_result(state, "Dosya yolu desteklenenden daha uzun.", 0);
        return;
    }

    size_t output_path_size = input_length + sizeof("_scrambled");
    char *output_path = malloc(output_path_size);
    if (!output_path) {
        show_result(state, "Dosya yolu icin bellek ayrilamadi.", 0);
        return;
    }

    int was_zip = is_zip_file(input_path);
    if (process_dropped_file(input_path, output_path, output_path_size) != 0) {
        char message[UI_MESSAGE_CAPACITY];
        snprintf(message, sizeof(message), "%s islenemedi",
                 path_basename(input_path));
        show_result(state, message, 0);
        free(output_path);
        return;
    }

    char message[UI_MESSAGE_CAPACITY];
    snprintf(message, sizeof(message), "%s -> %s",
             path_basename(input_path), path_basename(output_path));
    show_result(state, message, 1);
    TraceLog(LOG_INFO, "%s: %s -> %s",
             was_zip ? "Scrambled" : "Restored",
             path_basename(input_path), path_basename(output_path));
    free(output_path);
}

static void process_startup_paths(AppState *state, int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        process_path(state, argv[i]);
    }
}

static void process_dropped_paths(AppState *state) {
    if (!IsFileDropped()) return;

    FilePathList paths = LoadDroppedFiles();
    for (unsigned int i = 0; i < paths.count; i++) {
        process_path(state, paths.paths[i]);
    }
    UnloadDroppedFiles(paths);
}

static float render_scale(void) {
    int screen_width = GetScreenWidth();
    if (screen_width <= 0) return 1.0f;

    float scale = (float)GetRenderWidth() / (float)screen_width;
    return scale > 0.0f ? scale : 1.0f;
}

int app_run(int argc, char *argv[]) {
    AppState state = {0};
    process_startup_paths(&state, argc, argv);
    if (argc > 1) return 0;

    Font font = load_ui_font();
    if (IsFontValid(font)) ui_set_font(font);

    while (!WindowShouldClose()) {
        process_dropped_paths(&state);

        UiLayout layout = ui_calculate_layout(GetScreenWidth(),
                                               GetScreenHeight());
        Vector2 mouse = GetMousePosition();
        int clear_hover = state.history_count > 0 &&
                          CheckCollisionPointRec(mouse, layout.clear_button);

        if (clear_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            state.history_count = 0;
        }
        SetMouseCursor(clear_hover ? MOUSE_CURSOR_POINTING_HAND
                                   : MOUSE_CURSOR_DEFAULT);

        double toast_age = GetTime() - state.toast_started_at;
        if (state.toast_active && toast_age >= TOAST_DURATION_SECONDS) {
            state.toast_active = 0;
        }

        float pulse = (sinf((float)GetTime() * 2.2f) + 1.0f) * 0.5f;

        BeginDrawing();
        Camera2D ui_camera = {0};
        ui_camera.zoom = render_scale();
        BeginMode2D(ui_camera);
        ui_draw(&layout, state.history, state.history_count,
                state.toast_message, state.toast_success, state.toast_active,
                toast_age, TOAST_DURATION_SECONDS, pulse, clear_hover);
        EndMode2D();
        EndDrawing();
    }

    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (IsFontValid(font)) {
        ui_set_font((Font){0});
        UnloadFont(font);
    }
    return 0;
}
