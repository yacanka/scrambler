#include "zip_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const unsigned char ZIP_SIGNATURE[4] = {0x50, 0x4B, 0x03, 0x04};

/*
 * Kaydirma miktari: 1-7 arasinda, 8'e (bir bayt) tam bolunmeyen bir
 * deger olmali. Boylece kaydirma bayt sinirlarini "kirar" ve her ciktı
 * baytini komsu iki girdi baytindan olusturur - sonuc olarak butun
 * dosya (baslik dahil) taninmaz hale gelir. 8'in kati bir deger (0 veya
 * 8) secilirse islem sadece baytlari yer degistirir, herhangi bir bit
 * kirilmasi olmaz ve ZIP imzasi baska bir konumda aynen korunabilir -
 * bu yuzden burada sabit tutuluyor.
 */
#define ROTATE_SHIFT_BITS 3

int is_zip_file(const char *filepath) {
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
    return memcmp(header, ZIP_SIGNATURE, sizeof(header)) == 0;
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
static void build_output_path(const char *input_path, const char *suffix,
                               char *output_path, size_t output_path_size) {
    const char *last_sep = find_last_separator(input_path);
    const char *filename = last_sep ? last_sep + 1 : input_path;
    const char *dot = strrchr(filename, '.');

    size_t dir_len = last_sep ? (size_t)(last_sep - input_path + 1) : 0;
    size_t name_len = dot ? (size_t)(dot - filename) : strlen(filename);
    const char *ext_part = dot ? dot : "";

    if (dir_len >= output_path_size) {
        dir_len = 0; /* guvenlik: siginmiyorsa dizini atla */
    }

    int written = snprintf(output_path, output_path_size, "%.*s%.*s%s%s",
                            (int)dir_len, input_path, (int)name_len, filename,
                            suffix, ext_part);
    (void)written;
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

    if (is_zip_file(input_path)) {
        /* Taninan zip -> bitleri kaydirarak taninmaz hale getir. */
        build_output_path(input_path, "_scrambled", output_path,
                           output_path_size);
        return rotate_stream_left(input_path, output_path,
                                   ROTATE_SHIFT_BITS);
    } else {
        /* Taninmayan dosya -> daha once uygulanmis kaydirmayi tersine
         * alarak zip'i geri getirmeyi dene. */
        build_output_path(input_path, "_restored", output_path,
                           output_path_size);
        return rotate_stream_right(input_path, output_path,
                                    ROTATE_SHIFT_BITS);
    }
}
