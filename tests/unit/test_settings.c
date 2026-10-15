/* waterbox_settings.h, compiled on the host as test_slots.c is: the reader
 * walks keys only, so a string VALUE equal to the key asked for is not taken
 * for it. The shipped flycast and pcsx2 packages have such a pair -
 * textureFiltering may be "machine", and a setting named machine exists - and
 * were safe only because machine is declared first. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "waterbox_settings.h"

int main(void)
{
	const char *json = "{\"textureFiltering\":\"machine\",\"machine\":\"naomi\",\"scale\":3,\"fast\":true}";
	wbx_settings_use_buffer(json, (int)strlen(json));
	char out[64];
	assert(wbx_setting_str("machine", out, sizeof out) == 5 && strcmp(out, "naomi") == 0);
	assert(wbx_setting_str("textureFiltering", out, sizeof out) == 7 && strcmp(out, "machine") == 0);
	assert(wbx_setting_long("scale", 0) == 3);
	assert(wbx_setting_bool("fast", 0) == 1);
	assert(wbx_setting_str("naomi", out, sizeof out) == -1);   /* a value is not a key */
	wbx_settings_use_file();
	puts("test_settings: ok");
	return 0;
}
