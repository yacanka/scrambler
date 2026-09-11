/*
 * ZIP Baslik Araci - Konsol surumu (tek dosya, bagimsizlik yok)
 * Windows / Linux / macOS
 * ---------------------------------------------------------------
 * Bu tek main.c dosyasi, sadece standart C kutuphanesini (C99)
 * kullanir. Hicbir dis kutuphaneye (SDL2 vb.) ihtiyac duymaz; ekstra
 * kurulum yapmadan herhangi bir C derleyicisiyle derlenebilir:
 *
 *   gcc -O2 -o zip_header_tool main.c          (Linux/macOS, gcc)
 *   clang -O2 -o zip_header_tool main.c        (macOS, clang)
 *   cl /O2 main.c                              (Windows, MSVC)
 *   gcc -O2 -o zip_header_tool.exe main.c      (Windows, MinGW)
 *
 * NE YAPAR
 * --------
 * Verilen bir dosyanin TAMAMINI dairesel (circular) bir bit
 * kaydirmasindan gecirir:
 *   - Dosya standart ZIP imzasiyla (`PK\x03\x04`) basliyorsa -> 3 bit
 *     SOLA kaydirilarak taninmaz hale getirilir (butun baytlar
 *     degisir, dosya boyutu AYNI kalir). Sonuc "<ad>_scrambled<uzanti>"
 *     adiyla yeni bir dosyaya yazilir.
 *   - Baslamiyorsa -> bu araçla daha once bozulmus oldugu varsayilir;
 *     ayni miktar TERS yonde (saga) kaydirilarak orijinal ZIP imzasinin
 *     geri gelmesi saglanir. Sonuc "<ad>_restored<uzanti>" adiyla yeni
 *     bir dosyaya yazilir.
 * Orijinal dosyaya ASLA yazilmaz.
 *
 * DOSYAYI PROGRAMA VERME (SURUKLE-BIRAK)
 * ---------------------------------------
 * Isletim sistemleri, bir dosyanin DERLENMIS BIR KONSOL PROGRAMININ
 * UZERINE suruklenmesini farkli sekillerde ele alir:
 *
 *   - Windows: Dosyayi dogrudan .exe simgesinin uzerine surukleyip
 *     birakabilirsiniz. Windows Gezgini, dosya yolunu otomatik olarak
 *     programa komut satiri argumani (argv[1]) olarak gecirip programi
 *     baslatir. Bu programda destekleniyor.
 *   - Linux / macOS: Cogu dosya yoneticisi (Nautilus, Finder), calisan
 *     bir programa dosya surukleyip birakmayi normal bir program
 *     simgesi icin desteklemez (bu, .app / .desktop gibi dosya
 *     iliskilendirmesi gerektirir). Bunun yerine, PROGRAMI bir
 *     terminalden calistirin, ACILAN TERMINAL PENCERESINE dosyayi
 *     surukleyip birakin - bu, terminaller tarafindan yaygin olarak
 *     desteklenir ve dosya yolunu otomatik olarak yaziya donusturur.
 *     Program, boyle "suruklenip yapistirilan" bir satiri okuyup
 *     isleyecek sekilde tasarlandi (bkz. asagidaki interaktif dongu).
 *
 * Ozetle: hem "programin uzerine surukle" (argv araciligiyla) hem de
 * "calisan konsol penceresinin icine surukle" (stdin araciligiyla)
 * desteklenir. argv ile baslatilan program verilen dosyalari isleyip
 * kapanir; argumansiz baslatilan program stdin uzerinden yeni yollar
 * beklemeye devam eder.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================ */
/*  ZIP tespiti ve bit-kaydirma tabanli bozma/geri getirme       */
/* ============================================================ */

/* Standart ZIP "local file header" imza baytlari: 50 4B 03 04 ("PK\3\4") */
static const unsigned char ZIP_SIGNATURE[4] = {0x50, 0x4B, 0x03, 0x04};

/*
 * Kaydirma miktari: 1-7 arasinda, 8'e (bir bayt) tam bolunmeyen bir
 * deger olmali. Boylece kaydirma bayt sinirlarini "kirar" ve her ciktı
 * baytini komsu iki girdi baytindan olusturur - sonuc olarak butun
 * dosya (baslik dahil) taninmaz hale gelir.
 */
#define ROTATE_SHIFT_BITS 3

/* Dosyanin ilk 4 baytini standart ZIP imzasiyla karsilastirir.
 * Dosya acilamiyorsa veya 4 bayttan kisaysa 0 (zip degil) doner. */
