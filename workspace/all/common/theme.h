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

// Get all available theme names
const char** THEME_getAllNames(void);

// Get theme count
int THEME_getCount(void);

// Get theme by name
const Theme* THEME_getByName(const char* name);

// Get theme by index
const Theme* THEME_getByIndex(int index);

// Get theme index by name
int THEME_getIndexByName(const char* name);

// Get theme name by index
const char* THEME_getNameByIndex(int index);

// Get current theme colors
SDL_Color THEME_getForeground(const char* theme_name);
SDL_Color THEME_getBackground(const char* theme_name);
SDL_Color THEME_getAccent(const char* theme_name);

#endif // __THEME_H__
