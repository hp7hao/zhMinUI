#ifndef __FONT_H__
#define __FONT_H__

#include "sdl.h"

///////////////////////////////////////
// Font Management Module

// Get available fonts from res/fonts folder
int FONT_getCount(void);
const char* FONT_getName(int index);
const char* FONT_getCurrentFont(void);

// Set current font
void FONT_setCurrentFont(const char* font_name);

// Get font path for current font
const char* FONT_getCurrentFontPath(void);

// Reload fonts with current font setting
void FONT_reloadFonts(void);

#endif
