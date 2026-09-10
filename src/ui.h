#ifndef SCRAMBLER_UI_H
#define SCRAMBLER_UI_H

#include "raylib.h"

#define UI_HISTORY_LIMIT 5
#define UI_MESSAGE_CAPACITY 512

typedef struct {
    char message[UI_MESSAGE_CAPACITY];
    int success;
} UiHistoryItem;

typedef struct {
    Rectangle header;
    Rectangle drop_zone;
    Rectangle history;
    Rectangle clear_button;
} UiLayout;

UiLayout ui_calculate_layout(int width, int height);

void ui_set_font(Font font);

void ui_draw(const UiLayout *layout, const UiHistoryItem *history,
             int history_count, const char *toast_message,
             int toast_success, int toast_active, double toast_age,
             double toast_duration, float pulse, int clear_hover);

#endif /* SCRAMBLER_UI_H */
