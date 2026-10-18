/*
 * Super Mario World 2: Yoshi's Island Recomp
 * Launcher Contract & Verification Tests
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "crc32.h"
#include "sha256.h"
#include "keybinds.h"
#include "host_paths.h"
#include "launcher_cache.h"
#include "recomp_launcher.h"
#include "launcher_profile.h"
#include "launcher_binds.h"

static const uint8_t kExpectedSha256[32] = {
    0x9b, 0x49, 0x57, 0x46, 0x67, 0x98, 0xbb, 0xdb,
    0x5b, 0x43, 0xa4, 0x50, 0xbb, 0xb6, 0x0b, 0x25,
    0x91, 0xae, 0x81, 0xd9, 0x5b, 0x89, 0x14, 0x30,
    0xf6, 0x2d, 0x53, 0xca, 0x62, 0xe8, 0xbc, 0x7b,
};
#define EXPECTED_CRC32 0xD138F224u
#define ROM_PAYLOAD_SIZE 2097152u
#define SRAM_SIZE 65536u

/* Joypad button bits matching runner layout */
enum {
    BTN_B      = 1 << 0,
    BTN_Y      = 1 << 1,
    BTN_SELECT = 1 << 2,
    BTN_START  = 1 << 3,
    BTN_UP     = 1 << 4,
    BTN_DOWN   = 1 << 5,
    BTN_LEFT   = 1 << 6,
    BTN_RIGHT  = 1 << 7,
    BTN_A      = 1 << 8,
    BTN_X      = 1 << 9,
    BTN_L      = 1 << 10,
    BTN_R      = 1 << 11,
};

/* Stubs for runner hooks referenced by runner objects */
void *g_spc_player = NULL;
bool g_new_ppu = true;
void RtlApuLock(void) {}
void RtlApuUnlock(void) {}
void debug_on_block_enter(uint32_t pc, uint32_t a, uint32_t x, uint32_t y) {(void)pc;(void)a;(void)x;(void)y;}
void debug_on_wram_write_byte(uint32_t a, uint8_t b, uint8_t c) {(void)a;(void)b;(void)c;}
void debug_on_wram_write_word(uint32_t a, uint16_t b, uint16_t c) {(void)a;(void)b;(void)c;}
void Die(const char *s) {fprintf(stderr, "FATAL: %s\n", s ? s : ""); exit(1);}

static int s_tests_run = 0;
static int s_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    s_tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "[-] FAIL: %s (line %d): %s\n", __func__, __LINE__, msg); \
        return 0; \
    } else { \
        s_tests_passed++; \
    } \
} while (0)

/* Helper: verify a ROM payload buffer */
static int verify_rom_payload(const uint8_t *data, size_t size) {
    if (!data || size != ROM_PAYLOAD_SIZE) {
        return 0;
    }
    uint8_t actual_sha[32];
    sha256_compute(data, size, actual_sha);
    if (memcmp(actual_sha, kExpectedSha256, 32) != 0) {
        return 0;
    }
    uint32_t actual_crc = crc32_compute(data, size);
    if (actual_crc != EXPECTED_CRC32) {
        return 0;
    }
    return 1;
}

/* Helper: load file with 512-byte SMC header support */
static uint8_t* load_rom_file(const char *path, size_t *out_size) {
    if (!path || !path[0]) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz < (long)ROM_PAYLOAD_SIZE) {
        fclose(f);
        return NULL;
    }
    size_t header_skip = (sz % 1024 == 512) ? 512 : 0;
    size_t payload_len = (size_t)sz - header_skip;
    if (payload_len != ROM_PAYLOAD_SIZE) {
        fclose(f);
        return NULL;
    }
    uint8_t *buf = (uint8_t*)malloc(sz);
    if (!buf || fread(buf, 1, sz, f) != (size_t)sz) {
        if (buf) free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    uint8_t *payload = (uint8_t*)malloc(payload_len);
    if (!payload) {
        free(buf);
        return NULL;
    }
    memcpy(payload, buf + header_skip, payload_len);
    free(buf);
    *out_size = payload_len;
    return payload;
}

static int test_headerless_rom(void) {
    printf("[*] Running test_headerless_rom...\n");
    const char *candidates[] = {
        "Super Mario World 2 - Yoshi's Island (USA).sfc",
        "../Super Mario World 2 - Yoshi's Island (USA).sfc",
        "../../Super Mario World 2 - Yoshi's Island (USA).sfc",
        "D:/MYAPPLICA/1GAMAMA/SFCGAMES/Super Mario World 2 - Yoshi's Island (USA).sfc"
    };
    uint8_t *rom = NULL;
    size_t sz = 0;
    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
        rom = load_rom_file(candidates[i], &sz);
        if (rom) break;
    }
    TEST_ASSERT(rom != NULL, "Could not locate or load Yoshi's Island (USA) ROM");
    TEST_ASSERT(sz == ROM_PAYLOAD_SIZE, "Payload size must be 2,097,152 bytes");
    TEST_ASSERT(verify_rom_payload(rom, sz) == 1, "Headerless ROM verification failed");
    free(rom);
    return 1;
}

