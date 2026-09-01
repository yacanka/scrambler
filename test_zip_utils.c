#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/zip_utils.h"

/*
 * Bilerek assert() KULLANMIYORUZ: bu proje varsayilan olarak Release
 * modunda derlenir (bkz. CMakeLists.txt) ve Release derlemeleri
 * genellikle NDEBUG tanimlar, bu da assert() cagrilarini sessizce
 * hicbir seye donusturur. Kontroller her zaman calisan basit bir
 * CHECK makrosuyla yapilir.
 */
static int failures = 0;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "BASARISIZ (satir %d): %s\n", __LINE__,  \
                    (msg));                                           \
            failures++;                                               \
        }                                                              \
    } while (0)

static int write_file(const char *path, const unsigned char *data,
                       size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t written = len > 0 ? fwrite(data, 1, len, f) : 0;
    fclose(f);
    return written == len;
}

static long file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    return sz;
}

static int files_fully_equal(const char *a, const char *b) {
    FILE *fa = fopen(a, "rb");
    FILE *fb = fopen(b, "rb");
    if (!fa || !fb) {
        if (fa) fclose(fa);
        if (fb) fclose(fb);
        return 0;
    }
    int c1, c2, equal = 1;
    while (1) {
        c1 = fgetc(fa);
        c2 = fgetc(fb);
        if (c1 != c2) {
            equal = 0;
            break;
        }
        if (c1 == EOF) break;
    }
    fclose(fa);
    fclose(fb);
    return equal;
}

/* Bir zip dosyasi olusturur, bozar (scramble), sonra bozulani tekrar
 * araca vererek geri getirir (restore); orijinalle birebir ayni
 * cikip cikmadigini ve dosya boyutunun hic degismedigini dogrular. */
static void run_roundtrip_case(const char *label, const unsigned char *payload,
                                size_t payload_len) {
    char original[512], scrambled[512], restored[512];
    snprintf(original, sizeof(original), "/tmp/rt_%s_original.zip", label);

    unsigned char full[4 + 512];
    size_t total_len = 4 + payload_len;
    memcpy(full, ZIP_SIGNATURE, 4);
    if (payload_len > 0) memcpy(full + 4, payload, payload_len);

    if (!write_file(original, full, total_len)) {
        fprintf(stderr, "Kurulum basarisiz (%s): dosya yazilamadi.\n", label);
        failures++;
        return;
    }

    CHECK(is_zip_file(original) == 1, "olusturulan dosya zip olarak taninmali");

    CHECK(process_dropped_file(original, scrambled, sizeof(scrambled)) == 0,
          "zip dosyasi bozulabilmeli (scramble)");
    CHECK(strstr(scrambled, "_scrambled") != NULL,
          "cikti adinda _scrambled olmali");
    CHECK(file_size(scrambled) == (long)total_len,
          "bozulmus dosyanin boyutu degismemis olmali");
    CHECK(is_zip_file(scrambled) == 0,
          "bozulmus dosya artik zip olarak taninmamali");
    CHECK(file_size(original) == (long)total_len,
          "orijinal dosya degismemis olmali (scramble sonrasi)");

    CHECK(process_dropped_file(scrambled, restored, sizeof(restored)) == 0,
          "bozulmus dosya geri getirilebilmeli (restore)");
    CHECK(strstr(restored, "_restored") != NULL,
          "cikti adinda _restored olmali");
    CHECK(file_size(restored) == (long)total_len,
          "geri getirilen dosyanin boyutu degismemis olmali");
    CHECK(is_zip_file(restored) == 1,
          "geri getirilen dosya tekrar zip olarak taninmali");
    CHECK(files_fully_equal(original, restored),
          "geri getirilen dosya, orijinalle bayt bayt ayni olmali");
    CHECK(file_size(scrambled) == (long)total_len,
          "bozulmus (ara) dosya restore sirasinda degismemis olmali");
}

int main(void) {
    /* Var olmayan / zip olmayan dosyalarda temel davranis */
    CHECK(is_zip_file("/tmp/olmayan_dosya_xyz_demo") == 0,
          "var olmayan dosya zip sayilmamali");

    unsigned char not_zip_payload[] = {'H', 'E', 'L', 'L', 'O'};
    write_file("/tmp/rt_notzip.bin", not_zip_payload,
               sizeof(not_zip_payload));
    CHECK(is_zip_file("/tmp/rt_notzip.bin") == 0,
          "PK imzasiyla baslamayan dosya zip sayilmamali");

    /* Ana senaryo: kucuk, orta ve "zorlayici" bit desenli veriyle
     * tam gidis-donus (round-trip) testi. */
    unsigned char small_payload[] = {'H', 'I', '!'};
    run_roundtrip_case("small", small_payload, sizeof(small_payload));

    run_roundtrip_case("empty_payload", NULL, 0); /* sadece 4 baytlik imza */

    unsigned char edge_payload[] = {0x00, 0xFF, 0x01, 0xFE, 0x80, 0x7F,
                                     0xAA, 0x55, 0x00, 0xFF};
    run_roundtrip_case("edge_bits", edge_payload, sizeof(edge_payload));

    unsigned char larger_payload[300];
    for (size_t i = 0; i < sizeof(larger_payload); i++) {
        larger_payload[i] = (unsigned char)((i * 37 + 11) & 0xFF);
    }
    run_roundtrip_case("larger", larger_payload, sizeof(larger_payload));

    if (failures == 0) {
        printf("Tum testler basarili.\n");
        return 0;
    }
    fprintf(stderr, "%d test basarisiz.\n", failures);
    return 1;
}
