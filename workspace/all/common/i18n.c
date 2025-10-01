#include <stdio.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>

#include "defines.h"
#include "utils.h"

///////////////////////////////////////
// i18n support

#define _(STRING) gettext(STRING)
#define N_(STRING) STRING

void I18N_init(void) {
	// Initialize i18n
	setlocale(LC_ALL, "");
	bindtextdomain("minui", SDCARD_PATH "/.system/locale");
	textdomain("minui");
	
	// Read locale from config file
	char locale_config[256];
	sprintf(locale_config, "%s/.system/locale.conf", SDCARD_PATH);
	if (exists(locale_config)) {
		char locale[32];
		getFile(locale_config, locale, 32);
		trimTrailingNewlines(locale);
		if (strlen(locale) > 0) {
			setlocale(LC_ALL, locale);
		}
	}
}
