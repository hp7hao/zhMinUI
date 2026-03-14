// rg35xxplus standalone lockscreen
// Launched by keymon when sleep is triggered. Draws "Screen Locked" UI.
// Exit codes: 0 = unlocked, 2 = power button pressed (re-sleep)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/input.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_sigterm = 0;
static void sigusr1_handler(int sig) { (void)sig; got_sigusr1 = 1; }
static void sigterm_handler(int sig) { (void)sig; got_sigterm = 1; }

#define FIXED_BPP		2
#define FIXED_DEPTH		(FIXED_BPP * 8)
#define RGBA_MASK_565	0xF800, 0x07E0, 0x001F, 0x0000

#define FONT_PATH "/mnt/sdcard/.system/res/fonts/BoutiqueBitmap7x7_1.7.ttf"
#define MAX_UNLOCK_BUTTONS 3
#define SLEEP_TIMEOUT_MS 60000 // 60 seconds
#define READY_PATH "/tmp/lockscreen_ready"

// Raw evdev key codes (must match platform.c)
#define RAW_UP		103
#define RAW_DOWN	108
#define RAW_LEFT	105
#define RAW_RIGHT	106
#define RAW_A		304
#define RAW_B		305
#define RAW_X		307
#define RAW_Y		306
#define RAW_START	311
#define RAW_SELECT	310
#define RAW_L1		308
#define RAW_R1		309
#define RAW_POWER	116

// for ev.value
#define RELEASED	0
#define PRESSED		1

static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Texture* texture;
static SDL_Surface* screen;    // composited frame
static SDL_Surface* bg;        // pre-rendered unlock background (with dots + hint)

static TTF_Font* font_large;
static TTF_Font* font_medium;

static int screen_w, screen_h;

// Layout constants (set in main after getting screen size)
static int dot_spacing;
static int dot_start_x;
static int dot_y;
static int dot_r;

// evdev input fds
static int ev_fd0 = -1; // /dev/input/event0 (dpad, face buttons)
static int ev_fd1 = -1; // /dev/input/event1 (shoulder, power, vol)

static const char* buttonLabel(int code) {
	switch (code) {
		case RAW_A:      return "A";
		case RAW_B:      return "B";
		case RAW_X:      return "X";
		case RAW_Y:      return "Y";
		case RAW_UP:     return "UP";
		case RAW_DOWN:   return "DN";
		case RAW_LEFT:   return "LT";
		case RAW_RIGHT:  return "RT";
		case RAW_START:  return "ST";
		case RAW_SELECT: return "SE";
		case RAW_L1:     return "L1";
		case RAW_R1:     return "R1";
		default:         return NULL;
	}
}

static void renderText(SDL_Surface* dst, TTF_Font* f, const char* text, int cx, int cy, SDL_Color color) {
	SDL_Surface* surf = TTF_RenderUTF8_Solid(f, text, color);
	if (!surf) return;
	SDL_Rect rect = {cx - surf->w / 2, cy - surf->h / 2, surf->w, surf->h};
	SDL_BlitSurface(surf, NULL, dst, &rect);
	SDL_FreeSurface(surf);
}

static void drawCircle(SDL_Surface* dst, int cx, int cy, int radius, Uint32 color) {
	int r2 = radius * radius;
	for (int y = -radius; y <= radius; y++) {
		for (int x = -radius; x <= radius; x++) {
			if (x*x + y*y <= r2) {
				int px = cx + x;
				int py = cy + y;
				if (px >= 0 && px < dst->w && py >= 0 && py < dst->h) {
					Uint16* pix = (Uint16*)((Uint8*)dst->pixels + py * dst->pitch + px * FIXED_BPP);
					*pix = (Uint16)color;
				}
			}
		}
	}
}

