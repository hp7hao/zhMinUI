#ifndef __PERF_H__
#define __PERF_H__

#include "sdl.h"

///////////////////////////////////////
// Performance Counter Module (shared across all apps)

// Initialize performance counter
void PERF_init(void);

// Update FPS counter (call every frame)
void PERF_update(void);

// Get current FPS
float PERF_getFPS(void);

// Check if performance counter is enabled
int PERF_isEnabled(void);

// Draw performance counter on screen
void PERF_draw(SDL_Surface* screen, TTF_Font* font);

#endif // __PERF_H__

