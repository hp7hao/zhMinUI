#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defines.h"
#include "theme.h"
#include "config.h"
#include "i18n.h"

///////////////////////////////////////
// Theme System Module

// Define all themes
// Theme definitions with both dark and light variants
typedef struct ThemeVariant {
    const char* name;
    SDL_Color dark_foreground;
    SDL_Color dark_background;
    SDL_Color dark_accent;
    SDL_Color light_foreground;
    SDL_Color light_background;
    SDL_Color light_accent;
} ThemeVariant;

static const ThemeVariant theme_variants[] = {
    // Default theme
    {"Default", 
        {0xff, 0xff, 0xff, 255}, {0x00, 0x00, 0x00, 255}, {0x99, 0x99, 0x99, 255},  // Dark
        {0x00, 0x00, 0x00, 255}, {0xf5, 0xf5, 0xf5, 255}, {0x66, 0x66, 0x66, 255}}, // Light - 降低背景亮度
    
    // Blue theme
    {"Blue", 
        {0x66, 0x99, 0xff, 255}, {0x00, 0x33, 0x66, 255}, {0x33, 0x66, 0x99, 255},  // Dark
        {0x00, 0x33, 0x66, 255}, {0xe0, 0xed, 0xff, 255}, {0x66, 0x99, 0xcc, 255}}, // Light - 降低背景亮度
    
    // Green theme
    {"Green", 
        {0x66, 0xff, 0x66, 255}, {0x00, 0x33, 0x00, 255}, {0x33, 0x99, 0x33, 255},  // Dark
        {0x00, 0x66, 0x00, 255}, {0xe8, 0xf8, 0xe8, 255}, {0x66, 0xcc, 0x66, 255}}, // Light - 降低背景亮度
    
    // Purple theme
    {"Purple", 
        {0xcc, 0x66, 0xff, 255}, {0x33, 0x00, 0x66, 255}, {0x66, 0x33, 0x99, 255},  // Dark
        {0x66, 0x00, 0x99, 255}, {0xf0, 0xe8, 0xff, 255}, {0x99, 0x66, 0xcc, 255}}, // Light - 降低背景亮度
    
    // Orange theme
    {"Orange", 
        {0xff, 0x99, 0x66, 255}, {0x66, 0x33, 0x00, 255}, {0x99, 0x66, 0x33, 255},  // Dark
        {0xcc, 0x66, 0x00, 255}, {0xff, 0xf0, 0xe8, 255}, {0xff, 0x99, 0x66, 255}}, // Light - 降低背景亮度
    
    // Red theme
    {"Red", 
        {0xff, 0x66, 0x66, 255}, {0x66, 0x00, 0x00, 255}, {0x99, 0x33, 0x33, 255},  // Dark
        {0xcc, 0x00, 0x00, 255}, {0xff, 0xe8, 0xe8, 255}, {0xff, 0x66, 0x66, 255}}, // Light - 降低背景亮度
    
    // Cyan theme
    {"Cyan", 
        {0x66, 0xff, 0xff, 255}, {0x00, 0x66, 0x66, 255}, {0x33, 0x99, 0x99, 255},  // Dark
        {0x00, 0x99, 0x99, 255}, {0xe8, 0xff, 0xff, 255}, {0x66, 0xcc, 0xcc, 255}}, // Light - 降低背景亮度
    
    // Pink theme
    {"Pink", 
        {0xff, 0x66, 0xcc, 255}, {0x66, 0x00, 0x33, 255}, {0x99, 0x33, 0x66, 255},  // Dark
        {0xcc, 0x00, 0x66, 255}, {0xff, 0xe8, 0xf8, 255}, {0xff, 0x66, 0xcc, 255}}, // Light - 降低背景亮度
    
    // Yellow theme - 大幅调整对比度
    {"Yellow", 
        {0xff, 0xff, 0x66, 255}, {0x66, 0x66, 0x00, 255}, {0x99, 0x99, 0x33, 255},  // Dark
        {0x66, 0x66, 0x00, 255}, {0xf8, 0xf8, 0xe0, 255}, {0xcc, 0xcc, 0x00, 255}}  // Light - 深色前景，浅色背景
};

