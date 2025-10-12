#include <stdio.h>
#include "defines.h"
#include "config.h"
#include "perf.h"

///////////////////////////////////////
// Performance Counter Module
// Config is the source of truth - always use CONFIG_getShowPerf()

static struct {
	unsigned long frame_times[60];
	int frame_index;
	unsigned long last_frame_time;
	float current_fps;
} perf;

void PERF_init(void) {
	perf.frame_index = 0;
	perf.last_frame_time = SDL_GetTicks();
	perf.current_fps = 60.0f;
	
	// Initialize frame times
	for (int i = 0; i < 60; i++) {
		perf.frame_times[i] = 0;
	}
}

void PERF_update(void) {
	if (!CONFIG_getShowPerf()) return; // Config is source of truth, already cached
	
	unsigned long current_time = SDL_GetTicks();
	unsigned long frame_time = current_time - perf.last_frame_time;
	perf.last_frame_time = current_time;
	
	// Store frame time
	perf.frame_times[perf.frame_index] = frame_time;
	perf.frame_index = (perf.frame_index + 1) % 60;
	
	// Calculate average FPS from last 60 frames
	unsigned long total_time = 0;
	int valid_frames = 0;
	for (int i = 0; i < 60; i++) {
		if (perf.frame_times[i] > 0) {
			total_time += perf.frame_times[i];
			valid_frames++;
		}
	}
	if (total_time > 0 && valid_frames > 0) {
		perf.current_fps = (valid_frames * 1000.0f) / total_time;
	}
}

float PERF_getFPS(void) {
	return perf.current_fps;
}

int PERF_isEnabled(void) {
	return CONFIG_getShowPerf(); // Config is source of truth, already cached
}

void PERF_draw(SDL_Surface* screen, TTF_Font* font) {
	if (!CONFIG_getShowPerf()) return; // Config is source of truth, already cached
	
	char fps_text[32];
	sprintf(fps_text, "FPS: %.1f", perf.current_fps);
	SDL_Color perf_color = {255, 255, 0, 255}; // Yellow
	SDL_Surface* perf_surface = TTF_RenderUTF8_Blended(font, fps_text, perf_color);
	if (perf_surface) {
		// Center at bottom of screen
		int x = (screen->w - perf_surface->w) / 2;
		int y = screen->h - perf_surface->h - SCALE1(4);
		SDL_BlitSurface(perf_surface, NULL, screen, &(SDL_Rect){x, y});
		SDL_FreeSurface(perf_surface);
	}
}

