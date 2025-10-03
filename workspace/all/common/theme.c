#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "theme.h"
#include "config.h"

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

// Static array for theme names
static const char* theme_names[THEME_COUNT + 1];

// Initialize theme names array
static void init_theme_names(void) {
    static int initialized = 0;
    if (initialized) return;
    
    for (int i = 0; i < THEME_COUNT; i++) {
        theme_names[i] = theme_variants[i].name;
    }
    theme_names[THEME_COUNT] = NULL;
    initialized = 1;
}

// Get all available theme names
const char** THEME_getAllNames(void) {
    init_theme_names();
    return theme_names;
}

// Get theme count
int THEME_getCount(void) {
    return THEME_COUNT;
}

// Get theme by name (returns a Theme struct based on UI mode)
const Theme* THEME_getByName(const char* name) {
    static Theme current_theme;
    const char* ui_mode = CONFIG_getUIMode();
    
    for (int i = 0; i < THEME_COUNT; i++) {
        if (strcmp(theme_variants[i].name, name) == 0) {
            // Fill current_theme based on UI mode
            current_theme.name = theme_variants[i].name;
            if (strcmp(ui_mode, "Light") == 0) {
                current_theme.foreground = theme_variants[i].light_foreground;
                current_theme.background = theme_variants[i].light_background;
                current_theme.accent = theme_variants[i].light_accent;
            } else {
                current_theme.foreground = theme_variants[i].dark_foreground;
                current_theme.background = theme_variants[i].dark_background;
                current_theme.accent = theme_variants[i].dark_accent;
            }
            return &current_theme;
        }
    }
    
    // Return default theme if not found
    current_theme.name = theme_variants[0].name;
    if (strcmp(ui_mode, "Light") == 0) {
        current_theme.foreground = theme_variants[0].light_foreground;
        current_theme.background = theme_variants[0].light_background;
        current_theme.accent = theme_variants[0].light_accent;
    } else {
        current_theme.foreground = theme_variants[0].dark_foreground;
        current_theme.background = theme_variants[0].dark_background;
        current_theme.accent = theme_variants[0].dark_accent;
    }
    return &current_theme;
}

// Get theme by index
const Theme* THEME_getByIndex(int index) {
    if (index >= 0 && index < THEME_COUNT) {
        return THEME_getByName(theme_variants[index].name);
    }
    return THEME_getByName(theme_variants[0].name); // Return default theme if invalid index
}

// Get theme index by name
int THEME_getIndexByName(const char* name) {
    for (int i = 0; i < THEME_COUNT; i++) {
        if (strcmp(theme_variants[i].name, name) == 0) {
            return i;
        }
    }
    return 0; // Return default theme index if not found
}

// Get theme name by index
const char* THEME_getNameByIndex(int index) {
    if (index >= 0 && index < THEME_COUNT) {
        return theme_variants[index].name;
    }
    return theme_variants[0].name; // Return default theme name if invalid index
}

// Get current theme colors
SDL_Color THEME_getForeground(const char* theme_name) {
    const Theme* theme = THEME_getByName(theme_name);
    return theme->foreground;
}

SDL_Color THEME_getBackground(const char* theme_name) {
    const Theme* theme = THEME_getByName(theme_name);
    return theme->background;
}

SDL_Color THEME_getAccent(const char* theme_name) {
    const Theme* theme = THEME_getByName(theme_name);
    return theme->accent;
}

// This function is no longer needed as THEME_getByName now handles UI mode internally

// Get current theme colors based on UI mode and background mode
SDL_Color THEME_getCurrentForeground(void) {
    const char* current_theme = CONFIG_getTheme();
    
    // Foreground color is always based on UI mode and theme, not affected by background mode
    return THEME_getForeground(current_theme);
}

SDL_Color THEME_getCurrentBackground(void) {
    const char* current_theme = CONFIG_getTheme();
    const char* ui_mode = CONFIG_getUIMode();
    const char* background_mode = CONFIG_getBackgroundMode();
    
    // Background mode only affects background color
    if (strcmp(background_mode, "黑白") == 0) {
        if (strcmp(ui_mode, "Light") == 0) {
            return (SDL_Color){0xff, 0xff, 0xff, 255}; // White background
        } else {
            return (SDL_Color){0x00, 0x00, 0x00, 255}; // Black background
        }
    }
    
    // Otherwise use theme background color
    return THEME_getBackground(current_theme);
}

SDL_Color THEME_getCurrentAccent(void) {
    const char* current_theme = CONFIG_getTheme();
    
    // Accent color is always based on UI mode and theme, not affected by background mode
    return THEME_getAccent(current_theme);
}