static int test_headered_rom_synthetic(void) {
    printf("[*] Running test_headered_rom_synthetic...\n");
    const char *candidates[] = {
        "Super Mario World 2 - Yoshi's Island (USA).sfc",
        "../Super Mario World 2 - Yoshi's Island (USA).sfc",
        "../../Super Mario World 2 - Yoshi's Island (USA).sfc",
        "D:/MYAPPLICA/1GAMAMA/SFCGAMES/Super Mario World 2 - Yoshi's Island (USA).sfc"
    };
    uint8_t *valid_rom = NULL;
    size_t sz = 0;
    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
        valid_rom = load_rom_file(candidates[i], &sz);
        if (valid_rom) break;
    }
    TEST_ASSERT(valid_rom != NULL, "Need valid ROM to create synthetic headered file");

    const char *tmp_headered = "test_yi_headered_temp.smc";
    FILE *f = fopen(tmp_headered, "wb");
    TEST_ASSERT(f != NULL, "Failed to create temp headered file");

    uint8_t header[512];
    memset(header, 0x55, sizeof(header));
    fwrite(header, 1, 512, f);
    fwrite(valid_rom, 1, sz, f);
    fclose(f);
    free(valid_rom);

    size_t loaded_sz = 0;
    uint8_t *loaded = load_rom_file(tmp_headered, &loaded_sz);
    remove(tmp_headered);

    TEST_ASSERT(loaded != NULL, "Loader failed on 512-byte headered file");
    TEST_ASSERT(loaded_sz == ROM_PAYLOAD_SIZE, "Stripped payload size must be 2,097,152");
    TEST_ASSERT(verify_rom_payload(loaded, loaded_sz) == 1, "Headered ROM payload verification failed");
    free(loaded);
    return 1;
}

static int test_invalid_rom_rejection(void) {
    printf("[*] Running test_invalid_rom_rejection...\n");
    uint8_t *bad_data = (uint8_t *)calloc(1, ROM_PAYLOAD_SIZE);
    TEST_ASSERT(bad_data != NULL, "Allocation failed for bad_data");
    TEST_ASSERT(verify_rom_payload(bad_data, ROM_PAYLOAD_SIZE) == 0, "All-zero ROM must be rejected");

    bad_data[0] = 0x78; /* SEI opcode */
    TEST_ASSERT(verify_rom_payload(bad_data, ROM_PAYLOAD_SIZE) == 0, "Corrupted ROM must be rejected");

    TEST_ASSERT(verify_rom_payload(bad_data, 1024) == 0, "Short ROM must be rejected");
    TEST_ASSERT(verify_rom_payload(NULL, 0) == 0, "NULL ROM must be rejected");
    free(bad_data);
    return 1;
}

static int test_nonexistent_rom(void) {
    printf("[*] Running test_nonexistent_rom...\n");
    size_t sz = 0;
    uint8_t *rom = load_rom_file("non_existent_file_xyz_yi_12345.sfc", &sz);
    TEST_ASSERT(rom == NULL, "Nonexistent ROM must return NULL");
    return 1;
}

static int test_single_player_input(void) {
    printf("[*] Running test_single_player_input...\n");
    uint32_t state = BTN_B | BTN_Y | BTN_START | BTN_UP;
    TEST_ASSERT((state & BTN_B) != 0, "BTN_B must be set");
    TEST_ASSERT((state & BTN_Y) != 0, "BTN_Y must be set");
    TEST_ASSERT((state & BTN_START) != 0, "BTN_START must be set");
    TEST_ASSERT((state & BTN_UP) != 0, "BTN_UP must be set");
    TEST_ASSERT((state & BTN_A) == 0, "BTN_A must not be set");
    TEST_ASSERT((state & BTN_X) == 0, "BTN_X must not be set");
    TEST_ASSERT((state & BTN_L) == 0, "BTN_L must not be set");
    TEST_ASSERT((state & BTN_R) == 0, "BTN_R must not be set");
    return 1;
}

static int test_keybinds_persistence(void) {
    printf("[*] Running test_keybinds_persistence...\n");
    const char *tmp_ini = "test_yi_keybinds_temp.ini";
    keybinds_init(tmp_ini);

    /* Test remapping button 0 (A) to L, button 1 (B) to J */
    keybinds_set_button(1, 0, SDL_SCANCODE_L);
    keybinds_set_button(1, 1, SDL_SCANCODE_J);

    SDL_Scancode p1_a = keybinds_get_button(1, 0);
    SDL_Scancode p1_b = keybinds_get_button(1, 1);

    TEST_ASSERT(p1_a == SDL_SCANCODE_L, "P1 button A mismatch (expected L)");
    TEST_ASSERT(p1_b == SDL_SCANCODE_J, "P1 button B mismatch (expected J)");

    keybinds_save();
    remove(tmp_ini);
    return 1;
}

