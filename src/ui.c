#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "raymath.h"

static const Color COLOR_BACKGROUND_TOP = {25, 27, 38, 255};
static const Color COLOR_BACKGROUND_BOTTOM = {12, 14, 21, 255};
static const Color COLOR_CARD = {37, 40, 54, 255};
static const Color COLOR_TEXT = {240, 241, 247, 255};
static const Color COLOR_MUTED = {151, 157, 177, 255};
static const Color COLOR_ACCENT = {112, 132, 255, 255};
static const Color COLOR_ACCENT_LIGHT = {155, 169, 255, 255};
static const Color COLOR_SUCCESS = {65, 192, 118, 255};
static const Color COLOR_ERROR = {224, 91, 96, 255};
static const Color COLOR_BUTTON = {58, 63, 83, 255};
static const Color COLOR_BUTTON_HOVER = {75, 82, 109, 255};
static Font ui_font = {0};

void ui_set_font(Font font) {
    ui_font = font;
}

static Font current_font(void) {
    return ui_font.texture.id != 0 ? ui_font : GetFontDefault();
}

static Color mix_color(Color from, Color to, float amount) {
    Color result = {
        (unsigned char)(from.r + (to.r - from.r) * amount),
        (unsigned char)(from.g + (to.g - from.g) * amount),
        (unsigned char)(from.b + (to.b - from.b) * amount),
        255
    };
    return result;
}

static void draw_text(const char *text, float x, float y, float size,
                      Color color) {
    DrawTextEx(current_font(), text, (Vector2){x, y}, size, size / 20.0f,
               color);
}

static float text_width(const char *text, float size) {
    return MeasureTextEx(current_font(), text, size, size / 20.0f).x;
}

static void draw_centered_text(const char *text, Rectangle area, float y,
                               float size, Color color) {
    float x = area.x + (area.width - text_width(text, size)) * 0.5f;
    draw_text(text, x, y, size, color);
}

static void fit_text(const char *text, float max_width, float font_size,
                     char *output, size_t output_size) {
    if (!output || output_size == 0) return;
    snprintf(output, output_size, "%s", text ? text : "");
    if (text_width(output, font_size) <= max_width) return;

    size_t length = strlen(output);
    while (length > 0) {
        length--;
        while (length > 0 &&
               ((unsigned char)output[length] & 0xC0U) == 0x80U) {
            length--;
        }
        output[length] = '\0';

        if (length + 4 > output_size) continue;
        memcpy(output + length, "...", 4);
        if (text_width(output, font_size) <= max_width) return;
        output[length] = '\0';
    }
    snprintf(output, output_size, "...");
}

static void draw_dashed_line(Vector2 start, Vector2 end, float dash,
                             float gap, float thickness, Color color) {
    Vector2 delta = {end.x - start.x, end.y - start.y};
    float length = Vector2Length(delta);
    if (length <= 0.0f) return;

    Vector2 direction = Vector2Scale(delta, 1.0f / length);
    for (float offset = 0.0f; offset < length; offset += dash + gap) {
        float segment_end = offset + dash;
        if (segment_end > length) segment_end = length;
        Vector2 from = Vector2Add(start, Vector2Scale(direction, offset));
        Vector2 to = Vector2Add(start, Vector2Scale(direction, segment_end));
        DrawLineEx(from, to, thickness, color);
    }
}

static void draw_dashed_border(Rectangle area, Color color) {
    float inset = 13.0f;
    float left = area.x + inset;
    float right = area.x + area.width - inset;
    float top = area.y + inset;
    float bottom = area.y + area.height - inset;

    draw_dashed_line((Vector2){left, top}, (Vector2){right, top},
                     10.0f, 7.0f, 2.0f, color);
    draw_dashed_line((Vector2){right, top}, (Vector2){right, bottom},
                     10.0f, 7.0f, 2.0f, color);
    draw_dashed_line((Vector2){right, bottom}, (Vector2){left, bottom},
                     10.0f, 7.0f, 2.0f, color);
    draw_dashed_line((Vector2){left, bottom}, (Vector2){left, top},
                     10.0f, 7.0f, 2.0f, color);
}

