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
    char ui_mode[16];       // "Dark" or "Light"
    char background_mode[16]; // "Monochrome" or "Theme"
    char ui_size[16];       // "Big", "Normal", "Compact"
    int show_perf;          // 0 or 1 (show performance counter)
} MinUIConfig;

static MinUIConfig config = {"en_US", "Default", "BoutiqueBitmap7x7_1.7.ttf", "Dark", "Monochrome", "Big", 1}; // Default values (show_perf=1 for testing)
static int config_loaded = 0;

void CONFIG_load(void) {
    if (config_loaded) return;
    
    printf("CONFIG_load: Checking for config file at %s\n", CONFIG_PATH);
    
    if (exists(CONFIG_PATH)) {
        printf("CONFIG_load: Config file exists, loading...\n");
        char config_content[512];
        memset(config_content, 0, 512); // Clear buffer first
        
        // Read file manually to ensure proper null termination
        FILE* file = fopen(CONFIG_PATH, "r");
        if (file) {
            size_t bytes_read = fread(config_content, 1, 511, file); // Leave room for null terminator
            config_content[bytes_read] = '\0'; // Ensure null termination
            fclose(file);
        } else {
            printf("CONFIG_load: Failed to open config file\n");
            return;
        }
        printf("CONFIG_load: Config content: %s", config_content);
        
        // Parse config file line by line
        char* line = strtok(config_content, "\n");
        while (line != NULL) {
            // Skip empty lines
            if (strlen(line) == 0) {
                line = strtok(NULL, "\n");
                continue;
            }
            
            // Find the = character
            char* equals_pos = strchr(line, '=');
            if (equals_pos != NULL) {
                // Split key and value
                *equals_pos = '\0'; // Terminate key string
                char* key = line;
                char* value = equals_pos + 1;
                
                // Strip whitespace from key and value
                // Trim leading whitespace from key
                while (*key == ' ' || *key == '\t') key++;
                // Trim trailing whitespace from key
                char* key_end = key + strlen(key) - 1;
                while (key_end > key && (*key_end == ' ' || *key_end == '\t' || *key_end == '\r')) {
                    *key_end = '\0';
                    key_end--;
                }
                
                // Trim leading whitespace from value
                while (*value == ' ' || *value == '\t') value++;
                // Trim trailing whitespace from value
                char* value_end = value + strlen(value) - 1;
                while (value_end > value && (*value_end == ' ' || *value_end == '\t' || *value_end == '\r')) {
                    *value_end = '\0';
                    value_end--;
                }
                
                printf("CONFIG_load: Found key='%s', value='%s'\n", key, value);
                
                // Set config values based on key
                if (strcmp(key, "language") == 0) {
                    strncpy(config.language, value, 15);
                    config.language[15] = '\0';
                } else if (strcmp(key, "theme") == 0) {
                    strncpy(config.theme, value, 15);
                    config.theme[15] = '\0';
                } else if (strcmp(key, "font") == 0) {
                    strncpy(config.font, value, 63);
                    config.font[63] = '\0';
                } else if (strcmp(key, "ui_mode") == 0) {
                    strncpy(config.ui_mode, value, 15);
                    config.ui_mode[15] = '\0';
                } else if (strcmp(key, "background_mode") == 0) {
                    strncpy(config.background_mode, value, 15);
                    config.background_mode[15] = '\0';
                } else if (strcmp(key, "ui_size") == 0) {
                    strncpy(config.ui_size, value, 15);
                    config.ui_size[15] = '\0';
                } else if (strcmp(key, "show_perf") == 0) {
                    config.show_perf = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
                }
            }
            
            line = strtok(NULL, "\n");
        }
    } else {
        printf("CONFIG_load: Config file does not exist, using defaults\n");
    }
    
    printf("CONFIG_load: Final config - language=%s, theme=%s, font=%s, ui_mode=%s, background_mode=%s\n", 
        config.language, config.theme, config.font, config.ui_mode, config.background_mode);
    
    config_loaded = 1;
}

void CONFIG_save(void) {
    // Create directory if it doesn't exist
    char dir_path[256];
    strncpy(dir_path, CONFIG_PATH, 255);
    dir_path[255] = '\0';
    
    // Find the last '/' to get directory path
    char* last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0'; // Remove filename, keep directory path
        char mkdir_cmd[512];
        sprintf(mkdir_cmd, "mkdir -p %s", dir_path);
        system(mkdir_cmd);
    }
    
    // Write all settings
    char config_content[512];
    sprintf(config_content, "language=%s\ntheme=%s\nfont=%s\nui_mode=%s\nbackground_mode=%s\nui_size=%s\nshow_perf=%d\n", 
        config.language, config.theme, config.font, config.ui_mode, config.background_mode, config.ui_size, config.show_perf);
    
    // Debug: Print config content to console
    printf("CONFIG_save: Writing to %s\n", CONFIG_PATH);
    printf("CONFIG_save: Content: %s", config_content);
    
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

const char* CONFIG_getUIMode(void) {
    CONFIG_load();
    return config.ui_mode;
}

const char* CONFIG_getBackgroundMode(void) {
    CONFIG_load();
    return config.background_mode;
}

int CONFIG_getShowPerf(void) {
    CONFIG_load();
    return config.show_perf;
}

const char* CONFIG_getUISize(void) {
    CONFIG_load();
    return config.ui_size;
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

void CONFIG_setUIMode(const char* value) {
    CONFIG_load();
    strncpy(config.ui_mode, value, 15);
    config.ui_mode[15] = '\0';
    CONFIG_save();
}

void CONFIG_setBackgroundMode(const char* value) {
    CONFIG_load();
    strncpy(config.background_mode, value, 15);
    config.background_mode[15] = '\0';
    CONFIG_save();
}

void CONFIG_setUISize(const char* value) {
    CONFIG_load();
    strncpy(config.ui_size, value, 15);
    config.ui_size[15] = '\0';
    CONFIG_save();
}

// Update multiple settings at once (for efficiency)
void CONFIG_update(const char* language, const char* theme, const char* font, const char* ui_mode, const char* background_mode) {
    CONFIG_load();
    strncpy(config.language, language, 15);
    config.language[15] = '\0';
    strncpy(config.theme, theme, 15);
    config.theme[15] = '\0';
    strncpy(config.font, font, 63);
    config.font[63] = '\0';
    strncpy(config.ui_mode, ui_mode, 15);
    config.ui_mode[15] = '\0';
    strncpy(config.background_mode, background_mode, 15);
    config.background_mode[15] = '\0';
    CONFIG_save();
}

// Get current theme colors as SDL_Color (now UI mode aware)
SDL_Color CONFIG_getThemeForeground(void) {
    CONFIG_load();
    return THEME_getCurrentForeground();
}

SDL_Color CONFIG_getThemeBackground(void) {
    CONFIG_load();
    return THEME_getCurrentBackground();
}

SDL_Color CONFIG_getThemeAccent(void) {
    CONFIG_load();
    return THEME_getCurrentAccent();
}
