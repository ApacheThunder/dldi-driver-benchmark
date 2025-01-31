// SPDX-License-Identifier: MIT
//
// SPDX-FileContributor: Adrian "asie" Siekierka, 2023, 2024

#include <stdio.h>

#include <fat.h>
#include <nds.h>
#include <unistd.h>

#include <nds/arm9/dldi.h>

#include "font.h"

const char *pad_filename = "/benchmark_pad.bin";
#define PAD_FILE_SIZE (8*1024*1024)
uint8_t *io_buffer;
int io_buffer_size;
int io_read_offset = 0;
bool lookup_cache_enabled = true;
bool fat_initialized = false;

static char options[20][33];

static PrintConsole tpConsole;
static PrintConsole btConsole;

static int bg;
static int bgSub;

static inline uint32_t my_rand(void) {
    static uint32_t seed = 0;
    seed = seed * 0xFDB97531 + 0x2468ACE;
    return seed;
}

static inline uint32_t get_ticks(void) {
    return TIMER0_DATA | (TIMER1_DATA << 16);
}

static void create_pad_file(void) {
    FILE *file;
    uint32_t buffer[256];

    file = fopen(pad_filename, "rb");
    if (file != NULL) {
        fclose(file);
        return;
    }

    file = fopen(pad_filename, "wb");
    if (file == NULL) {
        printf("\x1b[41mCould not create pad file!\n");
        return;
    }
    printf("Creating pad file... ");
    for (int i = 0; i < PAD_FILE_SIZE; i += sizeof(buffer)) {
        for (int k = 0; k < sizeof(buffer) / 4; k++) {
            buffer[k] = my_rand();
        }
        if (fwrite(buffer, sizeof(buffer), 1, file) <= 0) {
            fclose(file);
            unlink(pad_filename);
            printf("\x1b[41mError!\n");
            return;
        }
    }
    printf("OK\n");
    fclose(file);
}

static void fat_init(void) {
    if (!fat_initialized) {
        if (!fatInitDefault()) {
            printf("\x1b[41mFAT init failed!\n");
            // Set fat_initialized anyway; the file open operations
            // will fail instead.
        }
        create_pad_file();
        fat_initialized = true;
    }
}

static void benchmark_read(bool sequential) {
    fat_init();
    FILE *file = fopen(pad_filename, "rb");
    if (file == NULL) {
        printf("\x1b[41mCould not open '%s'!\n", pad_filename);
        return;
    }
//    printf("%d\n",
#ifdef BLOCKSDS
    if (lookup_cache_enabled)
	fatInitLookupCacheFile(file, 65536);
#endif
    printf("        \x1b[46mTesting reads...\x1b[39m\n");

    char msg_buffer[33];
    int reads_count = 4;
    for (int curr_size = io_buffer_size; curr_size >= 512; curr_size >>= 1) {
        if (curr_size >= 1024*1024) {
            printf("  %3d MiB", curr_size >> 20);
        } else if (curr_size >= 1024) {
            printf("  %3d KiB", curr_size >> 10);
            reads_count <<= 1;
        } else {
            printf("  0.5 KiB");
        }

        uint32_t ticks_start = get_ticks();
        uint32_t reads = 0;
	if (sequential) {
            int pos = 0;
	    while (reads < reads_count) {
		if (pos == 0)
	            fseek(file, io_read_offset, SEEK_SET);
                if (fread(io_buffer, curr_size, 1, file) <= 0)
                    break;
		pos = (pos + curr_size) & 0x3FFFFF;
                reads++;
            }
	} else {
	    while (reads < reads_count) {
                fseek(file, ((my_rand() & (~0x1FF)) & 0x3FFFFF) + io_read_offset, SEEK_SET);
                if (fread(io_buffer, curr_size, 1, file) <= 0)
                    break;
                reads++;
            }
	}
        double read_kilobytes = (curr_size / 1024.0) * reads_count;
        uint32_t ticks_end = get_ticks();
	    uint32_t ticks_diff = ticks_end - ticks_start;
        double seconds_time = (ticks_diff) / (double)(BUS_CLOCK >> 8);
        double kilobytes_per_second = read_kilobytes / seconds_time;
        if (reads < reads_count) {
            sprintf(msg_buffer, "\x1b[41mERROR");
        } else if (kilobytes_per_second >= 1024.0) {
            sprintf(msg_buffer, "%.3f MB/s", kilobytes_per_second / 1024.0);
        } else {
            sprintf(msg_buffer, "%.3f KB/s", kilobytes_per_second);
        }

        printf("\x1b[32D\x1b[%dC\x1b[42m%s\x1b[39m\n", 32 - 2 - strlen(msg_buffer), msg_buffer);
        swiDelay(5000000);
    }

    fclose(file);
}