static void draw_zip_icon(float center_x, float center_y, Color accent) {
    Rectangle document = {center_x - 31.0f, center_y - 38.0f, 62.0f, 76.0f};
    DrawRectangleRounded(document, 0.16f, 8, COLOR_TEXT);

    DrawTriangle((Vector2){document.x + 42.0f, document.y},
                 (Vector2){document.x + 62.0f, document.y + 20.0f},
                 (Vector2){document.x + 42.0f, document.y + 20.0f},
                 (Color){205, 209, 225, 255});

    float zipper_y = document.y + 8.0f;
    for (int i = 0; i < 7; i++) {
        float x = center_x + ((i % 2 == 0) ? -4.0f : 4.0f);
        DrawRectangleRounded((Rectangle){x - 3.0f, zipper_y + i * 7.0f,
                                         7.0f, 6.0f},
                             0.3f, 4, accent);
    }
    DrawRectangleRounded((Rectangle){center_x - 7.0f, document.y + 57.0f,
                                     14.0f, 11.0f},
                         0.3f, 4, accent);
}

static void draw_status_icon(Vector2 center, int success, float radius) {
    Color color = success ? COLOR_SUCCESS : COLOR_ERROR;
    DrawCircleV(center, radius, color);

    if (success) {
        DrawLineEx((Vector2){center.x - 5.0f, center.y},
                   (Vector2){center.x - 1.0f, center.y + 4.0f}, 2.0f,
                   RAYWHITE);
        DrawLineEx((Vector2){center.x - 1.0f, center.y + 4.0f},
                   (Vector2){center.x + 6.0f, center.y - 5.0f}, 2.0f,
                   RAYWHITE);
    } else {
        DrawLineEx((Vector2){center.x - 4.0f, center.y - 4.0f},
                   (Vector2){center.x + 4.0f, center.y + 4.0f}, 2.0f,
                   RAYWHITE);
        DrawLineEx((Vector2){center.x + 4.0f, center.y - 4.0f},
                   (Vector2){center.x - 4.0f, center.y + 4.0f}, 2.0f,
                   RAYWHITE);
    }
}

UiLayout ui_calculate_layout(int width, int height) {
    float padding = width < 760 ? 20.0f : 30.0f;
    float content_width = width - padding * 2.0f;
    float drop_height = height * 0.43f;
    if (drop_height < 230.0f) drop_height = 230.0f;
    if (drop_height > 300.0f) drop_height = 300.0f;

    UiLayout layout = {0};
    layout.header = (Rectangle){padding, 26.0f, content_width, 70.0f};
    layout.drop_zone = (Rectangle){padding, 105.0f, content_width,
                                   drop_height};
    layout.history = (Rectangle){padding,
                                 layout.drop_zone.y + drop_height + 16.0f,
                                 content_width,
                                 height - layout.drop_zone.y - drop_height -
                                     16.0f - padding};
    layout.clear_button = (Rectangle){layout.history.x +
                                          layout.history.width - 112.0f,
                                      layout.history.y + 14.0f,
                                      94.0f, 32.0f};
    return layout;
}

static void draw_header(const UiLayout *layout) {
    draw_text("Scrambler", layout->header.x, layout->header.y, 31.0f,
              COLOR_TEXT);
    draw_text("ZIP dosyalarını bozar ve geri getirir",
              layout->header.x, layout->header.y + 40.0f, 16.0f,
              COLOR_MUTED);

    const char *badge = "Yerel  |  Hızlı  |  Orijinal korunur";
    float badge_width = text_width(badge, 13.0f) + 24.0f;
    Rectangle badge_area = {layout->header.x + layout->header.width -
                                badge_width,
                            layout->header.y + 5.0f, badge_width, 30.0f};
    DrawRectangleRounded(badge_area, 0.5f, 8,
                         (Color){43, 48, 69, 255});
    draw_text(badge, badge_area.x + 12.0f, badge_area.y + 8.0f, 13.0f,
              COLOR_ACCENT_LIGHT);
}

