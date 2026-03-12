// rg35xxplus — DirectFB2 direct display path
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "../common/config.h"
#include <pthread.h>
#include <signal.h>
#include <execinfo.h>

#include <directfb.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

int is_cubexx = 0;
int is_rg34xx = 0;
int on_hdmi = 0;

///////////////////////////////

// Raw evdev codes — only used for analog axes and PLAT_shouldWake
#define RAW_HATY	17
#define RAW_HATX	16
#define RAW_LSY		3
#define RAW_LSX		2
#define RAW_RSY		5
#define RAW_RSX		4
#define RAW_POWER	116

// RGP01 analog axes
#define RGP01_LSY	1
#define RGP01_LSX	0
#define RGP01_RSY	5
#define RGP01_RSX	2

// Xbox analog axes
#define XBOX_LSY	1
#define XBOX_LSX	0
#define XBOX_RSY	4
#define XBOX_RSX	3
#define XBOX_L2_AXIS	2
#define XBOX_R2_AXIS	5

typedef enum GamepadType {
	kGamepadTypeUnknown,
	kGamepadTypeRGP01,
	kGamepadTypeXbox,
} GamepadType;

// evdev fds: [0]=event0 (built-in analog), [1]=event1 (power button for wake),
//            [2]=event3 (external gamepad analog+HAT)
#define INPUT_COUNT 3
static int inputs[INPUT_COUNT];

#define kPadIndex 2
static GamepadType pad_type = kGamepadTypeUnknown;

#define LID_PATH "/sys/class/power_supply/axp2202-battery/hallkey"
void PLAT_initLid(void) {
	lid.has_lid = exists(LID_PATH);
}
int PLAT_lidChanged(int* state) {
	if (lid.has_lid) {
		int lid_open = getInt(LID_PATH);
		if (lid_open!=lid.is_open) {
			lid.is_open = lid_open;
			if (state) *state = lid_open;
			return 1;
		}
	}
	return 0;
}

static void checkForGamepad(void) {
	uint32_t now = SDL_GetTicks();
	static uint32_t last_check = 0;
	if (last_check==0 || now-last_check>2000) {
		last_check = now;
		int connected = exists("/dev/input/event3");
		if (inputs[kPadIndex]<0 && connected) {
			LOG_info("Connecting gamepad: ");
			char pad_name[256];
			getFile("/sys/class/input/event3/device/name", pad_name, 256);
			if (containsString(pad_name,"Anbernic")) {
				LOG_info("P01\n");
				pad_type = kGamepadTypeRGP01;
			}
			else if (containsString(pad_name, "Microsoft")) {
				LOG_info("Xbox\n");
				pad_type = kGamepadTypeXbox;
			}
			else {
				LOG_info("Unknown\n");
				pad_type = kGamepadTypeUnknown;
			}

			inputs[kPadIndex] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		}
		else if (inputs[kPadIndex]>=0 && !connected) {
			LOG_info("Gamepad disconnected\n");
			close(inputs[kPadIndex]);
			inputs[kPadIndex] = -1;
			pad_type = kGamepadTypeUnknown;
		}
	}
}

void PLAT_initInput(void) {
	// Only open evdev fds for analog axes and power button wake
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // built-in analog
	inputs[1] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power button (wake)
	inputs[kPadIndex] = -1;
	checkForGamepad();
}
void PLAT_quitInput(void) {
	for (int i=0; i<INPUT_COUNT; i++) {
		if (inputs[i]>=0) close(inputs[i]);
	}
}

// from <linux/input.h> which has BTN_ constants that conflict with platform.h
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY			0x01
#define EV_ABS			0x03

static struct VID_Context {
	IDirectFB            *dfb;
	IDirectFBDisplayLayer *layer;
	IDirectFBWindow      *window;
	IDirectFBSurface     *surface;     // window's drawable surface
	IDirectFBEventBuffer *evbuf;       // input event buffer
	IDirectFBSurface     *blit_src;    // cached preallocated surface for blit path
	void                 *blit_src_ptr; // data pointer of cached blit surface
	int                   blit_src_w;   // cached dimensions
	int                   blit_src_h;