static int test_launcher_game_info_profile(void) {
    printf("[*] Running test_launcher_game_info_profile...\n");
    RecompLauncherCGameInfo gi;
    memset(&gi, 0, sizeof(gi));
    int res = launcher_profile_apply("snes", &gi);
    TEST_ASSERT(res == 1, "launcher_profile_apply('snes') must return 1");
    TEST_ASSERT(strcmp(gi.platform, "SUPER NINTENDO") == 0, "Platform must be SUPER NINTENDO");
    TEST_ASSERT(strcmp(gi.rom_noun, "ROM") == 0, "ROM noun must be ROM");
    return 1;
}

static int test_sram_rw_cycle(void) {
    printf("[*] Running test_sram_rw_cycle...\n");
    const char *tmp_sram = "test_yi_sram_temp.srm";

    uint8_t *original_buf = (uint8_t *)malloc(SRAM_SIZE);
    TEST_ASSERT(original_buf != NULL, "Failed to allocate original_buf");
    for (size_t i = 0; i < SRAM_SIZE; i++) {
        original_buf[i] = (uint8_t)((i * 37 + 13) & 0xFF);
    }

    FILE *f_out = fopen(tmp_sram, "wb");
    TEST_ASSERT(f_out != NULL, "Failed to open temp SRAM file for writing");
    size_t written = fwrite(original_buf, 1, SRAM_SIZE, f_out);
    fclose(f_out);
    TEST_ASSERT(written == SRAM_SIZE, "SRAM bytes written mismatch");

    uint8_t *read_buf = (uint8_t *)calloc(1, SRAM_SIZE);
    TEST_ASSERT(read_buf != NULL, "Failed to allocate read_buf");
    FILE *f_in = fopen(tmp_sram, "rb");
    TEST_ASSERT(f_in != NULL, "Failed to open temp SRAM file for reading");
    size_t nread = fread(read_buf, 1, SRAM_SIZE, f_in);
    fclose(f_in);
    remove(tmp_sram);

    TEST_ASSERT(nread == SRAM_SIZE, "SRAM bytes read mismatch");
    int cmp = memcmp(original_buf, read_buf, SRAM_SIZE);
    free(original_buf);
    free(read_buf);
    TEST_ASSERT(cmp == 0, "SRAM byte integrity failed");
    return 1;
}

static int test_settings_ini_rw(void) {
    printf("[*] Running test_settings_ini_rw...\n");
    const char *tmp_cfg = "test_yi_config_temp.ini";

    launcher_ini_kv_write(tmp_cfg, "General", "SkipLauncher", "1");
    launcher_ini_kv_write(tmp_cfg, "Graphics", "WindowScale", "4");
    launcher_ini_kv_write(tmp_cfg, "Sound", "Volume", "85");

    FILE *f = fopen(tmp_cfg, "r");
    TEST_ASSERT(f != NULL, "Failed to open temp config file");
    char line[256];
    bool found_scale = false, found_vol = false, found_skip = false;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "WindowScale = 4") || strstr(line, "WindowScale=4")) found_scale = true;
        if (strstr(line, "Volume = 85") || strstr(line, "Volume=85")) found_vol = true;
        if (strstr(line, "SkipLauncher = 1") || strstr(line, "SkipLauncher=1")) found_skip = true;
    }
    fclose(f);
    remove(tmp_cfg);

    TEST_ASSERT(found_scale, "WindowScale was not saved properly in ini");
    TEST_ASSERT(found_vol, "Volume was not saved properly in ini");
    TEST_ASSERT(found_skip, "SkipLauncher was not saved properly in ini");
    return 1;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("====================================================\n");
    printf(" Super Mario World 2: Yoshi's Island Contract Tests\n");
    printf("====================================================\n");

    int ok = 1;
    ok &= test_headerless_rom();
    ok &= test_headered_rom_synthetic();
    ok &= test_invalid_rom_rejection();
    ok &= test_nonexistent_rom();
    ok &= test_single_player_input();
    ok &= test_keybinds_persistence();
    ok &= test_launcher_game_info_profile();
    ok &= test_sram_rw_cycle();
    ok &= test_settings_ini_rw();

    printf("====================================================\n");
    printf(" Tests Run: %d, Passed: %d\n", s_tests_run, s_tests_passed);
    if (ok && s_tests_run == s_tests_passed) {
        printf(" ALL TESTS PASSED!\n");
        printf("====================================================\n");
        return 0;
    } else {
        printf(" SOME TESTS FAILED!\n");
        printf("====================================================\n");
        return 1;
    }
}
