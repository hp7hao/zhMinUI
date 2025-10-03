#ifndef __I18N_H__
#define __I18N_H__

#include <libintl.h>
#include <locale.h>

///////////////////////////////////////
// i18n support

#define _(STRING) gettext(STRING)
#define N_(STRING) STRING

void I18N_init(void);
void I18N_updateLanguage(const char* language_name);

// Language options
const char** I18N_getLanguageOptions(void);
const char** I18N_getLanguageValues(void);
int I18N_getLanguageCount(void);
int I18N_getLanguageIndex(const char* language_code);
const char* I18N_getLanguageCode(int index);

#endif