static void benchmark_write(bool sequential) {
    fat_init();
    FILE *file = fopen(pad_filename, "r+b");
    if (file == NULL) {
        printf("\x1b[41mCould not open '%s'!\n", pad_filename);
        return;
    }
#ifdef BLOCKSDS
    if (lookup_cache_enabled)
        fatInitLookupCacheFile(file, 65536);
#endif
    printf("        \x1b[46mTesting writes...\x1b[39m\n");

    char msg_buffer[33];
    int reads_count = 1024;
    for (int curr_size = 512; curr_size <= io_buffer_size; curr_size <<= 1) {
        if (curr_size > 1*1024*1024) {
            continue;
        } else if (curr_size >= 1024*1024) {
            printf("  %3d MiB", curr_size >> 20);
        } else if (curr_size >= 1024) {
            printf("  %3d KiB", curr_size >> 10);
            reads_count >>= 1;
        } else {
            printf("  0.5 KiB");
        }

        uint32_t ticks_start = get_ticks();
        uint32_t reads = 0;
        if (sequential) {
            int pos = 0;
	    while (reads < reads_count) {
		if (pos == 0)
	            fseek(file, io_read_offset, SEEK_SET);
                if (fwrite(io_buffer, curr_size, 1, file) <= 0)
                    break;
		pos = (pos + curr_size) & 0x1FFFFF;
                reads++;
            }
        } else {
            while (reads < reads_count) {
                fseek(file, ((my_rand() & (~0x1FF)) & 0x1FFFFF) + io_read_offset, SEEK_SET);
                if (fwrite(io_buffer, curr_size, 1, file) <= 0)
                    break;
                reads++;
            }
        }
        double read_kilobytes = (curr_size / 1024.0) * reads_count;
        uint32_t ticks_end = get_ticks();
	    uint32_t ticks_diff = ticks_end - ticks_start;
        double seconds_time = (ticks_diff) / (double)(BUS_CLOCK >> 8);
        double kilobytes_per_second = read_kilobytes / seconds_time;
        if (reads < reads_count) {
            sprintf(msg_buffer, "\x1b[41mERROR");
        } else if (kilobytes_per_second >= 1024.0) {
            sprintf(msg_buffer, "%.3f MB/s", kilobytes_per_second / 1024.0);
        } else {
            sprintf(msg_buffer, "%.3f KB/s", kilobytes_per_second);
        }

        printf("\x1b[32D\x1b[%dC\x1b[42m%s\x1b[39m\n", 32 - 2 - strlen(msg_buffer), msg_buffer);
        swiDelay(5000000);
    }

    fclose(file);
}

static bool run_menu(int options_count, int *selection) {
    int last_selection = -1;
    int max_option_width = 0;
    for (int i = 0; i < options_count; i++) {
        int option_width = strlen(options[i]);
        if (max_option_width < option_width) max_option_width = option_width;
    }
    int menu_left = ((30 - max_option_width) >> 1) - 1;

    while (1) {
        if (last_selection != *selection) {
            printf("\x1b[2J"); // Clear console
            for (int i = 0; i < options_count; i++) {
                printf("\x1b[%dC\x1b[46m%c\x1b[39m %s\n", menu_left, i == *selection ? '>' : ' ', options[i]);
            }
        }

        swiWaitForVBlank();
        scanKeys();
        uint32_t keys_down = keysDown();
        if(keys_down & KEY_A) return true;
        if(keys_down & (KEY_B | KEY_START)) return false;
        if(keys_down & KEY_UP) (*selection)--;
        if(keys_down & KEY_DOWN) (*selection)++;
        if((*selection) < 0) *selection = 0;
        if((*selection) >= options_count) *selection = options_count-1;
    }
}

static void press_start_to_continue(void) {
    printf("\x1b[39m\n");
    printf("Press START to continue\n");

    while (1) {
        swiWaitForVBlank();

        scanKeys();

        uint32_t keys_down = keysDown();
        if (keys_down & KEY_START)
            break;
    }
}