	SDL_Surface* buffer;
	SDL_Surface* screen;

	GFX_Renderer* blit; // yeesh

	int width;
	int height;
	int pitch;
	int sharpness;
} vid;

void PLAT_pollInput(void) {
	// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i=0; i<BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick>=pad.repeat_at[i])) {
			pad.just_repeated |= btn; // set
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}

	checkForGamepad();

	// --- DFB2 input event buffer: buttons (built-in + external gamepad via DirectFB2) ---
	DFBInputEvent dfb_evt;
	while (vid.evbuf && vid.evbuf->GetEvent(vid.evbuf, DFB_EVENT(&dfb_evt))==DFB_OK) {
		if (dfb_evt.type!=DIET_KEYPRESS && dfb_evt.type!=DIET_KEYRELEASE) continue;

		int btn = BTN_NONE;
		int pressed = (dfb_evt.type==DIET_KEYPRESS);
		int id = -1;

		// Map DFB2 key_id (DIKI_*) to MinUI buttons
		if (dfb_evt.flags & DIEF_KEYID) {
			DFBInputDeviceKeyIdentifier key_id = dfb_evt.key_id;
			// Built-in gamepad mapping (d-pad comes through as DIKI_UP/DOWN/LEFT/RIGHT)
				 if (key_id==DIKI_UP)        { btn = BTN_DPAD_UP;    id = BTN_ID_DPAD_UP; }
			else if (key_id==DIKI_DOWN)      { btn = BTN_DPAD_DOWN;  id = BTN_ID_DPAD_DOWN; }
			else if (key_id==DIKI_LEFT)      { btn = BTN_DPAD_LEFT;  id = BTN_ID_DPAD_LEFT; }
			else if (key_id==DIKI_RIGHT)     { btn = BTN_DPAD_RIGHT; id = BTN_ID_DPAD_RIGHT; }
			else if (key_id==DIKI_SPACE)     { btn = BTN_A;          id = BTN_ID_A; }       // BTN_SOUTH→KEY_SPACE
			else if (key_id==DIKI_CONTROL_L) { btn = BTN_B;          id = BTN_ID_B; }       // BTN_EAST→KEY_LEFTCTRL
			else if (key_id==DIKI_SHIFT_L)   { btn = BTN_X;          id = BTN_ID_X; }       // BTN_NORTH→KEY_LEFTSHIFT
			else if (key_id==DIKI_ALT_L)     { btn = BTN_Y;          id = BTN_ID_Y; }       // BTN_C→KEY_LEFTALT
			else if (key_id==DIKI_ENTER)     { btn = BTN_START;      id = BTN_ID_START; }   // BTN_TR→KEY_ENTER
			else if (key_id==DIKI_CONTROL_R) { btn = BTN_SELECT;     id = BTN_ID_SELECT; }  // BTN_TL→KEY_RIGHTCTRL
			else if (key_id==DIKI_ESCAPE)    { btn = BTN_MENU;       id = BTN_ID_MENU; }    // BTN_TL2→KEY_ESC
			else if (key_id==DIKI_TAB)       { btn = BTN_L1;         id = BTN_ID_L1; }      // BTN_WEST→KEY_TAB
			else if (key_id==DIKI_META_L)    { btn = BTN_L2;         id = BTN_ID_L2; }      // BTN_SELECT→KEY_LEFTMETA
			else if (key_id==DIKI_ALT_R)     { btn = BTN_L3;         id = BTN_ID_L3; }      // BTN_TR2→KEY_RIGHTALT
			else if (key_id==DIKI_BACKSPACE) { btn = BTN_R1;         id = BTN_ID_R1; }      // BTN_Z→KEY_BACKSPACE
			else if (key_id==DIKI_META_R)    { btn = BTN_R2;         id = BTN_ID_R2; }      // BTN_START→KEY_RIGHTMETA
			else if (key_id==DIKI_KP_DIV)    { btn = BTN_R3;         id = BTN_ID_R3; }      // BTN_THUMBR→KEY_KPSLASH
			else if (key_id==DIKI_KP_ENTER)  { btn = BTN_L3;         id = BTN_ID_L3; }      // BTN_THUMBL→KEY_KPENTER (alt L3)
			// External gamepad: different BTN codes produce different DIKI values
			else if (pad_type!=kGamepadTypeUnknown) {
				     if (key_id==DIKI_SUPER_R) { btn = BTN_MENU;    id = BTN_ID_MENU; }    // BTN_MODE→KEY_COMPOSE→DIKI_SUPER_R
			}
		}

		// Map DFB2 key_symbol (DIKS_*) for keys without DIKI_ identifiers
		if (btn==BTN_NONE && (dfb_evt.flags & DIEF_KEYSYMBOL)) {
			DFBInputDeviceKeySymbol key_sym = dfb_evt.key_symbol;
			     if (key_sym==DIKS_POWER)       { btn = BTN_POWER; id = BTN_ID_POWER; }
			else if (key_sym==DIKS_VOLUME_UP)   { btn = BTN_PLUS;  id = BTN_ID_PLUS; }
			else if (key_sym==DIKS_VOLUME_DOWN) { btn = BTN_MINUS; id = BTN_ID_MINUS; }
		}

		if (btn==BTN_NONE) continue;

		if (!pressed) {
			pad.is_pressed		&= ~btn; // unset
			pad.just_repeated	&= ~btn; // unset
			pad.just_released	|= btn; // set
		}
		else if ((pad.is_pressed & btn)==BTN_NONE) {
			pad.just_pressed	|= btn; // set
			pad.just_repeated	|= btn; // set
			pad.is_pressed		|= btn; // set
			pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
		}
	}

	// --- Raw evdev: analog axes + external gamepad HAT d-pad ---
	int input;
	static struct input_event ev;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		if (input<0) continue;
		while (read(input, &ev, sizeof(ev))==sizeof(ev)) {
			if (ev.type!=EV_ABS) continue;

			int code = ev.code;
			int value = ev.value;

			if (i==kPadIndex) {
				// External gamepad analog + HAT
				if (code==RAW_HATX || code==RAW_HATY) {
					// HAT d-pad on external gamepads
					int hats[4] = {-1,-1,-1,-1};
					if (code==RAW_HATY) {
						hats[0] = value==-1; // up
						hats[1] = value==1;  // down
					}
					else {
						hats[2] = value==-1; // left
						hats[3] = value==1;  // right
					}
					for (int id=0; id<4; id++) {
						int state = hats[id];
						int btn = 1 << id;
						if (state==0) {
							pad.is_pressed		&= ~btn;
							pad.just_repeated	&= ~btn;
							pad.just_released	|= btn;
						}
						else if (state==1 && (pad.is_pressed & btn)==BTN_NONE) {
							pad.just_pressed	|= btn;
							pad.just_repeated	|= btn;
							pad.is_pressed		|= btn;
							pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
						}
					}
				}
				else if (pad_type==kGamepadTypeRGP01) {
						 if (code==RGP01_LSX) { pad.laxis.x = ((value-128) * 32767) / 128; PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick+PAD_REPEAT_DELAY); }
					else if (code==RGP01_LSY) { pad.laxis.y = ((value-128) * 32767) / 128; PAD_setAnalog(BTN_ID_ANALOG_UP,   BTN_ID_ANALOG_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY); }
					else if (code==RGP01_RSX) pad.raxis.x = ((value-128) * 32767) / 128;
					else if (code==RGP01_RSY) pad.raxis.y = ((value-128) * 32767) / 128;
				}
				else if (pad_type==kGamepadTypeXbox) {
						 if (code==XBOX_LSX) { pad.laxis.x = value; PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick+PAD_REPEAT_DELAY); }
					else if (code==XBOX_LSY) { pad.laxis.y = value; PAD_setAnalog(BTN_ID_ANALOG_UP,   BTN_ID_ANALOG_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY); }
					else if (code==XBOX_RSX) pad.raxis.x = value;
					else if (code==XBOX_RSY) pad.raxis.y = value;
					else if (code==XBOX_L2_AXIS) { int pressed = value>0; int btn = BTN_L2; int id = BTN_ID_L2; if (!pressed) { pad.is_pressed &= ~btn; pad.just_repeated &= ~btn; pad.just_released |= btn; } else if ((pad.is_pressed & btn)==BTN_NONE) { pad.just_pressed |= btn; pad.just_repeated |= btn; pad.is_pressed |= btn; pad.repeat_at[id] = tick + PAD_REPEAT_DELAY; } }
					else if (code==XBOX_R2_AXIS) { int pressed = value>0; int btn = BTN_R2; int id = BTN_ID_R2; if (!pressed) { pad.is_pressed &= ~btn; pad.just_repeated &= ~btn; pad.just_released |= btn; } else if ((pad.is_pressed & btn)==BTN_NONE) { pad.just_pressed |= btn; pad.just_repeated |= btn; pad.is_pressed |= btn; pad.repeat_at[id] = tick + PAD_REPEAT_DELAY; } }
				}
			}
			else {
				// Built-in analog sticks (event0)
					 if (code==RAW_LSX) { pad.laxis.x = (value * 32767) / 4096; PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick+PAD_REPEAT_DELAY); }
				else if (code==RAW_LSY) { pad.laxis.y = (value * 32767) / 4096; PAD_setAnalog(BTN_ID_ANALOG_UP,   BTN_ID_ANALOG_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY); }
				else if (code==RAW_RSX) pad.raxis.x = (value * 32767) / 4096;
				else if (code==RAW_RSY) pad.raxis.y = (value * 32767) / 4096;
			}
		}
	}

	if (lid.has_lid && PLAT_lidChanged(NULL)) pad.just_released |= BTN_SLEEP;
}

