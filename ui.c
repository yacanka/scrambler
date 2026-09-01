#include "ui.h"

#include <math.h>
#include <string.h>

static void fill_circle(SDL_Renderer *renderer, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)lround(sqrt((double)radius * radius - (double)dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void ui_draw_rounded_rect(SDL_Renderer *renderer, SDL_Rect rect, int radius,
                           SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    if (radius <= 0 || rect.w <= 0 || rect.h <= 0) {
        SDL_RenderFillRect(renderer, &rect);
        return;
    }
    if (radius * 2 > rect.w) radius = rect.w / 2;
    if (radius * 2 > rect.h) radius = rect.h / 2;

    SDL_Rect center_col = {rect.x + radius, rect.y, rect.w - 2 * radius,
                            rect.h};
    SDL_RenderFillRect(renderer, &center_col);

    SDL_Rect left_col = {rect.x, rect.y + radius, radius,
                          rect.h - 2 * radius};
    SDL_RenderFillRect(renderer, &left_col);

    SDL_Rect right_col = {rect.x + rect.w - radius, rect.y + radius, radius,
                           rect.h - 2 * radius};
    SDL_RenderFillRect(renderer, &right_col);

    fill_circle(renderer, rect.x + radius, rect.y + radius, radius);
    fill_circle(renderer, rect.x + rect.w - radius - 1, rect.y + radius,
                radius);
    fill_circle(renderer, rect.x + radius, rect.y + rect.h - radius - 1,
                radius);
    fill_circle(renderer, rect.x + rect.w - radius - 1,
                rect.y + rect.h - radius - 1, radius);
}

void ui_draw_rounded_rect_outline(SDL_Renderer *renderer, SDL_Rect rect,
                                   int radius, int thickness,
                                   SDL_Color color) {
    for (int i = 0; i < thickness; i++) {
        SDL_Rect r = {rect.x + i, rect.y + i, rect.w - 2 * i,
                       rect.h - 2 * i};
        if (r.w <= 0 || r.h <= 0) break;

        int rad = radius - i;
        if (rad < 0) rad = 0;

        /* Sadece cerceve icin: dis rounded rect'i ciz, sonra icini bir
         * ust katmanla (cagiran taraf zaten arka plani cizdiyse) degil,
         * burada basit yontem: tek pikseli kose yaylari + kenar
         * cizgileriyle olustur. */
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        if (rad <= 0) {
            SDL_RenderDrawRect(renderer, &r);
            continue;
        }

        /* Ust ve alt kenarlar */
        SDL_RenderDrawLine(renderer, r.x + rad, r.y, r.x + r.w - rad, r.y);
        SDL_RenderDrawLine(renderer, r.x + rad, r.y + r.h,
                            r.x + r.w - rad, r.y + r.h);
        /* Sol ve sag kenarlar */
        SDL_RenderDrawLine(renderer, r.x, r.y + rad, r.x, r.y + r.h - rad);
        SDL_RenderDrawLine(renderer, r.x + r.w, r.y + rad, r.x + r.w,
                            r.y + r.h - rad);
    }
}

static void draw_dashed_line(SDL_Renderer *renderer, double x1, double y1,
                              double x2, double y2, int dash_len,
                              int gap_len) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    double len = sqrt(dx * dx + dy * dy);
    if (len < 1.0) return;

    double ux = dx / len;
    double uy = dy / len;
    double pos = 0.0;
    int drawing = 1;

    while (pos < len) {
        double seg = drawing ? (double)dash_len : (double)gap_len;
        double end = pos + seg;
        if (end > len) end = len;

        if (drawing) {
            int sx = (int)lround(x1 + ux * pos);
            int sy = (int)lround(y1 + uy * pos);
            int ex = (int)lround(x1 + ux * end);
            int ey = (int)lround(y1 + uy * end);
            SDL_RenderDrawLine(renderer, sx, sy, ex, ey);
        }
        pos = end;
        drawing = !drawing;
    }
}

void ui_draw_dashed_rect(SDL_Renderer *renderer, SDL_Rect rect, int dash_len,
                          int gap_len, int thickness, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int t = 0; t < thickness; t++) {
        SDL_Rect r = {rect.x + t, rect.y + t, rect.w - 2 * t,
                       rect.h - 2 * t};
        if (r.w <= 0 || r.h <= 0) break;
        draw_dashed_line(renderer, r.x, r.y, r.x + r.w, r.y, dash_len,
                          gap_len);
        draw_dashed_line(renderer, r.x + r.w, r.y, r.x + r.w, r.y + r.h,
                          dash_len, gap_len);
        draw_dashed_line(renderer, r.x + r.w, r.y + r.h, r.x, r.y + r.h,
                          dash_len, gap_len);
        draw_dashed_line(renderer, r.x, r.y + r.h, r.x, r.y, dash_len,
                          gap_len);
    }
}

