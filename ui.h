#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

/* Duz renk, yuvarlatilmis kose (rounded rect) dolgu. radius <= 0 ise
 * normal dikdortgen cizer. */
void ui_draw_rounded_rect(SDL_Renderer *renderer, SDL_Rect rect, int radius,
                           SDL_Color color);

/* Sadece kenarlik (stroke) olarak yuvarlatilmis dikdortgen. */
void ui_draw_rounded_rect_outline(SDL_Renderer *renderer, SDL_Rect rect,
                                   int radius, int thickness,
                                   SDL_Color color);

/* Dikdortgenin dort kenarina kesikli (dashed) cizgi ceker - "buraya
 * birak" bolgesi icin. */
void ui_draw_dashed_rect(SDL_Renderer *renderer, SDL_Rect rect, int dash_len,
                          int gap_len, int thickness, SDL_Color color);

/* Basit bir "zip dosyasi" simgesi: dokumanin ustunde fermuar cizgisi. */
void ui_draw_zip_icon(SDL_Renderer *renderer, int center_x, int center_y,
                       int size, SDL_Color body_color, SDL_Color zip_color);

/* Ic ice iki renk arasinda dikey gecisli (gradient) arka plan. */
void ui_draw_vertical_gradient(SDL_Renderer *renderer, int w, int h,
                                SDL_Color top, SDL_Color bottom);

/* Duz onay (check) veya carpi (X) isareti - islem sonucu geri bildirimi
 * icin. success != 0 ise check, degilse X cizer. */
void ui_draw_status_glyph(SDL_Renderer *renderer, int center_x, int center_y,
                           int size, int success);

/*
 * Verilen metni bir defaya mahsus dokuya (texture) render eder.
 * Cagiran taraf donen texture'i SDL_DestroyTexture ile serbest birakmali.
 * font == NULL ise NULL doner (metinsiz calisma modu).
 */
SDL_Texture *ui_create_text_texture(SDL_Renderer *renderer, TTF_Font *font,
                                     const char *text, SDL_Color color,
                                     int *out_w, int *out_h);

/*
 * text, max_width_px genisligine sigmiyorsa sonuna "..." ekleyerek
 * kisaltilmis bir kopyasini out'a yazar (UTF-8 karakter sinirlarini
 * bozmadan). Sigiyorsa oldugu gibi kopyalar. font == NULL ise metni
 * degistirmeden kopyalar (metinsiz modda olcum yapilamaz).
 * Uzun dosya adlarinin bildirim/gecmis kartlarini tasmasini onlemek
 * icin kullanilir.
 */
void ui_fit_text(TTF_Font *font, const char *text, int max_width_px,
                  char *out, size_t out_size);

/* Onceden olusturulmus bir metin dokusunu belirtilen konuma (sol-orta
 * hizali) cizer. texture == NULL ise sessizce hicbir sey yapmaz. */
void ui_draw_text(SDL_Renderer *renderer, SDL_Texture *texture, int x, int y,
                   int w, int h);

#endif /* UI_H */