int PLAT_shouldWake(void) {
	// Use raw evdev for wake — app may not have focus during sleep
	int lid_open = 1; // assume open by default
	if (lid.has_lid && PLAT_lidChanged(&lid_open) && lid_open) return 1;

	int input;
	static struct input_event event;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		if (input<0) continue;
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			// Wake on ANY button release (EV_KEY), not just power button
			// Ignore axis motion (EV_ABS) to prevent accidental wakes
			if (event.type==EV_KEY && event.value==0) {
				// ignore input while lid is closed
				if (lid.has_lid && !lid.is_open) return 0;  // do it here so we eat the input
				return 1;
			}
		}
	}
	return 0;
}

///////////////////////////////

// based on rgb30 + tg5040 + m17
#define HDMI_STATE_PATH "/sys/class/switch/hdmi/cable.0/state" // TODO: can detect but doesn't update automatically
#define BLANK_PATH "/sys/class/graphics/fb0/blank"

static int device_width;
static int device_height;
static int device_pitch;
static int rotate = 0;

static void crash_handler(int sig)
{
	void *bt[20];
	int n = backtrace(bt, 20);
	fprintf(stderr, "minui: caught signal %d, backtrace:\n", sig);
	backtrace_symbols_fd(bt, n, STDERR_FILENO);
	_exit(128 + sig);
}

