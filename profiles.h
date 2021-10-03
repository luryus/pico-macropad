#if !defined(PROFILES__H)
#define PROFILES__H

void prof_set_current_profile_name(const char *name);

void prof_set_current_key_names(const char *key_names);

char *prof_get_current_name();

char *prof_get_current_key_names();

#endif // PROFILES__H
