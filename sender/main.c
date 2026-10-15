#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <fat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif
#include "convert.h"
#include "track_store.h"
#include "link.h"
#include "player_image.h"

#define MAX_FILES 128
#define MAX_PATH 512
static char paths[MAX_FILES][MAX_PATH];
static unsigned file_count = 1, selected, port = 1, excerpt;
static int booted, a_held;
static char cache_directory[MAX_PATH], prepared_key[MAX_PATH];
static uint8_t *prepared_track;
static size_t prepared_length;
static void init_video(void) {
    GXRModeObj *mode; void *frame;
    VIDEO_Init(); mode = VIDEO_GetPreferredMode(NULL);
    frame = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
    VIDEO_Configure(mode); VIDEO_SetNextFramebuffer(frame); VIDEO_SetBlack(FALSE); VIDEO_Flush(); VIDEO_WaitVSync();
    if (mode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
    CON_InitEx(mode, 20, 24, mode->fbWidth - 40, mode->xfbHeight - 48);
    VIDEO_ClearFrameBuffer(mode, frame, COLOR_BLACK);
}
static unsigned buttons(void) {
    unsigned b;
    PAD_ScanPads(); b = PAD_ButtonsDown(0); a_held = !!(PAD_ButtonsHeld(0) & PAD_BUTTON_A);
    SI_DisablePolling(SI_CHAN_BIT(port));
#ifdef HW_RVL
    WPAD_ScanPads();
    if (WPAD_ButtonsHeld(0) & WPAD_BUTTON_A) a_held = 1;
    { unsigned w = WPAD_ButtonsDown(0);
      if (w & WPAD_BUTTON_A) b |= PAD_BUTTON_A;
      if (w & WPAD_BUTTON_B) b |= PAD_BUTTON_B;
      if (w & WPAD_BUTTON_UP) b |= PAD_BUTTON_UP;
      if (w & WPAD_BUTTON_DOWN) b |= PAD_BUTTON_DOWN;
      if (w & WPAD_BUTTON_PLUS) b |= PAD_BUTTON_X;
      if (w & WPAD_BUTTON_MINUS) b |= PAD_BUTTON_Y;
      if (w & WPAD_BUTTON_HOME) b |= PAD_BUTTON_START;
    }
#endif
    return b;
}
static int compare(const void *a, const void *b) { return strcasecmp((const char *)a, (const char *)b); }
static void scan(const char *base, const char *subdirectory) {
    DIR *dir; struct dirent *entry; char folder[MAX_PATH];
    if (snprintf(folder, sizeof(folder), "%s/%s", base, subdirectory) >= (int)sizeof(folder)) return;
    dir = opendir(folder); if (!dir) return;
    while ((entry = readdir(dir)) && file_count < MAX_FILES) {
        const char *ext = strrchr(entry->d_name, '.'); struct stat st; char path[MAX_PATH];
        if (!ext || (strcasecmp(ext, ".mp3") && strcasecmp(ext, ".tuna"))) continue;
        if (snprintf(path, sizeof(path), "%s/%s", folder, entry->d_name) >= (int)sizeof(path)) continue;
        if (stat(path, &st) || !S_ISREG(st.st_mode)) continue;
        strcpy(paths[file_count++], path);
    }
    closedir(dir);
}
static void rescan(void) {
    struct stat st;
    file_count = 1; selected = 0; cache_directory[0] = 0;
    if (fatInitDefault()) {
        if (!stat("sd:/", &st) && S_ISDIR(st.st_mode)) strcpy(cache_directory, "sd:/tuna");
#ifdef HW_RVL
        scan("sd:", "tuna"); scan("sd:", "music");
        scan("usb:", "tuna"); scan("usb:", "music");
#else
        /* Native GC libfat may expose SD Gecko as the default mount only. */
        if (!cache_directory[0]) strcpy(cache_directory, "/tuna");
        scan("sd:", "tuna"); scan("sd:", "music");
#endif
        if (file_count == 1) { scan("", "tuna"); scan("", "music"); }
    }
    if (file_count > 2) qsort(paths[1], file_count - 1, sizeof(paths[0]), compare);
}
static void save_conversion(const char *path, const uint8_t *track, size_t length) {
    struct stat st; TunaError e;
    if (!cache_directory[0]) { printf("WARNING: SD unavailable; conversion retained in RAM for retry.\n"); return; }
    if (!stat(path, &st)) return; /* Preserve existing/user-created files. */
    if (stat(cache_directory, &st) && mkdir(cache_directory, 0777)) e = TUNA_IO;
    else e = tuna_track_save(path, track, length);
    if (!e) printf("Saved conversion: %s\n", path);
    else printf("WARNING: cannot save to SD /tuna; conversion retained in RAM for retry.\n");
}
static TunaError load_track(uint8_t **track, size_t *length) {
    const char *ext, *name; TunaError e; char key[MAX_PATH], saved[MAX_PATH];
    uint8_t *data = NULL; size_t bytes = 0; int mp3;
    if (!selected) {
        strcpy(key, "embedded-jingle");
        if (prepared_track && !strcmp(key, prepared_key)) goto reuse;
        e = tuna_make_jingle(&data, &bytes); if (e) return e;
        goto prepared;
    }
    name = strrchr(paths[selected], '/'); name = name ? name + 1 : paths[selected];
    ext = strrchr(name, '.');
    mp3 = ext && !strcasecmp(ext, ".mp3");
    if (!mp3) {
        /* Re-read selected containers so replaced/corrupt files are validated. */
        strcpy(key, paths[selected]);
        e = tuna_track_load(paths[selected], &data, &bytes); if (e) return e;
        goto prepared;
    }
    e = tuna_cache_path(cache_directory[0] ? cache_directory : "/tuna", paths[selected], excerpt, key);
    if (e) return e;
    strcpy(saved, key);
    /* Sender RAM identity is independent of whether SD is mounted/writable. */
    strcpy(key, strrchr(saved, '/') + 1);
    if (prepared_track && !strcmp(key, prepared_key)) {
        printf("Using completed conversion from sender RAM.\n");
        save_conversion(saved, prepared_track, prepared_length); goto reuse;
    }
    if (cache_directory[0]) {
        struct stat st;
        if (!stat(saved, &st)) {
            e = tuna_track_load(saved, &data, &bytes);
            if (!e) { printf("Using saved conversion: %s\n", saved); goto prepared; }
            printf("WARNING: saved conversion is invalid; keeping it untouched.\n");
        }
    }
    printf("Converting MP3...\n");
    { uint64_t start = gettime();
      e = tuna_convert_mp3(paths[selected], name, excerpt, &data, &bytes);
      printf("MP3 conversion time: %.3f seconds.\n", (double)ticks_to_millisecs(gettime() - start) / 1000); }
    if (e) return e;
    save_conversion(saved, data, bytes);
prepared:
    free(prepared_track); prepared_track = data; prepared_length = bytes; strcpy(prepared_key, key);
reuse:
    *track = prepared_track; *length = prepared_length; return TUNA_OK;
}
static int confirm(void) {
    printf("\nA: send/replace GBA RAM track; B: cancel\n");
    /* Cache hits are fast: the preview-opening press must not also send. */
    do {
        unsigned b = buttons();
        if (b & (PAD_BUTTON_B | PAD_BUTTON_START)) return 0;
        if (!a_held) break;
        VIDEO_WaitVSync();
    } while (1);
    while (1) { unsigned b = buttons(); if (b & PAD_BUTTON_A) return 1; if (b & (PAD_BUTTON_B | PAD_BUTTON_START)) return 0; VIDEO_WaitVSync(); }
}
static void send_selected(void) {
    uint8_t *track = NULL; size_t length = 0; TunaTrack info; TunaError e;
    const char *stage = "conversion/file validation";
    printf("\x1b[2JConverting/validating; please wait...\n");
    e = load_track(&track, &length);
    if (!e) e = tuna_track_validate(track, length, &info);
    if (!e) {
        printf("%s\n%.3f seconds; %lu/%u bytes\n%s, %lu Hz mono.\n", info.title, (double)info.samples / info.rate, (unsigned long)length, TUNA_CAPACITY, tuna_profile_name(info.codec), (unsigned long)info.rate);
        if (info.codec != TUNA_IMA4) printf("LOW-FIDELITY PROFILE: RAM fit, not CD quality.\n");
        if (excerpt && selected && strstr(paths[selected], ".tuna") == NULL) printf("EXPLICIT EXCERPT enabled, actual duration shown above.\n");
        if (confirm()) {
            link_port(port);
            if (!booted) {
                stage = "GBA multiboot/application startup";
                printf("Power GBA on without cartridge, cable in port %u.\nBooting resident player...\n", port + 1);
                e = link_boot(player_image, player_image_size); if (!e) booted = 1;
            }
            if (!e) { stage = "GBA track transfer"; e = link_send_track(track, length); }
        }
    }
    if (e) {
        printf("ERROR during %s: %s. No READY: do not unplug.\n", stage, tuna_error(e));
        if (e == TUNA_SIZE) printf("Limit: five minutes / 16 MiB input. Choose a shorter track or explicit excerpt.\n");
        if (e == TUNA_TIMEOUT || e == TUNA_IO) { booted = 0; printf("Reconnect/reset GBA before retry.\n"); }
    }
    printf("B to return to browser. Finished conversion is kept for retry.\n");
    while (!(buttons() & PAD_BUTTON_B)) VIDEO_WaitVSync();
}
int main(int argc, char **argv) {
    int redraw = 1; (void)argc; (void)argv;
    init_video(); PAD_Init();
#ifdef HW_RVL
    WPAD_Init();
#endif
    link_port(port); strcpy(paths[0], "Original complete 4-second jingle (embedded)"); rescan();
    while (1) {
        unsigned b;
        if (redraw) {
            printf("\x1b[2JTUNA V1 - %s -> GBA RAM\n", 
#ifdef HW_RVL
                   "native Wii SD/USB"
#else
                   "native GameCube SD Gecko"
#endif
            );
            printf("%u/%u: %s\n\nGBA cable: port %u (X / Wii + changes)\n", selected + 1, file_count, paths[selected], port + 1);
            printf("Policy: %s\n", excerpt ? "EXPLICIT 30-second excerpt for MP3" : "AUTO compression - complete songs up to 5 minutes");
            printf("3-minute default target: 2-bit ADPCM at 4096 Hz.\n");
            printf("Up/down: browse  A: preview  B: rescan\nY / Wii -: toggle excerpt  Start/Home: exit\n\nBrowse FAT32 /music and /tuna.\nConversions saved to SD /tuna before sending.\nMax 128 entries including original jingle.\n");
            redraw = 0;
        }
        b = buttons();
        if (b & PAD_BUTTON_START) break;
        if (b & PAD_BUTTON_UP) { selected = selected ? selected - 1 : file_count - 1; redraw = 1; }
        if (b & PAD_BUTTON_DOWN) { selected = (selected + 1) % file_count; redraw = 1; }
        if (b & PAD_BUTTON_X) { port = port == 3 ? 1 : port + 1; booted = 0; link_port(port); redraw = 1; }
        if (b & PAD_BUTTON_Y) { excerpt = excerpt ? 0 : 30; redraw = 1; }
        if (b & PAD_BUTTON_B) { rescan(); redraw = 1; }
        if (b & PAD_BUTTON_A) { send_selected(); redraw = 1; }
        VIDEO_WaitVSync();
    }
#ifdef HW_RVL
    WPAD_Shutdown();
#endif
    free(prepared_track);
    return 0;
}