SDL_Surface* PLAT_initVideo(void) {
	/* Install crash handler for diagnostics */
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = crash_handler;
	sa.sa_flags = SA_RESETHAND;
	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);

	char* model = getenv("RGXX_MODEL");
	is_cubexx = exactMatch("RGcubexx", model);
	is_rg34xx = prefixMatch("RG34xx", model);

	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	if (getInt(HDMI_STATE_PATH)) {
		w = HDMI_WIDTH;
		h = HDMI_HEIGHT;
		p = HDMI_PITCH;
		on_hdmi = 1;
	}

	// Initialize DirectFB2 (connects as slave to minwm's multi-app core)
	DirectFBInit(NULL, NULL);
	DirectFBCreate(&vid.dfb);

	vid.dfb->GetDisplayLayer(vid.dfb, DLID_PRIMARY, &vid.layer);
	vid.layer->SetCooperativeLevel(vid.layer, DLSCL_SHARED);

	// Check display rotation
	DFBDisplayLayerConfig layer_config;
	vid.layer->GetConfiguration(vid.layer, &layer_config);
	if (layer_config.height > layer_config.width) rotate = 3;

	// Create full-screen window with RGB16 pixel format
	DFBWindowDescription wdsc;
	memset(&wdsc, 0, sizeof(wdsc));
	wdsc.flags  = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT |
	              DWDESC_CAPS | DWDESC_PIXELFORMAT | DWDESC_SURFACE_CAPS;
	wdsc.posx   = 0;
	wdsc.posy   = 0;
	wdsc.width  = w;
	wdsc.height = h;
	wdsc.caps   = DWCAPS_DOUBLEBUFFER;
	wdsc.pixelformat = DSPF_RGB16;
	wdsc.surface_caps = DSCAPS_DOUBLE;

	vid.layer->CreateWindow(vid.layer, &wdsc, &vid.window);
	vid.window->GetSurface(vid.window, &vid.surface);
	vid.window->SetOpacity(vid.window, 0xFF);

	// Create global input event buffer (receives ALL DFB2 input events)
	vid.dfb->CreateInputEventBuffer(vid.dfb, DICAPS_ALL, DFB_TRUE, &vid.evbuf);

	// SDL2 for software surfaces only (no SDL_INIT_VIDEO needed)
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.screen	= SDL_CreateRGBSurface(SDL_SWSURFACE, w,h, FIXED_DEPTH, RGBA_MASK_565);
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;

	device_width	= w;
	device_height	= h;
	device_pitch	= p;

	vid.sharpness = SHARPNESS_SOFT;

	return vid.screen;
}

