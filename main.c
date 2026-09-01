/*
 * ZIP Baslik Araci - Windows / Linux / macOS
 *
 * Pencereye suruklenen dosya:
 *   - Gecerli bir ZIP imzasiyla basliyorsa -> dosyanin TUM bitleri sabit
 *     bir miktar kaydirilarak taninmaz hale getirilir,
 *     "<ad>_scrambled<uzanti>" adiyla yeni bir dosya olusturulur.
 *   - Baslamiyorsa -> bu araçla daha once bozulmus oldugu varsayilir,
 *     ayni miktarda TERS yonde kaydirma uygulanarak orijinal ZIP
 *     imzasinin geri gelmesi saglanir, "<ad>_restored<uzanti>" adiyla
 *     yeni bir dosya olusturulur. Dosya boyutu her iki yonde de
 *     degismez.
 *
 * Orijinal dosyaya asla yazilmaz; sonuc her zaman yeni bir dosyadir.
 *
 * Arayuz: animasyonlu (nefes alan) kesikli cizgili birakma alani, islem
 * sonrasi beliren renkli bildirim (toast) ve son islemleri listeleyen,
 * "Temizle" butonuyla etkilesimli bir gecmis paneli.
 *
 * Bagimlilik: SDL2 + SDL2_ttf. Program herhangi bir font dosyasi
 * tasimaz; isletim sisteminde onceden yuklu yaygin bir fontu bulup
 * kullanir (bkz. font_finder.c). Font bulunamazsa uygulama metinsiz,
 * yalnizca sekil/renk tabanli bir arayuzle calismaya devam eder.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "font_finder.h"
#include "ui.h"
#include "zip_utils.h"

#define WINDOW_WIDTH 760
#define WINDOW_HEIGHT 620
#define MAX_HISTORY 5
#define TOAST_FADE_IN_MS 150
#define TOAST_HOLD_MS 1600
#define TOAST_FADE_OUT_MS 700

/* ---------- Renk paleti ---------- */
static const SDL_Color COL_BG_TOP = {30, 32, 44, 255};
static const SDL_Color COL_BG_BOTTOM = {17, 18, 26, 255};
static const SDL_Color COL_TITLE = {235, 236, 245, 255};
static const SDL_Color COL_SUBTITLE = {150, 155, 172, 255};
static const SDL_Color COL_CARD = {40, 43, 58, 255};
static const SDL_Color COL_ACCENT_A = {90, 112, 210, 255};
static const SDL_Color COL_ACCENT_B = {140, 160, 255, 255};
static const SDL_Color COL_DROP_TEXT = {222, 224, 234, 255};
static const SDL_Color COL_DROP_SUBTEXT = {142, 146, 164, 255};
static const SDL_Color COL_HISTORY_HEADER = {198, 201, 215, 255};
static const SDL_Color COL_HISTORY_TEXT = {212, 215, 226, 255};
static const SDL_Color COL_HISTORY_EMPTY = {120, 124, 140, 255};
static const SDL_Color COL_BUTTON = {55, 58, 76, 255};
static const SDL_Color COL_BUTTON_HOVER = {76, 80, 102, 255};
static const SDL_Color COL_BUTTON_TEXT = {218, 220, 230, 255};
static const SDL_Color COL_SUCCESS = {60, 180, 105, 255};
static const SDL_Color COL_ERROR = {212, 74, 74, 255};

typedef struct {
    char message[256];
    int success;
    SDL_Texture *tex;
    int tex_w, tex_h;
} HistoryEntry;

typedef struct {
    HistoryEntry entries[MAX_HISTORY];
    int count;
} History;

typedef struct {
    SDL_Texture *tex;
    int tex_w, tex_h;
    int success;
    Uint32 start_ticks;
    int active;
} Toast;

typedef struct {
    TTF_Font *title;
    TTF_Font *body;
    TTF_Font *small;
} Fonts;

typedef struct {
    SDL_Texture *title, *subtitle, *drop_main, *drop_sub, *history_header,
        *history_empty, *clear_button;
    int w_title, h_title, w_subtitle, h_subtitle, w_drop_main, h_drop_main,
        w_drop_sub, h_drop_sub, w_history_header, h_history_header,
        w_history_empty, h_history_empty, w_clear_button, h_clear_button;
} StaticTexts;

