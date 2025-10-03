#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "defines.h"
#include "utils.h"
#include "theme.h"

///////////////////////////////////////
// MinUI Configuration Module

#define CONFIG_PATH SDCARD_PATH "/.userdata/shared/.minui.conf"

typedef struct MinUIConfig {
    char language[16];      // "English" or "中文"
    char theme[16];         // "Default", "Blue", "Green", "Purple", "Orange", "Red", "Cyan", "Pink", "Yellow", "Monochrome"
    char font[64];          // Font filename from res/fonts folder
} MinUIConfig;

static MinUIConfig config = {"English", "Default", "BoutiqueBitmap7x7_1.7.ttf"}; // Default values
static int config_loaded = 0;

void CONFIG_load(void) {
    if (config_loaded) return;
    
    if (exists(CONFIG_PATH)) {
        char config_content[512];
        getFile(CONFIG_PATH, config_content, 512);
        trimTrailingNewlines(config_content);
        
        // Parse language setting
        char* lang_line = strstr(config_content, "language=");
        if (lang_line) {
            char* lang_value = lang_line + 9;
            char* end = strchr(lang_value, '\n');
            if (end) *end = '\0';
            strncpy(config.language, lang_value, 15);
            config.language[15] = '\0';
        }
        
        // Parse theme setting
        char* theme_line = strstr(config_content, "theme=");
        if (theme_line) {
            char* theme_value = theme_line + 6;
            char* end = strchr(theme_value, '\n');
            if (end) *end = '\0';
            strncpy(config.theme, theme_value, 15);
            config.theme[15] = '\0';
        }
        
        // Parse font setting
        char* font_line = strstr(config_content, "font=");
        if (font_line) {
            char* font_value = font_line + 5;
            char* end = strchr(font_value, '\n');
            if (end) *end = '\0';
            strncpy(config.font, font_value, 63);
            config.font[63] = '\0';
        }
    }
    
    config_loaded = 1;
}

void CONFIG_save(void) {
    // Create directory if it doesn't exist
    system("mkdir -p /mnt/SDCARD/.userdata/shared");
    
    // Write all settings
    char config_content[512];
    sprintf(config_content, "language=%s\ntheme=%s\nfont=%s\n", 
        config.language, config.theme, config.font);
    putFile(CONFIG_PATH, config_content);
}

// Getter functions
const char* CONFIG_getLanguage(void) {
    CONFIG_load();
    return config.language;
}

const char* CONFIG_getTheme(void) {
    CONFIG_load();
    return config.theme;
}

const char* CONFIG_getFont(void) {
    CONFIG_load();
    return config.font;
}

// Setter functions
void CONFIG_setLanguage(const char* value) {
    CONFIG_load();
    strncpy(config.language, value, 15);
    config.language[15] = '\0';
    CONFIG_save();
}

void CONFIG_setTheme(const char* value) {
    CONFIG_load();
    strncpy(config.theme, value, 15);
    config.theme[15] = '\0';
    CONFIG_save();
}

void CONFIG_setFont(const char* value) {
    CONFIG_load();
    strncpy(config.font, value, 63);
    config.font[63] = '\0';
    CONFIG_save();
}

// Update multiple settings at once (for efficiency)
void CONFIG_update(const char* language, const char* theme, const char* font) {
    CONFIG_load();
    strncpy(config.language, language, 15);
    config.language[15] = '\0';
    strncpy(config.theme, theme, 15);
    config.theme[15] = '\0';
    strncpy(config.font, font, 63);
    config.font[63] = '\0';
    CONFIG_save();
}

// Get current theme colors as SDL_Color
SDL_Color CONFIG_getThemeForeground(void) {
    CONFIG_load();
    return THEME_getForeground(config.theme);
}

SDL_Color CONFIG_getThemeBackground(void) {
    CONFIG_load();
    return THEME_getBackground(config.theme);
}

SDL_Color CONFIG_getThemeAccent(void) {
    CONFIG_load();
    return THEME_getAccent(config.theme);
}