void ui_draw_zip_icon(SDL_Renderer *renderer, int center_x, int center_y,
                       int size, SDL_Color body_color, SDL_Color zip_color) {
    int w = size;
    int h = (int)(size * 1.3);

    SDL_Rect body = {center_x - w / 2, center_y - h / 2, w, h};
    ui_draw_rounded_rect(renderer, body, size / 7, body_color);

    /* Fermuar zigzag'i */
    SDL_SetRenderDrawColor(renderer, zip_color.r, zip_color.g, zip_color.b,
                            zip_color.a);
    int steps = 7;
    int step_h = h / steps;
    int amplitude = w / 9;
    int px = center_x;
    int py = body.y + 6;
    for (int i = 1; i <= steps; i++) {
        int nx = center_x + ((i % 2 == 0) ? -amplitude : amplitude);
        int ny = body.y + 6 + i * step_h;
        if (ny > body.y + h - 6) ny = body.y + h - 6;
        SDL_RenderDrawLine(renderer, px, py, nx, ny);
        px = nx;
        py = ny;
    }

    /* Fermuar cekecegi (tab) */
    SDL_Rect tab = {center_x - w / 10, body.y - h / 14, w / 5, h / 10};
    ui_draw_rounded_rect(renderer, tab, 2, zip_color);
}

void ui_draw_vertical_gradient(SDL_Renderer *renderer, int w, int h,
                                SDL_Color top, SDL_Color bottom) {
    for (int y = 0; y < h; y++) {
        double t = (h <= 1) ? 0.0 : (double)y / (double)(h - 1);
        Uint8 r = (Uint8)(top.r + (bottom.r - top.r) * t);
        Uint8 g = (Uint8)(top.g + (bottom.g - top.g) * t);
        Uint8 b = (Uint8)(top.b + (bottom.b - top.b) * t);
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawLine(renderer, 0, y, w, y);
    }
}

void ui_draw_status_glyph(SDL_Renderer *renderer, int center_x, int center_y,
                           int size, int success) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    int half = size / 2;

    if (success) {
        /* Check mark: kisa kol + uzun kol */
        SDL_RenderDrawLine(renderer, center_x - half, center_y,
                            center_x - half / 4, center_y + half / 2);
        SDL_RenderDrawLine(renderer, center_x - half / 4, center_y + half / 2,
                            center_x + half, center_y - half / 2);
        /* Kalinlik icin bir piksel kaydirilmis ikinci cizgi seti */
        SDL_RenderDrawLine(renderer, center_x - half, center_y + 1,
                            center_x - half / 4, center_y + half / 2 + 1);
        SDL_RenderDrawLine(renderer, center_x - half / 4,
                            center_y + half / 2 + 1, center_x + half,
                            center_y - half / 2 + 1);
    } else {
        SDL_RenderDrawLine(renderer, center_x - half, center_y - half,
                            center_x + half, center_y + half);
        SDL_RenderDrawLine(renderer, center_x + half, center_y - half,
                            center_x - half, center_y + half);
        SDL_RenderDrawLine(renderer, center_x - half, center_y - half + 1,
                            center_x + half, center_y + half + 1);
        SDL_RenderDrawLine(renderer, center_x + half, center_y - half + 1,
                            center_x - half, center_y + half + 1);
    }
}

SDL_Texture *ui_create_text_texture(SDL_Renderer *renderer, TTF_Font *font,
                                     const char *text, SDL_Color color,
                                     int *out_w, int *out_h) {
    if (!font || !text || !text[0]) {
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return NULL;
    }

    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (out_w) *out_w = surface->w;
    if (out_h) *out_h = surface->h;
    SDL_FreeSurface(surface);
    return texture;
}

void ui_fit_text(TTF_Font *font, const char *text, int max_width_px,
                  char *out, size_t out_size) {
    if (!text) {
        if (out_size) out[0] = '\0';
        return;
    }
    if (!font) {
        snprintf(out, out_size, "%s", text);
        return;
    }

    int w = 0, h = 0;
    if (TTF_SizeUTF8(font, text, &w, &h) == 0 && w <= max_width_px) {
        snprintf(out, out_size, "%s", text);
        return;
    }

    char buf[600];
    size_t len = strlen(text);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, text, len);
    buf[len] = '\0';

    while (len > 0) {
        /* Bir karakter daha at; UTF-8 devam baytlarini (10xxxxxx) da
         * birlikte kirp ki cok baytli bir karakterin ortasinda
         * kalinmasin. */
        len--;
        while (len > 0 && ((unsigned char)buf[len] & 0xC0) == 0x80) {
            len--;
        }
        buf[len] = '\0';
        if (len == 0) break;

        char candidate[620];
        snprintf(candidate, sizeof(candidate), "%s...", buf);
        int cw = 0, ch = 0;
        if (TTF_SizeUTF8(font, candidate, &cw, &ch) == 0 &&
            cw <= max_width_px) {
            snprintf(out, out_size, "%s", candidate);
            return;
        }
    }
    snprintf(out, out_size, "...");
}

void ui_draw_text(SDL_Renderer *renderer, SDL_Texture *texture, int x, int y,
                   int w, int h) {
    if (!texture) return;
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(renderer, texture, NULL, &dst);
}