static void history_push(History *hist, SDL_Renderer *renderer,
                          TTF_Font *font, const char *message, int success,
                          int max_width_px) {
    int old_count = hist->count;
    int last = (old_count == MAX_HISTORY) ? MAX_HISTORY - 1 : old_count;

    if (old_count == MAX_HISTORY && hist->entries[MAX_HISTORY - 1].tex) {
        SDL_DestroyTexture(hist->entries[MAX_HISTORY - 1].tex);
    }
    for (int i = last; i > 0; i--) {
        hist->entries[i] = hist->entries[i - 1];
    }
    if (old_count < MAX_HISTORY) {
        hist->count = old_count + 1;
    }

    HistoryEntry *e = &hist->entries[0];
    snprintf(e->message, sizeof(e->message), "%s", message);
    e->success = success;
    SDL_Color color = success ? COL_HISTORY_TEXT : (SDL_Color){255, 205, 205, 255};

    char fitted[300];
    ui_fit_text(font, message, max_width_px, fitted, sizeof(fitted));
    e->tex = ui_create_text_texture(renderer, font, fitted, color,
                                     &e->tex_w, &e->tex_h);
}

static void history_clear(History *hist) {
    for (int i = 0; i < hist->count; i++) {
        if (hist->entries[i].tex) {
            SDL_DestroyTexture(hist->entries[i].tex);
            hist->entries[i].tex = NULL;
        }
    }
    hist->count = 0;
}

static void toast_show(Toast *toast, SDL_Renderer *renderer, TTF_Font *font,
                        const char *message, int success, int max_width_px) {
    if (toast->tex) {
        SDL_DestroyTexture(toast->tex);
        toast->tex = NULL;
    }
    SDL_Color white = {255, 255, 255, 255};
    char fitted[300];
    ui_fit_text(font, message, max_width_px, fitted, sizeof(fitted));
    toast->tex = ui_create_text_texture(renderer, font, fitted, white,
                                         &toast->tex_w, &toast->tex_h);
    toast->success = success;
    toast->start_ticks = SDL_GetTicks();
    toast->active = 1;
}

static int point_in_rect(int x, int y, SDL_Rect r) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static void handle_dropped_file(const char *dropped_path,
                                 char *out_message, size_t out_message_size,
                                 int *out_success) {
    char output_path[2048];
    int was_zip = is_zip_file(dropped_path);

    /* Sadece dosya adini goster, tam yolu degil - daha okunakli. */
    const char *base = strrchr(dropped_path, '/');
    const char *base2 = strrchr(dropped_path, '\\');
    if (base2 && (!base || base2 > base)) base = base2;
    base = base ? base + 1 : dropped_path;

    if (process_dropped_file(dropped_path, output_path,
                              sizeof(output_path)) == 0) {
        const char *out_name = strrchr(output_path, '/');
        const char *out_name2 = strrchr(output_path, '\\');
        if (out_name2 && (!out_name || out_name2 > out_name)) out_name = out_name2;
        out_name = out_name ? out_name + 1 : output_path;

        *out_success = 1;
        snprintf(out_message, out_message_size,
                 was_zip ? "%s -> bitler kaydirilarak bozuldu -> %s"
                         : "%s -> ters kaydirma ile geri getirildi -> %s",
                 base, out_name);
    } else {
        *out_success = 0;
        snprintf(out_message, out_message_size, "%s islenemedi", base);
    }

    printf("%s\n", out_message);
    fflush(stdout);
}

static void render_header(SDL_Renderer *renderer, SDL_Rect area,
                           const StaticTexts *st) {
    int x = area.x + (area.w - st->w_title) / 2;
    ui_draw_text(renderer, st->title, x, area.y, st->w_title, st->h_title);

    int sx = area.x + (area.w - st->w_subtitle) / 2;
    ui_draw_text(renderer, st->subtitle, sx, area.y + st->h_title + 4,
                 st->w_subtitle, st->h_subtitle);
}

