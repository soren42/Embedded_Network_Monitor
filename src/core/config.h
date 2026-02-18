#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_MAX_ENTRIES  32
#define CONFIG_KEY_MAX      64
#define CONFIG_VAL_MAX      128

typedef struct {
    char key[CONFIG_KEY_MAX];
    char value[CONFIG_VAL_MAX];
} config_entry_t;

typedef struct {
    config_entry_t entries[CONFIG_MAX_ENTRIES];
    int            count;
} config_t;

int         config_load(config_t *cfg, const char *path);
const char *config_get_str(const config_t *cfg, const char *key,
                           const char *default_val);
int         config_get_int(const config_t *cfg, const char *key,
                           int default_val);
double      config_get_double(const config_t *cfg, const char *key,
                              double default_val);
int         config_save(const config_t *cfg, const char *path);

#endif /* CONFIG_H */