static void clearVideo(void) {
	SDL_FillRect(vid.screen, NULL, 0);
	if (vid.surface) {
		vid.surface->Clear(vid.surface, 0, 0, 0, 0xFF);
		vid.surface->Flip(vid.surface, NULL, DSFLIP_NONE);
		vid.surface->Clear(vid.surface, 0, 0, 0, 0xFF);
		vid.surface->Flip(vid.surface, NULL, DSFLIP_NONE);
	}
}

void PLAT_quitVideo(void) {
	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	if (vid.blit_src) vid.blit_src->Release(vid.blit_src);
	if (vid.evbuf)   vid.evbuf->Release(vid.evbuf);
	if (vid.surface) vid.surface->Release(vid.surface);
	if (vid.window)  vid.window->Release(vid.window);
	if (vid.layer)   vid.layer->Release(vid.layer);
	if (vid.dfb)     vid.dfb->Release(vid.dfb);
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_Color bg_color = CONFIG_getThemeBackground();
	Uint32 bg_rgb = SDL_MapRGB(screen->format, bg_color.r, bg_color.g, bg_color.b);
	SDL_FillRect(screen, NULL, bg_rgb);
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen);
	if (vid.surface) {
		vid.surface->Clear(vid.surface, 0, 0, 0, 0xFF);
	}
}

void PLAT_setVsync(int vsync) {
	// buh
}

static int hard_scale = 4;

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;

	if (w>=device_width && h>=device_height) hard_scale = 1;
	else if (h>=160) hard_scale = 2;
	else hard_scale = 4;

	LOG_info("resizeVideo(%i,%i,%i) hard_scale: %i\n",w,h,p, hard_scale);

	SDL_FreeSurface(vid.buffer);
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);

	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}
void PLAT_setNearestNeighbor(int enabled) {
	// buh
}
void PLAT_setSharpness(int sharpness) {
	vid.sharpness = sharpness;
	// No texture recreation needed — DFB2 uses CPU blitting
}