static void render_dropzone(SDL_Renderer *renderer, SDL_Rect rect,
                             const StaticTexts *st, double breathe) {
    ui_draw_rounded_rect(renderer, rect, 18, COL_CARD);

    SDL_Color border;
    border.r = (Uint8)(COL_ACCENT_A.r + (COL_ACCENT_B.r - COL_ACCENT_A.r) * breathe);
    border.g = (Uint8)(COL_ACCENT_A.g + (COL_ACCENT_B.g - COL_ACCENT_A.g) * breathe);
    border.b = (Uint8)(COL_ACCENT_A.b + (COL_ACCENT_B.b - COL_ACCENT_A.b) * breathe);
    border.a = 255;

    SDL_Rect inset = {rect.x + 12, rect.y + 12, rect.w - 24, rect.h - 24};
    ui_draw_dashed_rect(renderer, inset, 10, 7, 2, border);

    int icon_cy = rect.y + rect.h / 2 - 30;
    ui_draw_zip_icon(renderer, rect.x + rect.w / 2, icon_cy, 54, COL_TITLE,
                      border);

    int text_y = icon_cy + 55;
    int x = rect.x + (rect.w - st->w_drop_main) / 2;
    ui_draw_text(renderer, st->drop_main, x, text_y, st->w_drop_main,
                 st->h_drop_main);

    int sx = rect.x + (rect.w - st->w_drop_sub) / 2;
    ui_draw_text(renderer, st->drop_sub, sx, text_y + st->h_drop_main + 6,
                 st->w_drop_sub, st->h_drop_sub);
}

