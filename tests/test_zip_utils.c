#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrambler/zip_utils.h"

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

static void remove_if_set(const char *path) {
    if (path && path[0]) remove(path);
}

/* Bir zip dosyasi olusturur, bozar (scramble), sonra bozulani tekrar
 * araca vererek geri getirir (restore); orijinalle birebir ayni
 * cikip cikmadigini ve dosya boyutunun hic degismedigini dogrular. */
static void run_roundtrip_case(const char *label, const unsigned char *payload,
                                size_t payload_len) {
    char original[512] = {0};
    char scrambled[512] = {0};
    char restored[512] = {0};
    snprintf(original, sizeof(original), "test_%s_original.zip", label);

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

    remove_if_set(original);
    remove_if_set(scrambled);
    remove_if_set(restored);
}

static void test_zip_signatures(void) {
    static const unsigned char signatures[][4] = {
        {0x50, 0x4B, 0x03, 0x04},
        {0x50, 0x4B, 0x05, 0x06},
        {0x50, 0x4B, 0x07, 0x08}
    };
    const char *path = "test_signature.zip";

    for (size_t i = 0; i < sizeof(signatures) / sizeof(signatures[0]); i++) {
        CHECK(write_file(path, signatures[i], sizeof(signatures[i])),
              "imza test dosyasi yazilabilmeli");
        CHECK(is_zip_file(path) == 1, "gecerli ZIP imzasi taninmali");
    }
    remove_if_set(path);
}

static void test_small_output_buffer(void) {
    const char *path = "test_small_buffer.zip";
    char output_path[5] = "eski";

    CHECK(write_file(path, ZIP_SIGNATURE, sizeof(ZIP_SIGNATURE)),
          "kucuk tampon test dosyasi yazilabilmeli");
    CHECK(process_dropped_file(path, output_path, sizeof(output_path)) == -1,
          "yetersiz cikti yolu tamponu hata dondurmeli");
    CHECK(output_path[0] == '\0', "hata halinde cikti yolu bos olmali");
    CHECK(is_zip_file(path) == 1,
          "yetersiz tampon orijinal dosyayi degistirmemeli");
    remove_if_set(path);
}

static void test_same_input_and_output_buffer(void) {
    char path[64] = "test_same_buffer.zip";

    CHECK(write_file(path, ZIP_SIGNATURE, sizeof(ZIP_SIGNATURE)),
          "ayni tampon test dosyasi yazilabilmeli");
    CHECK(process_dropped_file(path, path, sizeof(path)) == -1,
          "girdi ve cikti yolu ayni tamponu kullanamamali");
    CHECK(strcmp(path, "test_same_buffer.zip") == 0,
          "hata halinde girdi yolu tampondaki degerini korumali");
    CHECK(is_zip_file(path) == 1,
          "ayni tampon hatasi orijinal dosyayi degistirmemeli");
    remove_if_set(path);
}

int main(void) {
    /* Var olmayan / zip olmayan dosyalarda temel davranis */
    CHECK(is_zip_file(NULL) == 0, "NULL yol zip sayilmamali");
    CHECK(is_zip_file("olmayan_dosya_xyz_demo") == 0,
          "var olmayan dosya zip sayilmamali");

    unsigned char not_zip_payload[] = {'H', 'E', 'L', 'L', 'O'};
    write_file("test_notzip.bin", not_zip_payload,
               sizeof(not_zip_payload));
    CHECK(is_zip_file("test_notzip.bin") == 0,
          "PK imzasiyla baslamayan dosya zip sayilmamali");
    remove_if_set("test_notzip.bin");

    test_zip_signatures();
    test_small_output_buffer();
    test_same_input_and_output_buffer();

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
