#include "scrambler/zip_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const unsigned char ZIP_SIGNATURE[4] = {0x50, 0x4B, 0x03, 0x04};

static const unsigned char ZIP_SIGNATURES[][4] = {
    {0x50, 0x4B, 0x03, 0x04}, /* Local file header */
    {0x50, 0x4B, 0x05, 0x06}, /* Empty archive / end of central directory */
    {0x50, 0x4B, 0x07, 0x08}  /* Spanned archive */
};

/*
 * Kaydirma miktari: 1-7 arasinda, 8'e (bir bayt) tam bolunmeyen bir
 * deger olmali. Boylece kaydirma bayt sinirlarini "kirar" ve her cikti
 * baytini komsu iki girdi baytindan olusturur - sonuc olarak butun
 * dosya (baslik dahil) taninmaz hale gelir. 8'in kati bir deger (0 veya
 * 8) secilirse islem sadece baytlari yer degistirir, herhangi bir bit
 * kirilmasi olmaz ve ZIP imzasi baska bir konumda aynen korunabilir -
 * bu yuzden burada sabit tutuluyor.
 */
#define ROTATE_SHIFT_BITS 3

int is_zip_file(const char *filepath) {
    if (!filepath) return 0;

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        return 0;
    }

    unsigned char header[4] = {0};
    size_t n = fread(header, 1, sizeof(header), f);
    fclose(f);

    if (n < sizeof(header)) {
        return 0;
    }
    for (size_t i = 0; i < sizeof(ZIP_SIGNATURES) / sizeof(ZIP_SIGNATURES[0]);
         i++) {
        if (memcmp(header, ZIP_SIGNATURES[i], sizeof(header)) == 0) {
            return 1;
        }
    }
    return 0;
}

/* input_path icindeki son dizin ayiricisini bulur ('/' veya '\').
 * Windows, Linux ve macOS yollarinin tumunu destekler. */
static const char *find_last_separator(const char *path) {
    const char *last_fwd = strrchr(path, '/');
    const char *last_back = strrchr(path, '\\');
    if (last_fwd && last_back) {
        return (last_fwd > last_back) ? last_fwd : last_back;
    }
    return last_fwd ? last_fwd : last_back;
}

/* input_path'ten, ayni dizinde ve dosya adina "suffix" eklenmis yeni bir
 * dosya yolu uretir. Ornek: "C:\a\rapor.zip" + "_scrambled" ->
 * "C:\a\rapor_scrambled.zip" */
static int build_output_path(const char *input_path, const char *suffix,
                              char *output_path, size_t output_path_size) {
    if (!input_path || !suffix || !output_path || output_path_size == 0) {
        return -1;
    }

    const char *last_sep = find_last_separator(input_path);
    const char *filename = last_sep ? last_sep + 1 : input_path;
    const char *dot = strrchr(filename, '.');
    if (dot == filename) dot = NULL; /* .hidden gibi adlar uzanti degildir. */

    size_t dir_len = last_sep ? (size_t)(last_sep - input_path + 1) : 0;
    size_t name_len = dot ? (size_t)(dot - filename) : strlen(filename);
    const char *ext_part = dot ? dot : "";
    size_t suffix_len = strlen(suffix);
    size_t ext_len = strlen(ext_part);

    size_t remaining = output_path_size;
    if (dir_len >= remaining) return -1;
    remaining -= dir_len;
    if (name_len >= remaining) return -1;
    remaining -= name_len;
    if (suffix_len >= remaining) return -1;
    remaining -= suffix_len;
    if (ext_len >= remaining) return -1;

    size_t used = 0;
    memcpy(output_path + used, input_path, dir_len);
    used += dir_len;
    memcpy(output_path + used, filename, name_len);
    used += name_len;
    memcpy(output_path + used, suffix, suffix_len);
    used += suffix_len;
    memcpy(output_path + used, ext_part, ext_len);
    used += ext_len;
    output_path[used] = '\0';
    return 0;
}

/*
 * Dosyanin TAMAMINI, dairesel (circular) bir bit dizisi gibi ele alip
 * `shift` bit SOLA kaydirir. Cikti, girdiyle birebir ayni boyuttadir.
 *
 * Akis (streaming) mantigi: her cikti bayti, o konumdaki girdi baytiyla
 * bir sonraki girdi baytinin birlesiminden olusur. Dosyanin EN SON
 * baytinin "bir sonrakisi" olarak, dosyanin EN BASTAKI bayti kullanilir
 * (dairesel sarma) - bu yuzden ilk bayt onceden okunup saklanir.
 *
 * Butun dosyayi belleğe yuklemeden, sabit (O(1)) ek bellekle calisir;
 * boylece buyuk dosyalarda da sorunsuzdur.
 */
