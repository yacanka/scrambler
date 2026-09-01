#ifndef FONT_FINDER_H
#define FONT_FINDER_H

#include <stddef.h>

/*
 * Isletim sisteminde onceden yuklu, yaygin bulunan bir TTF fontunun tam
 * yolunu bulmaya calisir (Windows: Segoe UI/Arial, macOS: SF/Helvetica/
 * Arial, Linux: DejaVu Sans / Liberation Sans / Noto Sans gibi yaygin
 * dagitim fontlari). Program herhangi bir font dosyasi tasimaz; boylece
 * lisans/dagitim derdi olmadan sadece kaynak kod paylasilabilir.
 *
 * Bulunursa 1 ve out_path'e yol yazilir, bulunamazsa 0 doner. Font
 * bulunamamasi ciddi bir hata degildir: uygulama metinsiz, sadece
 * sekil/renk tabanli bir arayuzle calismaya devam eder.
 */
int find_system_font(char *out_path, size_t out_path_size);

#endif /* FONT_FINDER_H */
