#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "theme.h"

///////////////////////////////////////
// Theme System Module

// Define all themes
static const Theme themes[] = {
    {"Default", {0xff, 0xff, 0xff, 255}, {0x00, 0x00, 0x00, 255}, {0x99, 0x99, 0x99, 255}},
    {"Blue", {0x66, 0x99, 0xff, 255}, {0x00, 0x33, 0x66, 255}, {0x33, 0x66, 0x99, 255}},
    {"Green", {0x66, 0xff, 0x66, 255}, {0x00, 0x33, 0x00, 255}, {0x33, 0x99, 0x33, 255}},
    {"Purple", {0xcc, 0x66, 0xff, 255}, {0x33, 0x00, 0x66, 255}, {0x66, 0x33, 0x99, 255}},
    {"Orange", {0xff, 0x99, 0x66, 255}, {0x66, 0x33, 0x00, 255}, {0x99, 0x66, 0x33, 255}},
    {"Red", {0xff, 0x66, 0x66, 255}, {0x66, 0x00, 0x00, 255}, {0x99, 0x33, 0x33, 255}},
    {"Cyan", {0x66, 0xff, 0xff, 255}, {0x00, 0x66, 0x66, 255}, {0x33, 0x99, 0x99, 255}},
    {"Pink", {0xff, 0x66, 0xcc, 255}, {0x66, 0x00, 0x33, 255}, {0x99, 0x33, 0x66, 255}},
    {"Yellow", {0xff, 0xff, 0x66, 255}, {0x66, 0x66, 0x00, 255}, {0x99, 0x99, 0x33, 255}},
    {"Monochrome", {0xf0, 0xf0, 0xf0, 255}, {0x10, 0x10, 0x10, 255}, {0x80, 0x80, 0x80, 255}}
};

#define THEME_COUNT (sizeof(themes) / sizeof(themes[0]))

// Static array for theme names
static const char* theme_names[THEME_COUNT + 1];

// Initialize theme names array
static void init_theme_names(void) {
    static int initialized = 0;
    if (initialized) return;
    
    for (int i = 0; i < THEME_COUNT; i++) {
        theme_names[i] = themes[i].name;
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

// Get theme by name
const Theme* THEME_getByName(const char* name) {
    for (int i = 0; i < THEME_COUNT; i++) {
        if (strcmp(themes[i].name, name) == 0) {
            return &themes[i];
        }
    }
    return &themes[0]; // Return default theme if not found
}

// Get theme by index
const Theme* THEME_getByIndex(int index) {
    if (index >= 0 && index < THEME_COUNT) {
        return &themes[index];
    }
    return &themes[0]; // Return default theme if invalid index
}

// Get theme index by name
int THEME_getIndexByName(const char* name) {
    for (int i = 0; i < THEME_COUNT; i++) {
        if (strcmp(themes[i].name, name) == 0) {
            return i;
        }
    }
    return 0; // Return default theme index if not found
}

// Get theme name by index
const char* THEME_getNameByIndex(int index) {
    if (index >= 0 && index < THEME_COUNT) {
        return themes[index].name;
    }
    return themes[0].name; // Return default theme name if invalid index
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
