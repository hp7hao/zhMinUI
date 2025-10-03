#ifndef __THEME_H__
#define __THEME_H__

#include "sdl.h"

///////////////////////////////////////
// Theme System Module

// Theme structure
typedef struct Theme {
    const char* name;
    SDL_Color foreground;
    SDL_Color background;
    SDL_Color accent;
} Theme;

// Theme options for UI
const char** THEME_getDisplayOptions(void);
const char** THEME_getConfigValues(void);
int THEME_getDisplayCount(void);
int THEME_getDisplayIndex(const char* theme_name);
const char* THEME_getDisplayName(int index);

// Get current theme colors based on UI mode
SDL_Color THEME_getCurrentForeground(void);
SDL_Color THEME_getCurrentBackground(void);
SDL_Color THEME_getCurrentAccent(void);

#endif // __THEME_H__