static int is_zip_file(const char *filepath) {
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
 * Dosyanin EN SON baytinin "bir sonrakisi" olarak, dosyanin EN
 * BASTAKI bayti kullanilir (dairesel sarma). Sabit (O(1)) ek bellekle
 * calisir; buyuk dosyalarda da sorunsuzdur.
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
 * kaydirir (dairesel). Once dosyanin son baytini okumak icin sona
 * atlar, sonra basa donup normal akisla ilerler - boylece yine sabit
 * ek bellekle calisir.
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

/*
 * Verilen dosyayi isler: zip ise bozar (scramble), degilse geri
 * getirmeyi (restore) dener. output_path / output_path_size, uretilen
 * yeni dosyanin tam yolunu almak icin kullanicinin sagladigi tampondur.
 * was_zip_out, dosyanin islenmeden once zip olarak taninip
 * taninmadigini bildirir (mesaj yazdirmak icin kullanislidir).
 * Donus: basarili olursa 0, hata olursa -1.
 */
static int process_file(const char *input_path, char *output_path,
                         size_t output_path_size, int *was_zip_out) {
    if (!input_path || !output_path || output_path_size == 0) {
        return -1;
    }

    int was_zip = is_zip_file(input_path);
    if (was_zip_out) *was_zip_out = was_zip;

    if (was_zip) {
        build_output_path(input_path, "_scrambled", output_path,
                           output_path_size);
        return rotate_stream_left(input_path, output_path,
                                   ROTATE_SHIFT_BITS);
    } else {
        build_output_path(input_path, "_restored", output_path,
                           output_path_size);
        return rotate_stream_right(input_path, output_path,
                                    ROTATE_SHIFT_BITS);
    }
}

/* ============================================================ */
/*  Konsol arayuzu                                               */
/* ============================================================ */

/*
 * Terminale surukle-birak ile gelen (veya elle yazilan) bir dosya
 * yolunu temizler:
 *  - Sondaki \r / \n karakterlerini atar.
 *  - Bastaki/sondaki bosluklari atar.
 *  - Bastan sona tek bir tirnak cifti ile sarilmissa (" veya ') tirnaklari
 *    kaldirir (bircok terminal, bosluklu yollari boyle tirnaklar).
 *  - "\ " (ters egik cizgi + bosluk) dizilerini tek bir bosluga
 *    cevirir (bash tabanli terminallerin bosluklu yollari kacirma
 *    - escape etme - sekli). Normal Windows yollarindaki ters egik
 *    cizgiler ("C:\Users\...") bundan etkilenmez, cunku onlardan
 *    sonra bosluk degil harf gelir.
 */
static void sanitize_path(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }

    size_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') start++;
    size_t end = len;
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) end--;
    size_t newlen = end - start;
    memmove(s, s + start, newlen);
    s[newlen] = '\0';
    len = newlen;

    if (len >= 2) {
        char first = s[0], last = s[len - 1];
        if ((first == '"' && last == '"') ||
            (first == '\'' && last == '\'')) {
            memmove(s, s + 1, len - 2);
            s[len - 2] = '\0';
            len -= 2;
        }
    }

    size_t r = 0, w = 0;
    while (s[r]) {
        if (s[r] == '\\' && s[r + 1] == ' ') {
            s[w++] = ' ';
            r += 2;
        } else {
            s[w++] = s[r++];
        }
    }
    s[w] = '\0';
}

static void print_banner(void) {
    printf("========================================\n");
    printf(" ZIP Baslik Araci - konsol surumu\n");
    printf("========================================\n");
    printf("Zip dosyasi        -> bitler kaydirilarak bozulur (_scrambled)\n");
    printf("Taninmayan dosya   -> ters kaydirma ile zip geri getirilir (_restored)\n");
    printf("Orijinal dosyaya dokunulmaz; sonuc her zaman yeni bir dosyadir.\n\n");
}

static void process_and_report(const char *raw_path) {
    char path[2048];
    snprintf(path, sizeof(path), "%s", raw_path);
    sanitize_path(path);

    if (path[0] == '\0') {
        return;
    }

    char output_path[2048];
    int was_zip = 0;
    if (process_file(path, output_path, sizeof(output_path), &was_zip) == 0) {
        if (was_zip) {
            printf("[OK]   %s\n       -> bitler kaydirilarak bozuldu\n       -> %s\n",
                   path, output_path);
        } else {
            printf("[OK]   %s\n       -> ters kaydirma ile geri getirildi\n       -> %s\n",
                   path, output_path);
        }
    } else {
        printf("[HATA] %s islenemedi (dosya bulunamadi/acilamadi ya da "
               "yazma izni yok).\n",
               path);
    }
}

int main(int argc, char *argv[]) {
    print_banner();

    if (argc > 1) {
        /* Program dogrudan dosya(lar) uzerinden baslatilmis
         * (Windows'ta .exe uzerine surukle-birak boyle calisir). */
        for (int i = 1; i < argc; i++) {
            process_and_report(argv[i]);
            printf("\n");
        }
        printf("Cikiliyor.\n");
        return 0;
    }

    printf("Islenecek dosyanin yolunu bu pencereye surukleyip birakin\n");
    printf("(veya elle yazin), sonra Enter'a basin.\n");
    printf("Cikmak icin bos satirda Enter'a basin.\n\n");

    char line[2048];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        char trimmed[2048];
        snprintf(trimmed, sizeof(trimmed), "%s", line);
        sanitize_path(trimmed);
        if (trimmed[0] == '\0') {
            break;
        }

        process_and_report(trimmed);
        printf("\n");
    }

    printf("Cikiliyor.\n");
    return 0;
}