static struct FX_Context {
	int scale;
	int type;
	int color;
	int next_scale;
	int next_type;
	int next_color;
	int live_type;
} effect = {
	.scale = 1,
	.next_scale = 1,
	.type = EFFECT_NONE,
	.next_type = EFFECT_NONE,
	.live_type = EFFECT_NONE,
	.color = 0,
	.next_color = 0,
};
void PLAT_setEffect(int next_type) {
	effect.next_type = next_type;
}
void PLAT_setEffectColor(int next_color) {
	effect.next_color = next_color;
}
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	// LOG_info("getScaler for scale: %i\n", renderer->scale);
	effect.next_scale = renderer->scale;
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
}

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {

	on_hdmi = GetHDMI();

	if (!vid.blit) {
		// Simple path: UI rendering — memcpy screen surface to DFB window surface
		resizeVideo(device_width,device_height,FIXED_PITCH);
		void *dst;
		int dst_pitch;
		if (vid.surface->Lock(vid.surface, DSLF_WRITE, &dst, &dst_pitch)==DFB_OK) {
			int copy_bytes = vid.width * FIXED_BPP;
			if (dst_pitch == vid.screen->pitch) {
				memcpy(dst, vid.screen->pixels, copy_bytes * vid.height);
			} else {
				uint8_t *src_row = (uint8_t*)vid.screen->pixels;
				uint8_t *dst_row = (uint8_t*)dst;
				for (int y=0; y<vid.height; y++) {
					memcpy(dst_row, src_row, copy_bytes);
					src_row += vid.screen->pitch;
					dst_row += dst_pitch;
				}
			}
			vid.surface->Unlock(vid.surface);
		}
		vid.surface->Flip(vid.surface, NULL, DSFLIP_NONE);
		return;
	}

	// Blit path: game rendering via minarch
	// Calculate destination rectangle (aspect ratio scaling)
	int dst_x = 0, dst_y = 0, dst_w = device_width, dst_h = device_height;

	if (vid.blit->aspect==0) { // native or cropped
		int w = vid.blit->src_w * vid.blit->scale;
		int h = vid.blit->src_h * vid.blit->scale;
		dst_x = (device_width - w) / 2;
		dst_y = (device_height - h) / 2;
		dst_w = w;
		dst_h = h;
	}
	else if (vid.blit->aspect>0) { // aspect ratio
		int h = device_height;
		int w = h * vid.blit->aspect;
		if (w>device_width) {
			double ratio = 1 / vid.blit->aspect;
			w = device_width;
			h = w * ratio;
		}
		dst_x = (device_width - w) / 2;
		dst_y = (device_height - h) / 2;
		dst_w = w;
		dst_h = h;
	}

	// Clear letterbox/pillarbox areas
	vid.surface->SetColor(vid.surface, 0, 0, 0, 0xFF);
	if (dst_y > 0) {
		vid.surface->FillRectangle(vid.surface, 0, 0, device_width, dst_y);
		vid.surface->FillRectangle(vid.surface, 0, dst_y + dst_h, device_width, device_height - (dst_y + dst_h));
	}
	if (dst_x > 0) {
		vid.surface->FillRectangle(vid.surface, 0, dst_y, dst_x, dst_h);
		vid.surface->FillRectangle(vid.surface, dst_x + dst_w, dst_y, device_width - (dst_x + dst_w), dst_h);
	}

	// Get or create preallocated DFB surface wrapping blit source buffer
	if (vid.blit_src && (vid.blit_src_ptr != vid.blit->src ||
	    vid.blit_src_w != vid.blit->true_w || vid.blit_src_h != vid.blit->true_h)) {
		vid.blit_src->Release(vid.blit_src);
		vid.blit_src = NULL;
	}
	if (!vid.blit_src) {
		DFBSurfaceDescription sdsc;
		memset(&sdsc, 0, sizeof(sdsc));
		sdsc.flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT |
		                   DSDESC_PREALLOCATED;
		sdsc.width       = vid.blit->true_w;
		sdsc.height      = vid.blit->true_h;
		sdsc.pixelformat = DSPF_RGB16;
		sdsc.preallocated[0].data  = vid.blit->src;
		sdsc.preallocated[0].pitch = vid.blit->src_p;
		if (vid.dfb->CreateSurface(vid.dfb, &sdsc, &vid.blit_src)==DFB_OK) {
			vid.blit_src_ptr = vid.blit->src;
			vid.blit_src_w   = vid.blit->true_w;
			vid.blit_src_h   = vid.blit->true_h;
		}
	}

	if (vid.blit_src) {
		DFBRectangle src_rect = {
			vid.blit->src_x, vid.blit->src_y,
			vid.blit->src_w, vid.blit->src_h
		};
		DFBRectangle dst_rect = { dst_x, dst_y, dst_w, dst_h };

		if (src_rect.w==dst_w && src_rect.h==dst_h) {
			vid.surface->Blit(vid.surface, vid.blit_src, &src_rect, dst_x, dst_y);
		} else {
			vid.surface->StretchBlit(vid.surface, vid.blit_src, &src_rect, &dst_rect);
		}
	}

	vid.surface->Flip(vid.surface, NULL, DSFLIP_NONE);
	vid.blit = NULL;
}