#define THEME_COUNT (sizeof(theme_variants) / sizeof(theme_variants[0]))


// Theme display options - original names for UI (will be translated at runtime)
static char* theme_display_options[] = {
    N_("Default"),
    N_("Blue"),
    N_("Green"),
    N_("Purple"),
    N_("Orange"),
    N_("Red"),
    N_("Cyan"),
    N_("Pink"),
    N_("Yellow"),
    NULL
};

// Theme config values - original names for config
static const char* theme_config_values[] = {
    "Default",
    "Blue",
    "Green",
    "Purple",
    "Orange",
    "Red",
    "Cyan",
    "Pink",
    "Yellow",
    NULL
};

// Get theme display options
const char** THEME_getDisplayOptions(void) {
    return theme_display_options;
}

// Get theme config values
const char** THEME_getConfigValues(void) {
    return theme_config_values;
}

// Get theme display count
int THEME_getDisplayCount(void) {
    return THEME_COUNT;
}

// Get theme display index by name
int THEME_getDisplayIndex(const char* theme_name) {
    for (int i = 0; i < THEME_COUNT; i++) {
        if (strcmp(theme_name, theme_config_values[i]) == 0) {
            return i;
        }
    }
    return 0; // Default to first theme
}

// Get theme display name by index
const char* THEME_getDisplayName(int index) {
    if (index >= 0 && index < THEME_COUNT) {
        return _(theme_display_options[index]);
    }
    return _(theme_display_options[0]); // Default to first theme
}


// Get current theme colors based on UI mode and background mode
SDL_Color THEME_getCurrentForeground(void) {
    const char* current_theme = CONFIG_getTheme();
    const char* ui_mode = CONFIG_getUIMode();
    
    // Find theme variant and return appropriate foreground color
    for (int i = 0; i < THEME_COUNT; i++) {
        if (strcmp(theme_variants[i].name, current_theme) == 0) {
            if (strcmp(ui_mode, "Light") == 0) {
                return theme_variants[i].light_foreground;
            } else {
                return theme_variants[i].dark_foreground;
            }
        }
    }
    
    // Return default theme foreground
    if (strcmp(ui_mode, "Light") == 0) {
        return theme_variants[0].light_foreground;
    } else {
        return theme_variants[0].dark_foreground;
    }
}

SDL_Color THEME_getCurrentBackground(void) {
    const char* current_theme = CONFIG_getTheme();
    const char* ui_mode = CONFIG_getUIMode();
    const char* background_mode = CONFIG_getBackgroundMode();
    
    // Background mode only affects background color
    if (strcmp(background_mode, "Monochrome") == 0) {
        if (strcmp(ui_mode, "Light") == 0) {
            return (SDL_Color){0xff, 0xff, 0xff, 255}; // White background
        } else {
            return (SDL_Color){0x00, 0x00, 0x00, 255}; // Black background
        }
    }
    
    // Otherwise use theme background color
    for (int i = 0; i < THEME_COUNT; i++) {
        if (strcmp(theme_variants[i].name, current_theme) == 0) {
            if (strcmp(ui_mode, "Light") == 0) {
                return theme_variants[i].light_background;
            } else {
                return theme_variants[i].dark_background;
            }
        }
    }
    
    // Return default theme background
    if (strcmp(ui_mode, "Light") == 0) {
        return theme_variants[0].light_background;
    } else {
        return theme_variants[0].dark_background;
    }
}

