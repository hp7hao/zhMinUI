#ifndef __LOCKSCREEN_H__
#define __LOCKSCREEN_H__

#include "sdl.h"

///////////////////////////////////////
// Lockscreen Module
// 
// Displays a lockscreen after waking from sleep
// Requires button combination to unlock
///////////////////////////////////////

// Initialize lockscreen (call at app startup)
void LOCKSCREEN_init(void);

// Activate lockscreen (call when timeout or manual sleep)
void LOCKSCREEN_activate(void);

// Update lockscreen state (call in main loop every frame)
// Returns 1 if should sleep (timed out), 0 otherwise
int LOCKSCREEN_update(void);

// Draw lockscreen (call in main loop if active)
void LOCKSCREEN_draw(SDL_Surface* screen);

// Check if lockscreen is currently active
int LOCKSCREEN_isActive(void);

// Check if input should be blocked (500ms grace period after unlock)
int LOCKSCREEN_isInputBlocked(void);

// Draw static lockscreen to buffer (for pre-sleep draw)
void LOCKSCREEN_drawStatic(SDL_Surface* screen);

// Check if lockscreen is enabled in config
int LOCKSCREEN_isEnabled(void);

#endif // __LOCKSCREEN_H__

