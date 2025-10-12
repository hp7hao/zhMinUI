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

#define MAX_UNLOCK_BUTTONS 3

static struct {
	int enabled;                // Is lockscreen feature enabled?
	int active;                 // Is lockscreen currently showing?
	int waiting_for_release;    // Waiting for buttons to release after unlock?
	char* pressed_button_labels[MAX_UNLOCK_BUTTONS];  // Which buttons were pressed
	int button_count;           // How many buttons pressed (0-3)
	uint32_t last_input_time;   // Last input time (for 1-minute timeout)
	uint32_t unlock_time;       // When unlocked (for 500ms input blocking)
} lockscreen;

void LOCKSCREEN_init(void) {
	// Check if lockscreen is enabled in config
	// For now, always enabled when screen timeout happens
	lockscreen.enabled = 1;
	lockscreen.active = 0;
	lockscreen.waiting_for_release = 0;
	lockscreen.button_count = 0;
	lockscreen.last_input_time = 0;
	lockscreen.unlock_time = 0;
	for (int i = 0; i < MAX_UNLOCK_BUTTONS; i++) {
		lockscreen.pressed_button_labels[i] = NULL;
	}
}

int LOCKSCREEN_isEnabled(void) {
	return lockscreen.enabled;
}

void LOCKSCREEN_activate(void) {
	if (!lockscreen.enabled) return;
	
	lockscreen.active = 1;
	lockscreen.waiting_for_release = 0;
	lockscreen.button_count = 0;
	lockscreen.last_input_time = SDL_GetTicks();
	lockscreen.unlock_time = 0;
	for (int i = 0; i < MAX_UNLOCK_BUTTONS; i++) {
		lockscreen.pressed_button_labels[i] = NULL;
	}
	PAD_reset();
}

int LOCKSCREEN_isActive(void) {
	return lockscreen.active;
}

int LOCKSCREEN_isInputBlocked(void) {
	if (lockscreen.unlock_time == 0) return 0; // Never unlocked
	uint32_t now = SDL_GetTicks();
	if (now - lockscreen.unlock_time < 500) {
		return 1; // Still in 500ms grace period
	}
	return 0;
}

// Helper function to draw a filled circle (dot)
static void drawCircle(SDL_Surface* screen, int cx, int cy, int radius, SDL_Color color) {
	Uint32 pixel_color = SDL_MapRGB(screen->format, color.r, color.g, color.b);
	
	for (int y = -radius; y <= radius; y++) {
		for (int x = -radius; x <= radius; x++) {
			if (x*x + y*y <= radius*radius) {
				int px = cx + x;
				int py = cy + y;
				if (px >= 0 && px < screen->w && py >= 0 && py < screen->h) {
					Uint32* pixel = (Uint32*)((Uint8*)screen->pixels + py * screen->pitch + px * screen->format->BytesPerPixel);
					*pixel = pixel_color;
				}
			}
		}
	}
}

void LOCKSCREEN_drawStatic(SDL_Surface* screen) {
	if (!lockscreen.enabled) return;
	
	GFX_clear(screen);
	
	// Get theme colors
	SDL_Color theme_foreground = CONFIG_getThemeForeground();
	SDL_Color theme_accent = CONFIG_getThemeAccent();
	
	// Draw title at top
	SDL_Surface* lock_surface = TTF_RenderUTF8_Blended(font.large, "Screen Locked", theme_foreground);
	if (lock_surface) {
		int x = (screen->w - lock_surface->w) / 2;
		int y = SCALE1(60);
		SDL_BlitSurface(lock_surface, NULL, screen, &(SDL_Rect){x, y});
		SDL_FreeSurface(lock_surface);
	}
	
	// Draw instruction at bottom
	SDL_Surface* unlock_surface = TTF_RenderUTF8_Blended(font.medium, "Press any 3 buttons to unlock", theme_foreground);
	if (unlock_surface) {
		int x = (screen->w - unlock_surface->w) / 2;
		int y = screen->h - SCALE1(80);
		SDL_BlitSurface(unlock_surface, NULL, screen, &(SDL_Rect){x, y});
		SDL_FreeSurface(unlock_surface);
	}
	
	// Draw three empty dots in center (using theme accent color with reduced opacity)
	int ui_button_size = THEME_getUIButtonSize();
	int dot_spacing = ui_button_size + SCALE1(20);
	int start_x = (screen->w - (dot_spacing * 2)) / 2;
	int dot_y = screen->h / 2;
	int dot_radius = SCALE1(12);
	
	SDL_Color dot_color = {theme_accent.r / 3, theme_accent.g / 3, theme_accent.b / 3, 255}; // Darker accent color
	
	for (int i = 0; i < 3; i++) {
		int x = start_x + (i * dot_spacing);
		drawCircle(screen, x, dot_y, dot_radius, dot_color);
	}
	
	GFX_flip(screen);
}

