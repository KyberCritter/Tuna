/* SPDX-License-Identifier: MIT */
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "Build-tool check failed at line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int hash_file(const char *path, char hex[65]) {
    FILE *f = fopen(path, "rb"); uint8_t buffer[4096], digest[32]; size_t n; TunaSha256 s;
    if (!f) return 1;
    tuna_sha256_init(&s);
    while ((n = fread(buffer, 1, sizeof(buffer), f))) tuna_sha256_update(&s, buffer, n);
    if (ferror(f)) { fclose(f); return 1; }
    if (fclose(f)) return 1;
    tuna_sha256_finish(&s, digest); tuna_sha256_hex(digest, hex); return 0;
}
static int memory_symbols(FILE *f) {
    char line[512], name[128], type; unsigned long address;
    int start = 0, end = 0, boundary = 0;
    static const char *const heap[] = {"malloc","_malloc_r","calloc","_calloc_r","realloc","_realloc_r","free","_free_r"};
    while (fgets(line, sizeof(line), f)) {
        unsigned i;
        if (sscanf(line, "%lx %c %127s", &address, &type, name) != 3) continue;
        for (i = 0; i < sizeof(heap)/sizeof(heap[0]); ++i) if (!strcmp(name, heap[i])) return 1;
        if (!strcmp(name, "__track_start")) start = address == 0x02010000;
        if (!strcmp(name, "__track_end")) end = address == 0x02040000;
        if (!strcmp(name, "boundary")) boundary = address >= 0x03000000 && address < 0x03008000;
    }
    return ferror(f) || !start || !end || !boundary;
}
static int read_image(const char *path, uint8_t data[65536], size_t *length) {
    FILE *f = fopen(path, "rb"); long n;
    if (!f) return 1;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 512 || n > 65536 || fseek(f, 0, SEEK_SET)) { fclose(f); return 1; }
    *length = (size_t)n;
    if (fread(data, 1, *length, f) != *length) { fclose(f); return 1; }
    return fclose(f);
}
/* Preserve timestamps for unchanged generated source, even when make rechecks it. */
static int publish(const char *temporary, const char *destination) {
    FILE *a = fopen(temporary, "rb"), *b = fopen(destination, "rb"); int same = b != NULL;
    if (!a) { if (b) fclose(b); return 1; }
    if (b) {
        int x, y;
        do { x = fgetc(a); y = fgetc(b); if (x != y) same = 0; } while (same && x != EOF);
        if (ferror(a) || ferror(b)) same = 0;
        fclose(b);
    }
    fclose(a);
    if (same) return remove(temporary);
    /* Windows rename cannot replace an existing file. These are explicit build outputs. */
    b = fopen(destination, "rb");
    if (b) { fclose(b); if (remove(destination)) return 1; }
    return rename(temporary, destination);
}
static int embed(const char *image, const char *source, const char *header) {
    static uint8_t data[65536]; size_t length, i; char ctmp[1024], htmp[1024]; FILE *c, *h; int bad = 0;
    if (read_image(image, data, &length)) return 1;
    if (snprintf(ctmp, sizeof(ctmp), "%s.tmp", source) >= (int)sizeof(ctmp) ||
        snprintf(htmp, sizeof(htmp), "%s.tmp", header) >= (int)sizeof(htmp)) return 1;
    c = fopen(ctmp, "wb"); if (!c) return 1;
    h = fopen(htmp, "wb"); if (!h) { fclose(c); remove(ctmp); return 1; }
    fprintf(h, "#ifndef PLAYER_IMAGE_H\n#define PLAYER_IMAGE_H\n#include <stddef.h>\n#include <stdint.h>\nextern const uint8_t player_image[];\nextern const size_t player_image_size;\n#endif\n");
    fprintf(c, "#include \"player_image.h\"\nconst uint8_t player_image[] = {\n");
    for (i = 0; i < length; ++i) { fprintf(c, "0x%02x,", data[i]); if (i % 16 == 15) fputc('\n', c); }
    fprintf(c, "\n};\nconst size_t player_image_size = sizeof(player_image);\n");
    bad = ferror(c) || ferror(h);
    if (fclose(c)) bad = 1;
    if (fclose(h)) bad = 1;
    if (bad) { remove(ctmp); remove(htmp); return 1; }
    if (publish(ctmp, source) || publish(htmp, header)) return 1;
    printf("Embedded %lu-byte GBA player.\n", (unsigned long)length); return 0;
}
static int selftest(void) {
    static const char *const input[] = {"", "abc", "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"};
    static const char *const expected[] = {
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"
    };
    unsigned i; uint8_t digest[32]; char hex[65]; TunaSha256 s; FILE *f;
    for (i = 0; i < 3; ++i) {
        size_t j; tuna_sha256_init(&s);
        for (j = 0; input[i][j]; ++j) tuna_sha256_update(&s, input[i] + j, 1);
        tuna_sha256_finish(&s, digest); tuna_sha256_hex(digest, hex); CHECK(!strcmp(hex, expected[i]));
    }
    tuna_sha256_init(&s);
    for (i = 0; i < 1000000; ++i) tuna_sha256_update(&s, "a", 1);
    tuna_sha256_finish(&s, digest); tuna_sha256_hex(digest, hex);
    CHECK(!strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
    f = tmpfile(); CHECK(f != NULL);
    CHECK(fputs("02010000 B __track_start\n02040000 B __track_end\n03000100 t boundary\n", f) >= 0);
    rewind(f); CHECK(!memory_symbols(f));
    CHECK(fseek(f, 0, SEEK_END) == 0); CHECK(fputs("02000100 T _malloc_r\n", f) >= 0);
    rewind(f); CHECK(memory_symbols(f)); CHECK(fclose(f) == 0);
    f = tmpfile(); CHECK(f != NULL);
    CHECK(fputs("02000000 B __track_start\n02040000 B __track_end\n02000100 t boundary\n", f) >= 0);
    rewind(f); CHECK(memory_symbols(f)); CHECK(fclose(f) == 0);
    printf("PASS: %u build-tool checks (SHA-256 golden vectors, memory/heap/IRQ gates).\n", checks); return 0;
}
int main(int argc, char **argv) {
    int i;
    if (argc == 2 && !strcmp(argv[1], "selftest")) return selftest();
    if (argc == 5 && !strcmp(argv[1], "embed")) return embed(argv[2], argv[3], argv[4]);
    if (argc == 4 && !strcmp(argv[1], "check-gba")) {
        static uint8_t data[65536]; size_t length; FILE *f; int bad;
        if (read_image(argv[2], data, &length)) { fprintf(stderr, "Invalid GBA image size/read.\n"); return 1; }
        f = fopen(argv[3], "rb"); if (!f) return 1;
        bad = memory_symbols(f); if (fclose(f)) bad = 1;
        if (bad) { fprintf(stderr, "GBA memory/heap/IWRAM IRQ validation failed.\n"); return 1; }
        puts("PASS: GBA resident size, track bounds, no heap symbols, IWRAM audio IRQ."); return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "sha256")) {
        for (i = 2; i < argc; ++i) {
            char hex[65]; if (hash_file(argv[i], hex)) { fprintf(stderr, "Cannot hash %s\n", argv[i]); return 1; }
            printf("%s  %s\n", hex, argv[i]);
        }
        return ferror(stdout) ? 1 : 0;
    }
    fprintf(stderr, "Usage: tuna-build selftest | embed image source.c header.h | check-gba image symbols.txt | sha256 files...\n");
    return 2;
}