static void present(void) {
	SDL_UpdateTexture(texture, NULL, screen->pixels, screen->pitch);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

// Draw just "Screen Locked" title on black — shown while not yet ready for input
static void drawSplash(void) {
	SDL_Color fg = {0xFF, 0xFF, 0xFF, 0xFF};
	int scale = 2;

	SDL_FillRect(screen, NULL, 0);
	renderText(screen, font_large, "Waking up", screen_w / 2, screen_h / 2, fg);
	present();
}

// Pre-render the unlock background: black + title + hint + 3 empty dots
static void renderBackground(void) {
	SDL_Color fg = {0xFF, 0xFF, 0xFF, 0xFF};
	SDL_Color dim = {0x40, 0x40, 0x60, 0xFF};
	int scale = 2;

	SDL_FillRect(bg, NULL, 0);

	renderText(bg, font_large, "Screen Locked", screen_w / 2, scale * 60, fg);
	renderText(bg, font_medium, "Press any 3 buttons to unlock", screen_w / 2, screen_h - scale * 60, fg);

	Uint32 c = SDL_MapRGB(bg->format, dim.r, dim.g, dim.b);
	for (int i = 0; i < MAX_UNLOCK_BUTTONS; i++) {
		int x = dot_start_x + i * dot_spacing;
		drawCircle(bg, x, dot_y, dot_r, c);
	}
}

// Composite a frame: blit pre-rendered bg, then draw filled dots on top
static void drawLockscreen(int button_count, const char* labels[]) {
	SDL_Color fg = {0xFF, 0xFF, 0xFF, 0xFF};
	SDL_Color accent = {0x80, 0x80, 0xFF, 0xFF};

	SDL_BlitSurface(bg, NULL, screen, NULL);

	for (int i = 0; i < button_count && i < MAX_UNLOCK_BUTTONS; i++) {
		if (!labels[i]) continue;
		int x = dot_start_x + i * dot_spacing;
		Uint32 c = SDL_MapRGB(screen->format, accent.r, accent.g, accent.b);
		drawCircle(screen, x, dot_y, dot_r, c);
		renderText(screen, font_medium, labels[i], x, dot_y, fg);
	}

	present();
}

// Poll evdev for a button press. Returns evdev code, or -1 if none, or -2 if power button.
static int pollInput(void) {
	struct input_event ev;
	int fds[] = {ev_fd0, ev_fd1};
	for (int i = 0; i < 2; i++) {
		if (fds[i] < 0) continue;
		while (read(fds[i], &ev, sizeof(ev)) == sizeof(ev)) {
			if (ev.type != 0x01) continue; // EV_KEY
			if (ev.value != PRESSED) continue; // only key-down
			if (ev.code == RAW_POWER) return -2;
			if (buttonLabel(ev.code)) return ev.code;
		}
	}
	return -1;
}

// Drain all pending evdev events
static void drainInput(void) {
	struct input_event ev;
	int fds[] = {ev_fd0, ev_fd1};
	for (int i = 0; i < 2; i++) {
		if (fds[i] < 0) continue;
		while (read(fds[i], &ev, sizeof(ev)) == sizeof(ev)) {}
	}
}

int main(int argc, char* argv[]) {
	// Set up signal handlers
	struct sigaction sa;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);

	sa.sa_handler = sigusr1_handler;
	sigaction(SIGUSR1, &sa, NULL);

	sa.sa_handler = sigterm_handler;
	sigaction(SIGTERM, &sa, NULL);

	// Open evdev inputs (non-blocking)
	ev_fd0 = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	ev_fd1 = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC);

	// Drain any stale events from before we started
	drainInput();

	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	TTF_Init();

	SDL_DisplayMode mode;
	SDL_GetCurrentDisplayMode(0, &mode);
	screen_w = mode.w;
	screen_h = mode.h;

	window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_SHOWN);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, screen_w, screen_h);

	screen = SDL_CreateRGBSurface(SDL_SWSURFACE, screen_w, screen_h, FIXED_DEPTH, RGBA_MASK_565);
	bg = SDL_CreateRGBSurface(SDL_SWSURFACE, screen_w, screen_h, FIXED_DEPTH, RGBA_MASK_565);

	// Layout
	int scale = 2;
	dot_spacing = scale * 50;
	dot_start_x = screen_w / 2 - dot_spacing;
	dot_y = screen_h / 2;
	dot_r = scale * 12;

	font_large = TTF_OpenFont(FONT_PATH, scale * 16);
	font_medium = TTF_OpenFont(FONT_PATH, scale * 14);
	if (!font_large || !font_medium) {
		fprintf(stderr, "lockscreen: failed to load font %s\n", FONT_PATH);
		goto cleanup;
	}

	// Pre-render unlock background (dots + hint) for later
	renderBackground();

	// Outer loop: each iteration = one sleep/wake cycle.
	// keymon kills and relaunches us for each re-sleep to ensure fresh GPU context.
	while (1) {
		// Draw splash: just "Screen Locked" centered — no dots, no hint
		drawSplash();

		// Signal keymon that we're ready (first frame on framebuffer)
		got_sigusr1 = 0;
		{ int fd = open(READY_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd >= 0) close(fd); }

		// Wait for SIGUSR1 from keymon (screen has been unblanked)
		while (!got_sigusr1 && !got_sigterm) {
			pause();
		}
		if (got_sigterm) break; // keymon asked us to exit

		// Drain stale events from while we were paused (wake button press etc)
		drainInput();

		int button_count = 0;
		const char* labels[MAX_UNLOCK_BUTTONS] = {NULL, NULL, NULL};
		uint32_t last_input = SDL_GetTicks();
		int exit_code = 0;

		// Now show the full unlock UI with dots — input is ready
		drawLockscreen(button_count, labels);

		while (1) {
			int code = pollInput();

			if (code == -2) {
				exit_code = 2;
				break;
			}

			if (code >= 0 && button_count < MAX_UNLOCK_BUTTONS) {
				labels[button_count++] = buttonLabel(code);
				last_input = SDL_GetTicks();
				drawLockscreen(button_count, labels);

				if (button_count >= MAX_UNLOCK_BUTTONS) {
					SDL_Delay(200);
					exit_code = 0;
					break;
				}
			}

			if (code >= 0) {
				last_input = SDL_GetTicks();
			}

			if (SDL_GetTicks() - last_input >= SLEEP_TIMEOUT_MS) {
				exit_code = 2;
				break;
			}

			usleep(16666); // ~60fps
		}

		if (exit_code == 0) break; // unlocked — exit process
		// exit_code == 2: re-sleep — loop back to splash + pause
	}

cleanup:
	unlink(READY_PATH);
	if (font_large) TTF_CloseFont(font_large);
	if (font_medium) TTF_CloseFont(font_medium);
	if (bg) SDL_FreeSurface(bg);
	if (screen) SDL_FreeSurface(screen);
	if (texture) SDL_DestroyTexture(texture);
	if (renderer) SDL_DestroyRenderer(renderer);
	if (window) SDL_DestroyWindow(window);
	TTF_Quit();
	SDL_Quit();
	if (ev_fd0 >= 0) close(ev_fd0);
	if (ev_fd1 >= 0) close(ev_fd1);
	return 0;
}