static void CustomConsoleInit() {
	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);
	vramSetBankA (VRAM_A_MAIN_BG);
	vramSetBankC (VRAM_C_SUB_BG);
	
	bg = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 1, 0);
	bgSub = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 1, 0);
		
	consoleInit(&btConsole, 3, BgType_Text4bpp, BgSize_T_256x256, 20, 0, false, false);
	consoleInit(&tpConsole, 3, BgType_Text4bpp, BgSize_T_256x256, 20, 0, true, false);
		
	ConsoleFont font;
	font.gfx = (u16*)fontTiles;
	font.pal = (u16*)fontPal;
	font.numChars = 95;
	font.numColors =  fontPalLen / 2;
	font.bpp = 4;
	font.asciiOffset = 32;
	font.convertSingleColor = true;
	consoleSetFont(&btConsole, &font);
	consoleSetFont(&tpConsole, &font);
			
	consoleSelect(&btConsole);
}

int main(int argc, char **argv) {
    CustomConsoleInit();
    consoleClear();
    // printf("\x1b[2J"); // Clear console

    //      01234567890123456789012345678901
    printf(" \x1b[43m*\x1b[39m DLDI driver benchmark v0.2 \x1b[43m*\x1b[39m\n");
    printf("\x1b[37m%s\x1b[39m\n\n", io_dldi_data->friendlyName);

    // set windows
    consoleSetWindow(&btConsole, 0, 3, 32, 25);
	consoleSetWindow(&tpConsole, 0, 3, 32, 25);

    if (argc <= 0 || argv[0][0] == 0) {
        printf("\x1b[41mCould not find argv!\n");
        goto exit;
    }

    io_buffer_size = 2*1024*1024;
    io_buffer = malloc(io_buffer_size);
    if (io_buffer == NULL) {
        printf("\x1b[41mOut of memory!\n");
        goto exit;
    }

    TIMER0_DATA = 0;
    TIMER1_DATA = 0;
    TIMER0_CR = TIMER_ENABLE | TIMER_DIV_256;
    TIMER1_CR = TIMER_ENABLE | TIMER_CASCADE;

#ifdef BLOCKSDS
    dldiSetMode(DLDI_MODE_ARM9);
#endif

    int options_count;
    int selection = -1;

    do {
        switch (selection) {
            case 0: printf("\x1b[2J"); benchmark_read(false); press_start_to_continue(); break;
            case 1: printf("\x1b[2J"); benchmark_write(false); press_start_to_continue(); break;
            case 2: printf("\x1b[2J"); benchmark_read(true); press_start_to_continue(); break;
            case 3: printf("\x1b[2J"); benchmark_write(true); press_start_to_continue(); break;
            case 4: REG_EXMEMCNT ^= (1 << 15); break;
            case 5:
                if (io_read_offset == 0) io_read_offset = 1;
                else if (io_read_offset >= 256) io_read_offset = 0;
                else io_read_offset <<= 1;
                break;
#ifdef BLOCKSDS
            case 6: lookup_cache_enabled = !lookup_cache_enabled; break;
            case 7: if (!fat_initialized) dldiSetMode(dldiGetMode() == DLDI_MODE_ARM7 ? DLDI_MODE_ARM9 : DLDI_MODE_ARM7); break;
#endif
        }

        snprintf(options[0], 33, "Bench. random reads");
        snprintf(options[1], 33, "Bench. random writes");
        snprintf(options[2], 33, "Bench. sequential reads");
        snprintf(options[3], 33, "Bench. sequential writes");
        snprintf(options[4], 33, "RAM priority: %s", (REG_EXMEMCNT & (1 << 15)) ? "ARM7" : "ARM9");
        snprintf(options[5], 33, "Byte offset: %d", io_read_offset);
#ifdef BLOCKSDS
        snprintf(options[6], 33, "Seek lookup cache: %s", lookup_cache_enabled ? "Yes" : "No");
        snprintf(options[7], 33, "DLDI CPU: %s", dldiGetMode() == DLDI_MODE_ARM7 ? "ARM7" : "ARM9");
        options_count = 8;
#else
        options_count = 6;
#endif
    } while (run_menu(options_count, &selection));

exit:
    printf("\x1b[39m\n");
    printf("Press START to exit to loader\n");

    while (1)
    {
        swiWaitForVBlank();

        scanKeys();

        uint32_t keys_down = keysDown();
        if (keys_down & KEY_START)
            break;
    }

    return 0;
}