SDL_Color THEME_getCurrentAccent(void) {
    const char* current_theme = CONFIG_getTheme();
    const char* ui_mode = CONFIG_getUIMode();
    
    // Find theme variant and return appropriate accent color
    for (int i = 0; i < THEME_COUNT; i++) {
        if (strcmp(theme_variants[i].name, current_theme) == 0) {
            if (strcmp(ui_mode, "Light") == 0) {
                return theme_variants[i].light_accent;
            } else {
                return theme_variants[i].dark_accent;
            }
        }
    }
    
    // Return default theme accent
    if (strcmp(ui_mode, "Light") == 0) {
        return theme_variants[0].light_accent;
    } else {
        return theme_variants[0].dark_accent;
    }
}

///////////////////////////////////////
// UI Size System (shared across all apps)

// Pill/row height
int THEME_getUIPillSize(void) {
    const char* ui_size = CONFIG_getUISize();
    if (strcmp(ui_size, "Compact") == 0) return SCALE1(24);
    if (strcmp(ui_size, "Normal") == 0) return SCALE1(27);
    return SCALE1(30); // Big (default)
}

// Spacing between elements
int THEME_getUIPadding(void) {
    const char* ui_size = CONFIG_getUISize();
    if (strcmp(ui_size, "Compact") == 0) return SCALE1(6);
    if (strcmp(ui_size, "Normal") == 0) return SCALE1(8);
    return SCALE1(10); // Big (default)
}

// Padding inside buttons/pills
int THEME_getUIButtonPadding(void) {
    const char* ui_size = CONFIG_getUISize();
    if (strcmp(ui_size, "Compact") == 0) return SCALE1(8);
    if (strcmp(ui_size, "Normal") == 0) return SCALE1(10);
    return SCALE1(12); // Big (default)
}

// Number of rows visible on screen
// Formula: (FIXED_HEIGHT / pill_size) - 2 (for top padding + bottom button hints)
int THEME_getUIRowCount(void) {
    const char* ui_size = CONFIG_getUISize();
    if (strcmp(ui_size, "Compact") == 0) {
        // 480 / 48 - 2 = 10 - 2 = 8 rows
        return (FIXED_HEIGHT / SCALE1(24)) - 2;
    }
    if (strcmp(ui_size, "Normal") == 0) {
        // 480 / 54 - 2 = 8.88 - 2 = ~7 rows
        return (FIXED_HEIGHT / SCALE1(27)) - 2;
    }
    // Big: 480 / 60 - 2 = 8 - 2 = 6 rows
    return (FIXED_HEIGHT / SCALE1(30)) - 2;
}

// Button/menu item height (used in minarch)
int THEME_getUIButtonSize(void) {
    const char* ui_size = CONFIG_getUISize();
    if (strcmp(ui_size, "Compact") == 0) return SCALE1(16);
    if (strcmp(ui_size, "Normal") == 0) return SCALE1(18);
    return SCALE1(20); // Big (default)
}

// Font sizes (scaled based on UI size)
int THEME_getFontLargeSize(void) {
    const char* ui_size = CONFIG_getUISize();
    if (strcmp(ui_size, "Compact") == 0) return SCALE1(14);  // Smaller
    if (strcmp(ui_size, "Normal") == 0) return SCALE1(15);   // Medium
    return SCALE1(16); // Big (default)
}

int THEME_getFontMediumSize(void) {
    const char* ui_size = CONFIG_getUISize();
    if (strcmp(ui_size, "Compact") == 0) return SCALE1(12);
    if (strcmp(ui_size, "Normal") == 0) return SCALE1(13);
    return SCALE1(14); // Big (default)
}

int THEME_getFontSmallSize(void) {
    const char* ui_size = CONFIG_getUISize();
    if (strcmp(ui_size, "Compact") == 0) return SCALE1(10);
    if (strcmp(ui_size, "Normal") == 0) return SCALE1(11);
    return SCALE1(12); // Big (default)
}

int THEME_getFontTinySize(void) {
    const char* ui_size = CONFIG_getUISize();
    if (strcmp(ui_size, "Compact") == 0) return SCALE1(8);
    if (strcmp(ui_size, "Normal") == 0) return SCALE1(9);
    return SCALE1(10); // Big (default)
}