static int rotate_stream_left(const char *input_path, const char *output_path,
                               int shift) {
    FILE *in = fopen(input_path, "rb");
    if (!in) return -1;
    FILE *out = fopen(output_path, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    int ok = 1;
    int first_int = fgetc(in);

    if (first_int != EOF) {
        unsigned char first_byte = (unsigned char)first_int;
        unsigned char current = first_byte;
        int next_int;

        while ((next_int = fgetc(in)) != EOF) {
            unsigned char next = (unsigned char)next_int;
            unsigned char out_byte =
                (unsigned char)((current << shift) | (next >> (8 - shift)));
            if (fputc(out_byte, out) == EOF) {
                ok = 0;
                break;
            }
            current = next;
        }

        if (ok) {
            /* Son bayt: dairesel sarma - "bir sonraki" olarak dosyanin
             * ilk baytini kullan. */
            unsigned char out_byte = (unsigned char)(
                (current << shift) | (first_byte >> (8 - shift)));
            if (fputc(out_byte, out) == EOF) {
                ok = 0;
            }
        }
    }

    if (ferror(in)) ok = 0;
    fclose(in);
    if (fclose(out) != 0) ok = 0;
    if (!ok) remove(output_path);
    return ok ? 0 : -1;
}

/*
 * rotate_stream_left'in tam tersi: dosyanin tamamini `shift` bit SAGA
 * kaydirir (dairesel). rotate_stream_left ile uretilmis bir dosyaya
 * ayni `shift` degeriyle uygulandiginda, orijinal bayt dizisini
 * birebir geri verir.
 *
 * Burada her cikti bayti, o konumdaki girdi baytiyla BIR ONCEKI girdi
 * baytinin birlesiminden olusur; bu yuzden akisa baslamadan once
 * dosyanin EN SON baytini bilmemiz gerekir (dairesel sarma: ilk cikti
 * baytinin "bir oncekisi" olarak kullanilir). Bunun icin dosyanin
 * sonuna atlayip son bayti okuyoruz, sonra basa donup normal akisla
 * ilerliyoruz - boylece yine butun dosyayi belleğe yuklemeden,
 * sabit ek bellekle calisir.
 */
static int rotate_stream_right(const char *input_path,
                                const char *output_path, int shift) {
    FILE *in = fopen(input_path, "rb");
    if (!in) return -1;

    if (fseek(in, 0, SEEK_END) != 0) {
        fclose(in);
        return -1;
    }
    long size = ftell(in);
    if (size < 0) {
        fclose(in);
        return -1;
    }

    FILE *out = fopen(output_path, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    int ok = 1;

    if (size > 0) {
        if (fseek(in, -1, SEEK_END) != 0) {
            ok = 0;
        }
        int last_int = ok ? fgetc(in) : EOF;
        if (last_int == EOF) {
            ok = 0;
        }
        unsigned char prev = (unsigned char)last_int;

        if (ok && fseek(in, 0, SEEK_SET) != 0) {
            ok = 0;
        }

        int current_int;
        while (ok && (current_int = fgetc(in)) != EOF) {
            unsigned char current = (unsigned char)current_int;
            unsigned char out_byte =
                (unsigned char)((current >> shift) | (prev << (8 - shift)));
            if (fputc(out_byte, out) == EOF) {
                ok = 0;
                break;
            }
            prev = current;
        }
        if (ferror(in)) ok = 0;
    }

    fclose(in);
    if (fclose(out) != 0) ok = 0;
    if (!ok) remove(output_path);
    return ok ? 0 : -1;
}

int process_dropped_file(const char *input_path, char *output_path,
                          size_t output_path_size) {
    if (!input_path || !output_path || output_path_size == 0) {
        return -1;
    }
    if (input_path == output_path) return -1;
    output_path[0] = '\0';

    int result;
    if (is_zip_file(input_path)) {
        /* Taninan zip -> bitleri kaydirarak taninmaz hale getir. */
        if (build_output_path(input_path, "_scrambled", output_path,
                              output_path_size) != 0) {
            return -1;
        }
        result = rotate_stream_left(input_path, output_path,
                                    ROTATE_SHIFT_BITS);
    } else {
        /* Taninmayan dosya -> daha once uygulanmis kaydirmayi tersine
         * alarak zip'i geri getirmeyi dene. */
        if (build_output_path(input_path, "_restored", output_path,
                              output_path_size) != 0) {
            return -1;
        }
        result = rotate_stream_right(input_path, output_path,
                                     ROTATE_SHIFT_BITS);
    }

    if (result != 0) output_path[0] = '\0';
    return result;
}
