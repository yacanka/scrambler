#ifndef ZIP_UTILS_H
#define ZIP_UTILS_H

#include <stddef.h>

/* Standart ZIP "local file header" imza baytlari: 50 4B 03 04 ("PK\3\4") */
extern const unsigned char ZIP_SIGNATURE[4];

/*
 * Dosyanin ilk 4 baytini standart ZIP imzasiyla karsilastirir.
 * Dosya acilamiyorsa veya 4 bayttan kisaysa 0 (zip degil) doner.
 */
int is_zip_file(const char *filepath);

/*
 * Suruklenen dosyayi isler. Ilk 4 bayti eklemek/silmek yerine, dosyanin
 * TUM icerigi dairesel (circular) bir bit kaydirmasindan gecirilir -
 * boylece sadece baslik degil, dosyanin butun bayt deseni degisir ve
 * dosya boyutu birebir ayni kalir:
 *
 *  - Zip ise (standart imzayla basliyorsa): butun dosya sabit bir
 *    miktar sola bit-kaydirilarak "taninmaz" hale getirilir, sonuc
 *    "<ad>_scrambled<uzanti>" adiyla yeni bir dosyaya yazilir.
 *  - Zip degilse: bu tool tarafindan daha once kaydirilmis oldugu
 *    varsayilir; ayni miktarda TERS yonde (saga) bit-kaydirma
 *    uygulanarak orijinal zip imzasinin geri gelmesi beklenir, sonuc
 *    "<ad>_restored<uzanti>" adiyla yeni bir dosyaya yazilir.
 *
 * Bu iki islem matematiksel olarak birbirinin tam tersidir: bir dosyayi
 * bu fonksiyonla bozup sonra cikan dosyayi tekrar bu fonksiyona
 * verdiginizde, orijinal bayt bayt ayni veriyi geri alirsiniz - ama
 * yalnizca dosya gercekten bu araçla bozulmussa anlamlidir; rastgele
 * bir dosyayi "geri yuklemeye" calismak yine rastgele veri uretir.
 *
 * Orijinal dosyaya asla yazilmaz. output_path / output_path_size,
 * uretilen yeni dosyanin tam yolunu almak icin kullanicinin sagladigi
 * tampondur.
 *
 * Donus: basarili olursa 0, hata olursa -1.
 */
int process_dropped_file(const char *input_path, char *output_path,
                          size_t output_path_size);

#endif /* ZIP_UTILS_H */
