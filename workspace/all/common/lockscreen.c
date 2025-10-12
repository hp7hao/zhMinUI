#include <stdio.h>
#include <string.h>
#include "defines.h"
#include "lockscreen.h"
#include "api.h"
#include "font.h"
#include "config.h"
#include "theme.h"

///////////////////////////////////////
// Lockscreen Module

static struct {
	int enabled;
} lockscreen;

void LOCKSCREEN_init(void) {
	// Check if lockscreen is enabled in config
	// For now, always enabled when screen timeout happens
	lockscreen.enabled = 1;
}

int LOCKSCREEN_isEnabled(void) {
	return lockscreen.enabled;
}

int LOCKSCREEN_show(SDL_Surface* screen) {
	if (!lockscreen.enabled) return 1; // Not enabled, treat as unlocked
	
	PAD_reset();
	GFX_clear(screen);
	
	// Get UI sizing
	int ui_padding = THEME_getUIPadding();
	
	// Draw lockscreen message
	char* lock_msg = "Screen Locked";
	char* unlock_msg = "Press A + B to unlock";
	
	SDL_Color text_color = {255, 255, 255, 255}; // White
	
	// Draw lock icon or message at center
	SDL_Surface* lock_surface = TTF_RenderUTF8_Blended(font.large, lock_msg, text_color);
	if (lock_surface) {
		int x = (screen->w - lock_surface->w) / 2;
		int y = (screen->h - lock_surface->h) / 2 - SCALE1(40);
		SDL_BlitSurface(lock_surface, NULL, screen, &(SDL_Rect){x, y});
		SDL_FreeSurface(lock_surface);
	}
	
	// Draw unlock instruction
	SDL_Surface* unlock_surface = TTF_RenderUTF8_Blended(font.medium, unlock_msg, text_color);
	if (unlock_surface) {
		int x = (screen->w - unlock_surface->w) / 2;
		int y = (screen->h + unlock_surface->h) / 2 + SCALE1(20);
		SDL_BlitSurface(unlock_surface, NULL, screen, &(SDL_Rect){x, y});
		SDL_FreeSurface(unlock_surface);
	}
	
	GFX_flip(screen);
	
	// Wait for A + B buttons to be pressed together (or timeout after 30 seconds)
	uint32_t last_input = SDL_GetTicks();
	
	while (1) {
		uint32_t now = SDL_GetTicks();
		
		PAD_poll();
		
		// Check if both A and B are pressed
		if (PAD_isPressed(BTN_A) && PAD_isPressed(BTN_B)) {
			// Both buttons pressed - unlock!
			
			// Visual feedback - flash screen
			GFX_clear(screen);
			SDL_Surface* unlocked_surface = TTF_RenderUTF8_Blended(font.large, "Unlocked!", text_color);
			if (unlocked_surface) {
				int x = (screen->w - unlocked_surface->w) / 2;
				int y = (screen->h - unlocked_surface->h) / 2;
				SDL_BlitSurface(unlocked_surface, NULL, screen, &(SDL_Rect){x, y});
				SDL_FreeSurface(unlocked_surface);
			}
			GFX_flip(screen);
			SDL_Delay(500); // Show "Unlocked!" for 500ms
			
			PAD_reset();
			return 1; // User unlocked successfully
		}
		
		// Check for any input to reset timeout
		if (PAD_anyPressed()) {
			last_input = now;
		}
		
		// Timeout after 1 minute (60 seconds) of no input on lockscreen
		if (now - last_input >= 60000) {
			PAD_reset();
			return 0; // Timed out, should proceed with sleep
		}
		
		GFX_sync();
		SDL_Delay(16); // ~60fps
	}
}

