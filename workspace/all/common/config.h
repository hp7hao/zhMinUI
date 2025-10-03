#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "sdl.h"

///////////////////////////////////////
// MinUI Configuration Module

// Load configuration from file (called automatically by getters)
void CONFIG_load(void);

// Save configuration to file
void CONFIG_save(void);

// Getter functions
const char* CONFIG_getLanguage(void);        // "English" or "中文"
const char* CONFIG_getTheme(void);           // "Default", "Blue", "Green", "Purple", "Orange", "Red", "Cyan", "Pink", "Yellow", "Monochrome"

// Setter functions
void CONFIG_setLanguage(const char* value);
void CONFIG_setTheme(const char* value);

// Update multiple settings at once (for efficiency)
void CONFIG_update(const char* language, const char* theme);

// Get current theme colors
SDL_Color CONFIG_getThemeForeground(void);
SDL_Color CONFIG_getThemeBackground(void);
SDL_Color CONFIG_getThemeAccent(void);

#endif
