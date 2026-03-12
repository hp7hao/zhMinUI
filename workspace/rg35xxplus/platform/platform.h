// rg35xx/platform/platform.h

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#include "sdl.h"

///////////////////////////////

extern int is_cubexx;
extern int is_rg34xx;
extern int on_hdmi;

///////////////////////////////

#define	BUTTON_UP		BUTTON_NA
#define	BUTTON_DOWN		BUTTON_NA
#define	BUTTON_LEFT		BUTTON_NA
#define	BUTTON_RIGHT	BUTTON_NA

#define	BUTTON_SELECT	BUTTON_NA
#define	BUTTON_START	BUTTON_NA

#define	BUTTON_A		BUTTON_NA
#define	BUTTON_B		BUTTON_NA
#define	BUTTON_X		BUTTON_NA
#define	BUTTON_Y		BUTTON_NA

#define	BUTTON_L1		BUTTON_NA
#define	BUTTON_R1		BUTTON_NA
#define	BUTTON_L2		BUTTON_NA
#define	BUTTON_R2		BUTTON_NA
#define BUTTON_L3 		BUTTON_NA
#define BUTTON_R3 		BUTTON_NA

#define	BUTTON_MENU		BUTTON_NA
#define	BUTTON_POWER	BUTTON_NA
#define	BUTTON_PLUS		BUTTON_NA
#define	BUTTON_MINUS	BUTTON_NA

///////////////////////////////

// SDL scancodes resulting from DirectFB2 gamepad-to-keyboard remapping:
// evdev BTN → linux_input remaps to KEY_* → key_translate() → SDL scancode
#define CODE_UP			82	/* SDL_SCANCODE_UP    (KEY_UP(103))       */
#define CODE_DOWN		81	/* SDL_SCANCODE_DOWN  (KEY_DOWN(108))     */
#define CODE_LEFT		80	/* SDL_SCANCODE_LEFT  (KEY_LEFT(105))     */
#define CODE_RIGHT		79	/* SDL_SCANCODE_RIGHT (KEY_RIGHT(106))    */

#define CODE_SELECT		228	/* SDL_SCANCODE_RCTRL (BTN_TL(310)→KEY_RIGHTCTRL(97))  */
#define CODE_START		40	/* SDL_SCANCODE_RETURN (BTN_TR(311)→KEY_ENTER(28))     */

#define CODE_A			44	/* SDL_SCANCODE_SPACE  (BTN_SOUTH(304)→KEY_SPACE(57))  */
#define CODE_B			224	/* SDL_SCANCODE_LCTRL  (BTN_EAST(305)→KEY_LEFTCTRL(29)) */
#define CODE_X			225	/* SDL_SCANCODE_LSHIFT (BTN_NORTH(307)→KEY_LEFTSHIFT(42)) */
#define CODE_Y			226	/* SDL_SCANCODE_LALT   (BTN_C(306)→KEY_LEFTALT(56))    */

#define CODE_L1			43	/* SDL_SCANCODE_TAB       (BTN_WEST(308)→KEY_TAB(15))      */
#define CODE_R1			42	/* SDL_SCANCODE_BACKSPACE (BTN_Z(309)→KEY_BACKSPACE(14))   */
#define CODE_L2			227	/* SDL_SCANCODE_LGUI   (BTN_SELECT(314)→KEY_LEFTMETA(125)) */
#define CODE_R2			231	/* SDL_SCANCODE_RGUI   (BTN_START(315)→KEY_RIGHTMETA(126)) */
#define CODE_L3			230	/* SDL_SCANCODE_RALT   (BTN_TR2(313)→KEY_RIGHTALT(100))    */
#define CODE_R3			101	/* SDL_SCANCODE_APPLICATION (BTN_THUMBR(318)→KEY_KPSLASH(98)) */

#define CODE_MENU		41	/* SDL_SCANCODE_ESCAPE (BTN_TL2(312)→KEY_ESC(1))       */
#define CODE_POWER		102	/* SDL_SCANCODE_POWER  (KEY_POWER(116), no remap)      */

#define CODE_PLUS		128	/* SDL_SCANCODE_VOLUMEUP   (KEY_VOLUMEUP(115))         */
#define CODE_MINUS		129	/* SDL_SCANCODE_VOLUMEDOWN (KEY_VOLUMEDOWN(114))       */

///////////////////////////////

#define JOY_UP			13
#define JOY_DOWN		16
#define JOY_LEFT		14
#define JOY_RIGHT		15

#define JOY_SELECT		6
#define JOY_START		7

#define JOY_A			0
#define JOY_B			1
#define JOY_X			3
#define JOY_Y			2

#define JOY_L1			4
#define JOY_R1			5
#define JOY_L2			9
#define JOY_R2			10
#define JOY_L3			JOY_NA
#define JOY_R3			JOY_NA

#define JOY_MENU		8
#define JOY_POWER		JOY_NA
#define JOY_PLUS		18
#define JOY_MINUS		17

///////////////////////////////

#define BTN_RESUME			BTN_X
#define BTN_SLEEP 			BTN_POWER
#define BTN_WAKE 			BTN_POWER
#define BTN_MOD_VOLUME 		BTN_NONE
#define BTN_MOD_BRIGHTNESS 	BTN_MENU
#define BTN_MOD_PLUS 		BTN_PLUS
#define BTN_MOD_MINUS 		BTN_MINUS

///////////////////////////////

#define FIXED_SCALE 	2
#define FIXED_WIDTH		(is_cubexx?720:(is_rg34xx?720:640))
#define FIXED_HEIGHT	(is_cubexx?720:480)
#define FIXED_BPP		2
#define FIXED_DEPTH		(FIXED_BPP * 8)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT)

///////////////////////////////

#define HAS_HDMI	1
#define HDMI_WIDTH 	1280
#define HDMI_HEIGHT 720
#define HDMI_PITCH 	(HDMI_WIDTH * FIXED_BPP)
#define HDMI_SIZE	(HDMI_PITCH * HDMI_HEIGHT)

// TODO: if HDMI_HEIGHT > FIXED_HEIGHT then MAIN_ROW_COUNT will be insufficient


///////////////////////////////

#define MAIN_ROW_COUNT (is_cubexx||on_hdmi?8:6)
#define PADDING (is_cubexx||on_hdmi?40:10)

///////////////////////////////

#define SDCARD_PATH "/mnt/sdcard"
#define MUTE_VOLUME_RAW 0
#define HAS_NEON
#define SAMPLES 400 // fix for (most) fceumm underruns

///////////////////////////////

#endif