// Update lockscreen logic (non-blocking, call every frame)
int LOCKSCREEN_update(void) {
	if (!lockscreen.active) return 0;
	
	uint32_t now = SDL_GetTicks();
	
	// If waiting for button release after unlock
	if (lockscreen.waiting_for_release) {
		if (!PAD_anyPressed()) {
			// All buttons released
			if (now - lockscreen.unlock_time >= 100) {
				// Grace period passed, unlock_time is already set for input blocking
				lockscreen.active = 0;
				lockscreen.waiting_for_release = 0;
				PAD_reset();
			}
		}
		return 0; // Don't sleep, just waiting
	}
	
	// Check for button presses
	int new_button = -1;
	char* button_label = NULL;
	if (PAD_justPressed(BTN_A)) { new_button = BTN_A; button_label = "A"; }
	else if (PAD_justPressed(BTN_B)) { new_button = BTN_B; button_label = "B"; }
	else if (PAD_justPressed(BTN_X)) { new_button = BTN_X; button_label = "X"; }
	else if (PAD_justPressed(BTN_Y)) { new_button = BTN_Y; button_label = "Y"; }
	else if (PAD_justPressed(BTN_UP)) { new_button = BTN_UP; button_label = "↑"; }
	else if (PAD_justPressed(BTN_DOWN)) { new_button = BTN_DOWN; button_label = "↓"; }
	else if (PAD_justPressed(BTN_LEFT)) { new_button = BTN_LEFT; button_label = "←"; }
	else if (PAD_justPressed(BTN_RIGHT)) { new_button = BTN_RIGHT; button_label = "→"; }
	else if (PAD_justPressed(BTN_START)) { new_button = BTN_START; button_label = "START"; }
	else if (PAD_justPressed(BTN_SELECT)) { new_button = BTN_SELECT; button_label = "SELECT"; }
	
	if (new_button != -1 && button_label != NULL && lockscreen.button_count < MAX_UNLOCK_BUTTONS) {
		// Add button to sequence
		lockscreen.pressed_button_labels[lockscreen.button_count] = button_label;
		lockscreen.button_count++;
		lockscreen.last_input_time = now;
		
		// Check if we have 3 buttons
		if (lockscreen.button_count >= MAX_UNLOCK_BUTTONS) {
			// Unlocked! Start waiting for button release
			lockscreen.unlock_time = now;
			lockscreen.waiting_for_release = 1;
		}
	}
	
	// Check for any input to reset timeout
	if (PAD_anyPressed()) {
		lockscreen.last_input_time = now;
	}
	
	// Timeout after 1 minute (60 seconds) of no input on lockscreen
	if (now - lockscreen.last_input_time >= 60000) {
		lockscreen.active = 0;
		PAD_reset();
		return 1; // Timed out, should sleep
	}
	
	return 0; // Continue showing lockscreen
}

// Draw lockscreen (non-blocking, call every frame)
void LOCKSCREEN_draw(SDL_Surface* screen) {
	if (!lockscreen.active) return;
	
	GFX_clear(screen);
	
	// Get UI sizing
	int ui_button_size = THEME_getUIButtonSize();
	
	// Get theme colors
	SDL_Color theme_foreground = CONFIG_getThemeForeground();
	SDL_Color theme_accent = CONFIG_getThemeAccent();
	
	char* lock_msg = "Screen Locked";
	char* unlock_msg = "Press any 3 buttons to unlock";
	
	// If just unlocked, show "Unlocked!" message
	if (lockscreen.waiting_for_release) {
		SDL_Surface* unlocked_surface = TTF_RenderUTF8_Blended(font.large, "Unlocked!", theme_accent);
		if (unlocked_surface) {
			int x = (screen->w - unlocked_surface->w) / 2;
			int y = (screen->h - unlocked_surface->h) / 2;
			SDL_BlitSurface(unlocked_surface, NULL, screen, &(SDL_Rect){x, y});
			SDL_FreeSurface(unlocked_surface);
		}
		return; // Don't draw rest of lockscreen
	}
	
	// Draw title
	SDL_Surface* lock_surface = TTF_RenderUTF8_Blended(font.large, lock_msg, theme_foreground);
	if (lock_surface) {
		int x = (screen->w - lock_surface->w) / 2;
		int y = SCALE1(60);
		SDL_BlitSurface(lock_surface, NULL, screen, &(SDL_Rect){x, y});
		SDL_FreeSurface(lock_surface);
	}
	
	// Draw instruction
	SDL_Surface* unlock_surface = TTF_RenderUTF8_Blended(font.medium, unlock_msg, theme_foreground);
	if (unlock_surface) {
		int x = (screen->w - unlock_surface->w) / 2;
		int y = screen->h - SCALE1(80);
		SDL_BlitSurface(unlock_surface, NULL, screen, &(SDL_Rect){x, y});
		SDL_FreeSurface(unlock_surface);
	}
	
	// Draw buttons/dots
	int dot_spacing = ui_button_size + SCALE1(20);
	int start_x = (screen->w - (dot_spacing * (MAX_UNLOCK_BUTTONS - 1))) / 2;
	int dot_y = screen->h / 2;
	int dot_radius = SCALE1(12);
	
	SDL_Color dot_color = {theme_accent.r / 3, theme_accent.g / 3, theme_accent.b / 3, 255};
	
	for (int i = 0; i < MAX_UNLOCK_BUTTONS; i++) {
		int x = start_x + (i * dot_spacing);
		if (lockscreen.pressed_button_labels[i] != NULL) {
			// Draw button with label as a pill
			SDL_Surface* label_surface = TTF_RenderUTF8_Blended(font.medium, lockscreen.pressed_button_labels[i], theme_foreground);
			if (label_surface) {
				// Draw background pill
				int pill_w = label_surface->w + SCALE1(20);
				int pill_h = ui_button_size;
				SDL_Rect pill_rect = {x - pill_w/2, dot_y - pill_h/2, pill_w, pill_h};
				GFX_blitPill(ASSET_BUTTON, screen, &pill_rect);
				
				// Draw label text on top
				int text_x = x - label_surface->w/2;
				int text_y = dot_y - label_surface->h/2;
				SDL_Surface* text_surface = TTF_RenderUTF8_Blended(font.medium, lockscreen.pressed_button_labels[i], theme_accent);
				if (text_surface) {
					SDL_BlitSurface(text_surface, NULL, screen, &(SDL_Rect){text_x, text_y});
					SDL_FreeSurface(text_surface);
				}
				SDL_FreeSurface(label_surface);
			}
		} else {
			// Draw empty circular dot
			drawCircle(screen, x, dot_y, dot_radius, dot_color);
		}
	}
}

