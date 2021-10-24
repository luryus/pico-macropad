#include "profiles.h"

#include "constants.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char current_profile_name[1 + MACROPAD_PROFILE_NAME_LENGTH] = {0};
static char current_key_names[MACROPAD_KEY_NAME_LENGTH * MACROPAD_KEY_COUNT] = {0};

void prof_set_current_profile_name(const char *name) {
    memset(current_profile_name, 0, sizeof(current_profile_name));
    strncpy(current_profile_name, name, MACROPAD_PROFILE_NAME_LENGTH);
}

void prof_set_current_key_names(const char *key_names) {
    // key_names must be a 12-element array of 4-element char arrays (no null terminators)
    // Just copy it into its place
    memcpy(current_key_names, key_names, sizeof(current_key_names));

    // Then ensure that all the characters are valid.
    // Replace invalid characters with spaces.
    for (uint8_t i = 0; i < sizeof(current_key_names); i++) {
        char c = current_key_names[i];
        if (c < 32 || c > 126) {
            current_key_names[i] = ' ';
        }
    }
}

char *prof_get_current_name() {
    return current_profile_name;
}

char *prof_get_current_key_names() {
    return current_key_names;
}
