#include "system_font.h"

#include <stdio.h>
#include <string.h>

static int file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    fclose(file);
    return 1;
}

int system_font_find(char *output_path, size_t output_path_size) {
    if (!output_path || output_path_size == 0) return 0;
    output_path[0] = '\0';

    static const char *candidates[] = {
#if defined(_WIN32)
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\calibri.ttf",
#elif defined(__APPLE__)
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
        NULL
    };

    for (int i = 0; candidates[i]; i++) {
        if (!file_exists(candidates[i])) continue;

        size_t path_length = strlen(candidates[i]);
        if (path_length >= output_path_size) continue;
        memcpy(output_path, candidates[i], path_length + 1);
        return 1;
    }
    return 0;
}
