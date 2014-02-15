#ifndef _CONFIG_H_
#define _CONFIG_H_

extern void config_init(const char *filename);
extern void config_clear();
extern void config_save();

extern void config_get_string(const char *key, char *destination, char *defaultvalue);
extern void config_set_string(const char *key, float value);

extern int config_get_int(const char *key, int defaultvalue);
extern void config_set_int(const char *key, int value);

#endif