int PLAT_supportsOverscan(void) { return is_cubexx; }

///////////////////////////////

// TODO:
#define OVERLAY_WIDTH PILL_SIZE // unscaled
#define OVERLAY_HEIGHT PILL_SIZE // unscaled
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 16
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP) // unscaled
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000 // ARGB
static struct OVL_Context {
	SDL_Surface* overlay;
} ovl;

SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}
void PLAT_enableOverlay(int enable) {

}

///////////////////////////////

static int online = 0;
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;

	*is_charging = getInt("/sys/class/power_supply/axp2202-usb/online");

	int i = getInt("/sys/class/power_supply/axp2202-battery/capacity");
	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;

	// wifi status, just hooking into the regular PWR polling
	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status,16);
	online = prefixMatch("up", status);
}

#define LED_PATH "/sys/class/power_supply/axp2202-battery/work_led"
void PLAT_enableBacklight(int enable) {
	if (enable) {
		putInt(BLANK_PATH, FB_BLANK_UNBLANK); // wake
		SetBrightness(GetBrightness());
		putInt(LED_PATH,1);
	}
	else {
		putInt(BLANK_PATH, FB_BLANK_POWERDOWN); // sleep
		SetRawBrightness(0);
		putInt(LED_PATH,0);
	}
}

void PLAT_powerOff(void) {
	system("rm -f /tmp/minui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	system("echo 0 > /sys/class/power_supply/axp2202-battery/work_led");
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	// system("cat /dev/zero > /dev/fb0 2>/dev/null");
	// system("shutdown");
	// while (1) pause(); // lolwat

	// touch("/tmp/poweroff");
	// sync();
	// system("touch /tmp/poweroff && sync");
	exit(0);
}

///////////////////////////////

void PLAT_setCPUSpeed(int speed) {
	// TODO: why wasn't this ever implemented?
}

#define RUMBLE_PATH "/sys/class/power_supply/axp2202-battery/moto"
void PLAT_setRumble(int strength) {
	if (GetHDMI()) return; // assume we're using a controller?
	putInt(RUMBLE_PATH, strength?1:0);
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

static char model[256];
char* PLAT_getModel(void) {
	// firmware "strings /mnt/vendor/bin/dmenu.bin | grep ^20"
	char* _model = getenv("RGXX_MODEL");
	if (_model!=NULL) {
		if (exactMatch(_model,"RGcubexx")) _model = "RG CubeXX";

		sprintf(model, "Anbernic %s", _model);
		char* tmp = strrchr(model, '_');
		if (tmp) *tmp = '\0';
		return model;
	}
	return "Anbernic RG*XX";
}

int PLAT_isOnline(void) {
	return online;
}