static void render_toast(SDL_Renderer *renderer, SDL_Rect dropzone_rect,
                          Toast *toast) {
    if (!toast->active) return;

    Uint32 elapsed = SDL_GetTicks() - toast->start_ticks;
    Uint32 total = TOAST_FADE_IN_MS + TOAST_HOLD_MS + TOAST_FADE_OUT_MS;
    if (elapsed >= total) {
        toast->active = 0;
        return;
    }

    double alpha;
    if (elapsed < TOAST_FADE_IN_MS) {
        alpha = (double)elapsed / TOAST_FADE_IN_MS;
    } else if (elapsed < TOAST_FADE_IN_MS + TOAST_HOLD_MS) {
        alpha = 1.0;
    } else {
        double t = (double)(elapsed - TOAST_FADE_IN_MS - TOAST_HOLD_MS) /
                   TOAST_FADE_OUT_MS;
        alpha = 1.0 - t;
    }
    if (alpha < 0) alpha = 0;
    if (alpha > 1) alpha = 1;

    SDL_Color base = toast->success ? COL_SUCCESS : COL_ERROR;
    int pad_x = 22, pad_y = 12;
    int w = (toast->tex_w > 0 ? toast->tex_w : 120) + pad_x * 2 + 34;
    int h = (toast->tex_h > 0 ? toast->tex_h : 18) + pad_y * 2;
    /* Guvenlik payi: metin ui_fit_text ile zaten sinirlandirildi, ama
     * pencere cok kucultulmus olsa bile karti asla dropzone disina
     * tasirma. */
    int max_w = dropzone_rect.w - 24;
    if (w > max_w) w = max_w;
    int x = dropzone_rect.x + (dropzone_rect.w - w) / 2;
    int y = dropzone_rect.y + 18;

    SDL_Rect card = {x, y, w, h};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_Color faded = base;
    faded.a = (Uint8)(230 * alpha);
    ui_draw_rounded_rect(renderer, card, 12, faded);

    ui_draw_status_glyph(renderer, x + pad_x + 8, y + h / 2, 14,
                          toast->success);

    if (toast->tex) {
        SDL_SetTextureAlphaMod(toast->tex, (Uint8)(255 * alpha));
        ui_draw_text(renderer, toast->tex, x + pad_x + 26, y + pad_y,
                     toast->tex_w, toast->tex_h);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void render_history(SDL_Renderer *renderer, SDL_Rect rect,
                            const StaticTexts *st, const History *hist,
                            SDL_Rect clear_btn_rect, int clear_hover) {
    ui_draw_rounded_rect(renderer, rect, 14, COL_CARD);

    int pad = 18;
    ui_draw_text(renderer, st->history_header, rect.x + pad, rect.y + pad,
                 st->w_history_header, st->h_history_header);

    ui_draw_rounded_rect(renderer, clear_btn_rect, 8,
                          clear_hover ? COL_BUTTON_HOVER : COL_BUTTON);
    int bx = clear_btn_rect.x + (clear_btn_rect.w - st->w_clear_button) / 2;
    int by = clear_btn_rect.y + (clear_btn_rect.h - st->h_clear_button) / 2;
    ui_draw_text(renderer, st->clear_button, bx, by, st->w_clear_button,
                 st->h_clear_button);

    int list_y = rect.y + pad + st->h_history_header + 14;
    int row_h = 30;

    if (hist->count == 0) {
        ui_draw_text(renderer, st->history_empty, rect.x + pad, list_y,
                     st->w_history_empty, st->h_history_empty);
        return;
    }

    for (int i = 0; i < hist->count; i++) {
        int row_y = list_y + i * row_h;
        if (row_y + row_h > rect.y + rect.h - 8) break;

        const HistoryEntry *e = &hist->entries[i];
        ui_draw_status_glyph(renderer, rect.x + pad + 8, row_y + row_h / 2,
                              12, e->success);
        if (e->tex) {
            ui_draw_text(renderer, e->tex, rect.x + pad + 26, row_y + 4,
                         e->tex_w, e->tex_h);
        }
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init hatasi: %s\n", SDL_GetError());
        return 1;
    }

    int ttf_ok = (TTF_Init() == 0);
    if (!ttf_ok) {
        fprintf(stderr, "TTF_Init basarisiz, metinsiz modda devam: %s\n",
                TTF_GetError());
    }

    Fonts fonts = {0};
    if (ttf_ok) {
        char font_path[1024];
        if (find_system_font(font_path, sizeof(font_path))) {
            fonts.title = TTF_OpenFont(font_path, 26);
            fonts.body = TTF_OpenFont(font_path, 16);
            fonts.small = TTF_OpenFont(font_path, 13);
        } else {
            fprintf(stderr,
                    "Sistemde uygun bir font bulunamadi, metinsiz modda "
                    "devam ediliyor.\n");
        }
    }

    SDL_Window *window = SDL_CreateWindow(
        "ZIP Baslik Araci", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow hatasi: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer hatasi: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    /* Statik metin dokularini bir kez olustur. */
    StaticTexts st = {0};
    st.title = ui_create_text_texture(renderer, fonts.title,
                                       "ZIP Baslik Araci", COL_TITLE,
                                       &st.w_title, &st.h_title);
    st.subtitle = ui_create_text_texture(
        renderer, fonts.small,
        "Surukle-birak ile ZIP imzasini sil ya da geri ekle", COL_SUBTITLE,
        &st.w_subtitle, &st.h_subtitle);
    st.drop_main = ui_create_text_texture(renderer, fonts.body,
                                           "ZIP dosyasini buraya surukle",
                                           COL_DROP_TEXT, &st.w_drop_main,
                                           &st.h_drop_main);
    st.drop_sub = ui_create_text_texture(
        renderer, fonts.small, "Sonuc, orijinali degistirmeden yeni bir dosyaya kaydedilir",
        COL_DROP_SUBTEXT, &st.w_drop_sub, &st.h_drop_sub);
    st.history_header = ui_create_text_texture(
        renderer, fonts.body, "Son islemler", COL_HISTORY_HEADER,
        &st.w_history_header, &st.h_history_header);
    st.history_empty = ui_create_text_texture(
        renderer, fonts.small, "Henuz islem yapilmadi.", COL_HISTORY_EMPTY,
        &st.w_history_empty, &st.h_history_empty);
    st.clear_button = ui_create_text_texture(renderer, fonts.small,
                                              "Temizle", COL_BUTTON_TEXT,
                                              &st.w_clear_button,
                                              &st.h_clear_button);

    History history = {0};
    Toast toast = {0};

    int running = 1;
    SDL_Event event;
    int mouse_x = 0, mouse_y = 0;

    while (running) {
        /* Bu karenin yerlesim dikdortgenlerini olay isleme baslamadan
         * ONCE hesapla. Boylece ayni olay grubunda once fare hareketi
         * sonra tikma gelirse (ornegin hizli/programatik tiklamalarda),
         * tiklama testi bir onceki karenin degil BU karenin buton
         * konumuna gore yapilir. */
        int w, h;
        SDL_GetRendererOutputSize(renderer, &w, &h);

        int pad = 24;
        SDL_Rect header_rect = {pad, pad, w - 2 * pad, 70};

        int history_h = 210;
        int gap = 16;
        int dropzone_y = header_rect.y + header_rect.h + gap;
        int dropzone_h = h - dropzone_y - history_h - gap - pad;
        if (dropzone_h < 90) dropzone_h = 90;
        SDL_Rect dropzone_rect = {pad, dropzone_y, w - 2 * pad, dropzone_h};

        int history_y = dropzone_y + dropzone_h + gap;
        SDL_Rect history_rect = {pad, history_y, w - 2 * pad, history_h};

        SDL_Rect clear_btn_rect = {history_rect.x + history_rect.w - 100 - 18,
                                    history_rect.y + 14, 100, 30};

        /* render_history / render_toast icindeki yerlesim sabitleriyle
         * (pad, ikon bosluğu, kart kenar boşluğu) uyumlu, metnin
         * kartlarin disina taşmasini onleyen guvenli genislikler. */
        int history_text_max_w = history_rect.w - 18 /*pad*/ -
                                  26 /*ikon offseti*/ - 16 /*sag pay*/;
        int toast_text_max_w = dropzone_rect.w - 48 /*kart yan boşluğu*/ -
                                44 /*toast ic pad_x*2*/ -
                                34 /*ikon+bosluk*/;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;

                case SDL_DROPFILE: {
                    char *dropped_path = event.drop.file;
                    if (dropped_path) {
                        char message[256];
                        int success = 0;
                        handle_dropped_file(dropped_path, message,
                                             sizeof(message), &success);
                        history_push(&history, renderer, fonts.small,
                                     message, success, history_text_max_w);
                        toast_show(&toast, renderer, fonts.body, message,
                                   success, toast_text_max_w);
                        SDL_free(dropped_path);
                    }
                    break;
                }

                case SDL_MOUSEMOTION: {
                    /* Yuksek DPI ekranlarda pencere/renderer olcegini
                     * hizalamak icin ciz alani (drawable) boyutuna gore
                     * olcekle. */
                    int win_w, win_h, draw_w, draw_h;
                    SDL_GetWindowSize(window, &win_w, &win_h);
                    SDL_GetRendererOutputSize(renderer, &draw_w, &draw_h);
                    double scale_x = win_w ? (double)draw_w / win_w : 1.0;
                    double scale_y = win_h ? (double)draw_h / win_h : 1.0;
                    mouse_x = (int)(event.motion.x * scale_x);
                    mouse_y = (int)(event.motion.y * scale_y);
                    break;
                }

                case SDL_MOUSEBUTTONDOWN: {
                    /* mouse_x/mouse_y bu noktada, ayni olay grubunda
                     * onceden islenmis olabilecek MOUSEMOTION'dan sonraki
                     * en guncel degeri tasir; clear_btn_rect de bu
                     * karenin yerlesimine ait - boylece "hareket et ve
                     * ayni anda tikla" durumunda gecikme yasanmaz. */
                    if (event.button.button == SDL_BUTTON_LEFT &&
                        point_in_rect(mouse_x, mouse_y, clear_btn_rect)) {
                        history_clear(&history);
                    }
                    break;
                }

                default:
                    break;
            }
        }

        int clear_hover = point_in_rect(mouse_x, mouse_y, clear_btn_rect);

        double breathe = (sin(SDL_GetTicks() * 0.0022) + 1.0) / 2.0;

        ui_draw_vertical_gradient(renderer, w, h, COL_BG_TOP, COL_BG_BOTTOM);
        render_header(renderer, header_rect, &st);
        render_dropzone(renderer, dropzone_rect, &st, breathe);
        render_history(renderer, history_rect, &st, &history, clear_btn_rect,
                        clear_hover);
        render_toast(renderer, dropzone_rect, &toast);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    history_clear(&history);
    if (toast.tex) SDL_DestroyTexture(toast.tex);

    SDL_Texture **static_textures[] = {&st.title,          &st.subtitle,
                                        &st.drop_main,      &st.drop_sub,
                                        &st.history_header, &st.history_empty,
                                        &st.clear_button};
    for (size_t i = 0; i < sizeof(static_textures) / sizeof(static_textures[0]); i++) {
        if (*static_textures[i]) SDL_DestroyTexture(*static_textures[i]);
    }

    if (fonts.title) TTF_CloseFont(fonts.title);
    if (fonts.body) TTF_CloseFont(fonts.body);
    if (fonts.small) TTF_CloseFont(fonts.small);
    if (ttf_ok) TTF_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
