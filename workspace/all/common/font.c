#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include "defines.h"
#include "config.h"
#include "api.h"

///////////////////////////////////////
// Font Management Module

#define FONTS_DIR RES_PATH "/fonts"
#define MAX_FONTS 10
#define MAX_FONT_NAME 64

static char available_fonts[MAX_FONTS][MAX_FONT_NAME];
static int font_count = 0;
static char current_font[MAX_FONT_NAME] = "BoutiqueBitmap7x7_1.7.ttf";

// Scan fonts directory and populate available_fonts array
static void FONT_scanFonts(void) {
    font_count = 0;
    
    DIR* dir = opendir(FONTS_DIR);
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && font_count < MAX_FONTS) {
        if (entry->d_type == DT_REG) {
            const char* name = entry->d_name;
            // Check if it's a font file (ttf, ttc, otf)
            if (strstr(name, ".ttf") || strstr(name, ".ttc") || strstr(name, ".otf")) {
                strncpy(available_fonts[font_count], name, MAX_FONT_NAME - 1);
                available_fonts[font_count][MAX_FONT_NAME - 1] = '\0';
                font_count++;
            }
        }
    }
    closedir(dir);
}

int FONT_getCount(void) {
    if (font_count == 0) FONT_scanFonts();
    return font_count;
}

const char* FONT_getName(int index) {
    if (font_count == 0) FONT_scanFonts();
    if (index < 0 || index >= font_count) return NULL;
    return available_fonts[index];
}

const char* FONT_getCurrentFont(void) {
    return current_font;
}

void FONT_setCurrentFont(const char* font_name) {
    strncpy(current_font, font_name, MAX_FONT_NAME - 1);
    current_font[MAX_FONT_NAME - 1] = '\0';
}

const char* FONT_getCurrentFontPath(void) {
    static char font_path[256];
    snprintf(font_path, sizeof(font_path), "%s/%s", FONTS_DIR, current_font);
    return font_path;
}

void FONT_reloadFonts(void) {
    // Get current font from config
    const char* config_font = CONFIG_getFont();
    FONT_setCurrentFont(config_font);
    
    // Close existing fonts
    TTF_CloseFont(font.large);
    TTF_CloseFont(font.medium);
    TTF_CloseFont(font.small);
    TTF_CloseFont(font.tiny);
    
    // Load fonts with current font
    const char* font_path = FONT_getCurrentFontPath();
    font.large  = TTF_OpenFont(font_path, SCALE1(FONT_LARGE));
    font.medium = TTF_OpenFont(font_path, SCALE1(FONT_MEDIUM));
    font.small  = TTF_OpenFont(font_path, SCALE1(FONT_SMALL));
    font.tiny   = TTF_OpenFont(font_path, SCALE1(FONT_TINY));
    
    // Set font styles
    if (font.large) TTF_SetFontStyle(font.large, TTF_STYLE_NORMAL);
    if (font.medium) TTF_SetFontStyle(font.medium, TTF_STYLE_NORMAL);
    if (font.small) TTF_SetFontStyle(font.small, TTF_STYLE_NORMAL);
    if (font.tiny) TTF_SetFontStyle(font.tiny, TTF_STYLE_NORMAL);
}
