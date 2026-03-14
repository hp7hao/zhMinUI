// rg35xxplus power management helpers
// Shared by keymon and lockscreen — no SDL/GFX dependencies.
#ifndef PLATFORM_POWER_H
#define PLATFORM_POWER_H

#include <stdio.h>
#include <linux/fb.h>
#include <msettings.h>

#define PWR_BLANK_PATH "/sys/class/graphics/fb0/blank"
#define PWR_LED_PATH "/sys/class/power_supply/axp2202-battery/work_led"
#define PWR_CHARGING_PATH "/sys/class/power_supply/axp2202-usb/online"

static inline void pwrPutInt(const char* path, int val) {
	FILE* f = fopen(path, "w");
	if (f) { fprintf(f, "%d", val); fclose(f); }
}

static inline int pwrGetInt(const char* path) {
	int i = 0;
	FILE* f = fopen(path, "r");
	if (f) { fscanf(f, "%i", &i); fclose(f); }
	return i;
}

static inline void pwrEnableBacklight(int enable) {
	if (enable) {
		pwrPutInt(PWR_BLANK_PATH, FB_BLANK_UNBLANK);
		SetBrightness(GetBrightness());
		pwrPutInt(PWR_LED_PATH, 0);
	} else {
		pwrPutInt(PWR_BLANK_PATH, FB_BLANK_POWERDOWN);
		SetRawBrightness(0);
		pwrPutInt(PWR_LED_PATH, 1);
	}
}

static inline int pwrIsCharging(void) {
	return pwrGetInt(PWR_CHARGING_PATH);
}

#endif