static void draw_drop_zone(const UiLayout *layout, float pulse) {
    Rectangle shadow = layout->drop_zone;
    shadow.y += 5.0f;
    DrawRectangleRounded(shadow, 0.07f, 12, Fade(BLACK, 0.25f));
    DrawRectangleRounded(layout->drop_zone, 0.07f, 12, COLOR_CARD);

    Color border = mix_color(COLOR_ACCENT, COLOR_ACCENT_LIGHT, pulse);
    draw_dashed_border(layout->drop_zone, border);

    float center_x = layout->drop_zone.x + layout->drop_zone.width * 0.5f;
    float icon_y = layout->drop_zone.y + layout->drop_zone.height * 0.40f;
    draw_zip_icon(center_x, icon_y, border);

    draw_centered_text("Dosyaları buraya sürükleyin", layout->drop_zone,
                       icon_y + 54.0f, 21.0f, COLOR_TEXT);
    draw_centered_text("Birden fazla dosyayı aynı anda bırakabilirsiniz",
                       layout->drop_zone, icon_y + 84.0f, 14.0f,
                       COLOR_MUTED);

    const char *mode = "ZIP  ->  SCRAMBLE       SCRAMBLED  ->  RESTORE";
    draw_centered_text(mode, layout->drop_zone,
                       layout->drop_zone.y + layout->drop_zone.height - 38.0f,
                       12.0f, COLOR_ACCENT_LIGHT);
}

static void draw_history(const UiLayout *layout,
                         const UiHistoryItem *history, int history_count,
                         int clear_hover) {
    DrawRectangleRounded(layout->history, 0.07f, 12, COLOR_CARD);
    draw_text("Son işlemler", layout->history.x + 18.0f,
              layout->history.y + 18.0f, 18.0f, COLOR_TEXT);

    Color button_color = clear_hover ? COLOR_BUTTON_HOVER : COLOR_BUTTON;
    if (history_count == 0) button_color = Fade(COLOR_BUTTON, 0.45f);
    DrawRectangleRounded(layout->clear_button, 0.35f, 8, button_color);
    draw_centered_text("Temizle", layout->clear_button,
                       layout->clear_button.y + 9.0f, 13.0f,
                       history_count > 0 ? COLOR_TEXT : COLOR_MUTED);

    float list_y = layout->history.y + 58.0f;
    if (history_count == 0) {
        draw_text("Henüz bir dosya işlenmedi.", layout->history.x + 18.0f,
                  list_y + 6.0f, 14.0f, COLOR_MUTED);
        return;
    }

    float row_height = 31.0f;
    for (int i = 0; i < history_count; i++) {
        float row_y = list_y + i * row_height;
        if (row_y + row_height > layout->history.y + layout->history.height) {
            break;
        }

        Vector2 icon_center = {layout->history.x + 29.0f, row_y + 12.0f};
        draw_status_icon(icon_center, history[i].success, 9.0f);

        char fitted[UI_MESSAGE_CAPACITY];
        fit_text(history[i].message, layout->history.width - 72.0f, 14.0f,
                 fitted, sizeof(fitted));
        draw_text(fitted, layout->history.x + 48.0f, row_y + 4.0f, 14.0f,
                  history[i].success ? COLOR_TEXT
                                     : (Color){255, 199, 201, 255});
    }
}

static void draw_toast(const UiLayout *layout, const char *message,
                       int success, double age, double duration) {
    float alpha = 1.0f;
    if (age < 0.18) alpha = (float)(age / 0.18);
    if (age > duration - 0.65) {
        alpha = (float)((duration - age) / 0.65);
    }
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    Rectangle toast = {layout->drop_zone.x + 25.0f,
                       layout->drop_zone.y + 22.0f,
                       layout->drop_zone.width - 50.0f, 46.0f};
    Color base = success ? COLOR_SUCCESS : COLOR_ERROR;
    DrawRectangleRounded(toast, 0.24f, 8, Fade(base, 0.92f * alpha));
    draw_status_icon((Vector2){toast.x + 24.0f, toast.y + 23.0f}, success,
                     10.0f);

    char fitted[UI_MESSAGE_CAPACITY];
    fit_text(message, toast.width - 58.0f, 14.0f, fitted, sizeof(fitted));
    draw_text(fitted, toast.x + 43.0f, toast.y + 15.0f, 14.0f,
              Fade(RAYWHITE, alpha));
}

void ui_draw(const UiLayout *layout, const UiHistoryItem *history,
             int history_count, const char *toast_message,
             int toast_success, int toast_active, double toast_age,
             double toast_duration, float pulse, int clear_hover) {
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                           COLOR_BACKGROUND_TOP, COLOR_BACKGROUND_BOTTOM);
    draw_header(layout);
    draw_drop_zone(layout, pulse);
    draw_history(layout, history, history_count, clear_hover);

    if (toast_active) {
        draw_toast(layout, toast_message, toast_success, toast_age,
                   toast_duration);
    }
}
