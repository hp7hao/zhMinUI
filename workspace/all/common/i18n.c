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
	if (strcmp(language_name, "zh_CN") == 0) {
		return "zh_CN.UTF-8";
	} else if (strcmp(language_name, "en_US") == 0) {
		return "C"; // Use C locale for English (no translation needed)
	}
	return "C"; // Default fallback
}

void I18N_init(void) {
	// Initialize i18n
	printf("[INFO] I18N_init: Initializing i18n system\n");
	setlocale(LC_ALL, "");
	
	const char* locale_path = SDCARD_PATH "/.system/locale";
	printf("[INFO] I18N_init: Binding text domain to: %s\n", locale_path);
	bindtextdomain("minui", locale_path);
	textdomain("minui");
	
	// Apply language setting from config
	const char* language = CONFIG_getLanguage();
	printf("[INFO] I18N_init: Current language from config: %s\n", language);
	const char* locale = getLocaleCode(language);
	printf("[INFO] I18N_init: Setting locale to: %s\n", locale);
	setlocale(LC_ALL, locale);
	printf("[INFO] I18N_init: i18n initialization completed\n");
}

void I18N_updateLanguage(const char* language_name) {
	// Update locale based on language name
	printf("[INFO] I18N_updateLanguage: Changing language to: %s\n", language_name);
	const char* locale = getLocaleCode(language_name);
	printf("[INFO] I18N_updateLanguage: Setting locale to: %s\n", locale);
	setlocale(LC_ALL, locale);
	printf("[INFO] I18N_updateLanguage: Language change completed\n");
}

// Language options - display names (translated)
static const char* language_options[] = {
	N_("English"),
	N_("中文"),
	NULL
};

// Language values - original values for config
static const char* language_values[] = {
	"en_US",
	"zh_CN",
	NULL
};

// Get language display options
const char** I18N_getLanguageOptions(void) {
	return language_options;
}

// Get language config values
const char** I18N_getLanguageValues(void) {
	return language_values;
}

// Get language count
int I18N_getLanguageCount(void) {
	int count = 0;
	while (language_values[count] != NULL) {
		count++;
	}
	return count;
}

// Get language index by code
int I18N_getLanguageIndex(const char* language_code) {
	printf("[INFO] I18N_getLanguageIndex: Looking for code '%s'\n", language_code);
	for (int i = 0; language_values[i] != NULL; i++) {
		printf("[INFO] I18N_getLanguageIndex: Checking against '%s'\n", language_values[i]);
		if (strcmp(language_code, language_values[i]) == 0) {
			printf("[INFO] I18N_getLanguageIndex: Found match at index %d\n", i);
			return i;
		}
	}
	printf("[INFO] I18N_getLanguageIndex: No match found, returning default index 0\n");
	return 0; // Default to first language
}

// Get language code by index
const char* I18N_getLanguageCode(int index) {
	if (index >= 0 && index < I18N_getLanguageCount()) {
		return language_values[index];
	}
	return language_values[0]; // Default to first language
}

