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

// UI Size System (shared across all apps)
int THEME_getUIPillSize(void);              // Scaled pill/row height based on ui_size
int THEME_getUIPadding(void);               // Scaled padding between elements
int THEME_getUIButtonPadding(void);         // Scaled padding inside buttons
int THEME_getUIButtonSize(void);            // Scaled button/menu item height (minarch)
int THEME_getUIRowCount(void);              // Number of rows visible on screen

// Font sizes (scaled based on UI size)
int THEME_getFontLargeSize(void);           // Large font size (menu items)
int THEME_getFontMediumSize(void);          // Medium font size (single char button labels)
int THEME_getFontSmallSize(void);           // Small font size (button hints)
int THEME_getFontTinySize(void);            // Tiny font size (multi char button labels)

#endif // __THEME_H__
