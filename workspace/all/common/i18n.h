#ifndef __I18N_H__
#define __I18N_H__

#include <libintl.h>
#include <locale.h>

///////////////////////////////////////
// i18n support

#define _(STRING) gettext(STRING)
#define N_(STRING) STRING

void I18N_init(void);

#endif
