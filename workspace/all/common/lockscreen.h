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

// Show lockscreen and wait for unlock
// Returns 1 if user unlocked, 0 if timed out (should sleep)
int LOCKSCREEN_show(SDL_Surface* screen);

// Check if lockscreen is enabled in config
int LOCKSCREEN_isEnabled(void);

#endif // __LOCKSCREEN_H__

