#include <gccore.h>
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
#include "link.h"
#include "player_image.h"

#define MAX_FILES 128
#define MAX_PATH 512
static char paths[MAX_FILES][MAX_PATH];
static unsigned file_count = 1, selected, port = 1, excerpt;
static int booted;
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
    PAD_ScanPads(); b = PAD_ButtonsDown(0);
    SI_DisablePolling(SI_CHAN_BIT(port));
#ifdef HW_RVL
    WPAD_ScanPads();
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
static void scan(const char *base) {
    DIR *dir; struct dirent *entry; char folder[MAX_PATH];
    if (snprintf(folder, sizeof(folder), "%s/music", base) >= (int)sizeof(folder)) return;
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
    file_count = 1; selected = 0;
    if (fatInitDefault()) {
#ifdef HW_RVL
        scan("sd:"); scan("usb:");
#else
        scan("sd:");
#endif
        if (file_count == 1) scan("");
    }
    if (file_count > 2) qsort(paths[1], file_count - 1, sizeof(paths[0]), compare);
}
static TunaError load_track(uint8_t **track, size_t *length) {
    const char *ext, *name; FILE *f; long size; TunaTrack info; TunaError e;
    if (!selected) return tuna_make_jingle(track, length);
    name = strrchr(paths[selected], '/'); name = name ? name + 1 : paths[selected];
    ext = strrchr(name, '.');
    if (ext && !strcasecmp(ext, ".mp3")) return tuna_convert_mp3(paths[selected], name, excerpt, track, length);
    f = fopen(paths[selected], "rb"); if (!f) return TUNA_IO;
    if (fseek(f, 0, SEEK_END) || (size = ftell(f)) < (long)TUNA_HEADER || size > (long)TUNA_CAPACITY || fseek(f, 0, SEEK_SET)) { fclose(f); return TUNA_SIZE; }
    *track = (uint8_t *)malloc((size_t)size); if (!*track) { fclose(f); return TUNA_IO; }
    if (fread(*track, 1, (size_t)size, f) != (size_t)size) { fclose(f); free(*track); *track = NULL; return TUNA_IO; }
    fclose(f); *length = (size_t)size; e = tuna_track_validate(*track, *length, &info);
    if (e) { free(*track); *track = NULL; } return e;
}
static int confirm(void) {
    printf("\nA: send/replace GBA RAM track; B: cancel\n");
    while (1) { unsigned b = buttons(); if (b & PAD_BUTTON_A) return 1; if (b & (PAD_BUTTON_B | PAD_BUTTON_START)) return 0; VIDEO_WaitVSync(); }
}
static void send_selected(void) {
    uint8_t *track = NULL; size_t length = 0; TunaTrack info; TunaError e;
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
                printf("Power GBA on without cartridge, cable in port %u.\nBooting resident player...\n", port + 1);
                e = link_boot(player_image, player_image_size); if (!e) booted = 1;
            }
            if (!e) e = link_send_track(track, length);
        }
    }
    if (e) {
        printf("ERROR: %s. No READY: do not unplug.\n", tuna_error(e));
        if (e == TUNA_SIZE) printf("Limit: five minutes / 16 MiB input. Choose a shorter track or explicit excerpt.\n");
        if (e == TUNA_TIMEOUT || e == TUNA_IO) { booted = 0; printf("Reconnect/reset GBA before retry.\n"); }
    }
    free(track); printf("B to return to browser.\n");
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
            printf("Up/down: browse  A: preview  B: rescan\nY / Wii -: toggle excerpt  Start/Home: exit\n\nPut MP3 or .tuna files in FAT32 /music.\nMax 128 entries including original jingle.\n");
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
    return 0;
}
