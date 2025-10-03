#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libintl.h>
#include <locale.h>

#include "defines.h"
#include "utils.h"
#include "config.h"

///////////////////////////////////////
// i18n support

#define _(STRING) gettext(STRING)
#define N_(STRING) STRING

// Language name to locale code mapping
static const char* getLocaleCode(const char* language_name) {
	if (strcmp(language_name, "中文") == 0) {
		return "zh_CN.UTF-8";
	}
	return "C"; // Default to English
}

void I18N_init(void) {
	// Initialize i18n
	setlocale(LC_ALL, "");
	bindtextdomain("minui", SDCARD_PATH "/.system/locale");
	textdomain("minui");
	
	// Apply language setting from config
	const char* language = CONFIG_getLanguage();
	const char* locale = getLocaleCode(language);
	setlocale(LC_ALL, locale);
}

void I18N_updateLanguage(const char* language_name) {
	// Update locale based on language name
	const char* locale = getLocaleCode(language_name);
	setlocale(LC_ALL, locale);
}

