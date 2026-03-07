#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Trim leading and trailing whitespace in-place; return pointer into buf. */
static char *trim(char *buf)
{
    while (isspace((unsigned char)*buf)) buf++;
    if (*buf == '\0') return buf;

    char *end = buf + strlen(buf) - 1;
    while (end > buf && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return buf;
}

static int config_find(const config_t *cfg, const char *key)
{
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

int config_load(config_t *cfg, const char *path)
{
    if (!cfg || !path) return -1;

    cfg->count = 0;

    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *p = trim(line);

        /* Skip empty lines and comments */
        if (*p == '\0' || *p == '#') continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);

        if (*key == '\0') continue;
        if (cfg->count >= CONFIG_MAX_ENTRIES) break;

        /* Update existing or add new */
        int idx = config_find(cfg, key);
        if (idx >= 0) {
            snprintf(cfg->entries[idx].value, CONFIG_VAL_MAX, "%s", val);
        } else {
            snprintf(cfg->entries[cfg->count].key,   CONFIG_KEY_MAX, "%s", key);
            snprintf(cfg->entries[cfg->count].value,  CONFIG_VAL_MAX, "%s", val);
            cfg->count++;
        }
    }

    fclose(fp);
    return 0;
}

const char *config_get_str(const config_t *cfg, const char *key,
                           const char *default_val)
{
    if (!cfg || !key) return default_val;

    int idx = config_find(cfg, key);
    if (idx >= 0) return cfg->entries[idx].value;
    return default_val;
}

int config_get_int(const config_t *cfg, const char *key, int default_val)
{
    const char *val = config_get_str(cfg, key, NULL);
    if (!val) return default_val;

    char *endptr;
    long result = strtol(val, &endptr, 10);
    if (endptr == val) return default_val;
    return (int)result;
}

double config_get_double(const config_t *cfg, const char *key,
                         double default_val)
{
    const char *val = config_get_str(cfg, key, NULL);
    if (!val) return default_val;

    char *endptr;
    double result = strtod(val, &endptr);
    if (endptr == val) return default_val;
    return result;
}

void config_set_str(config_t *cfg, const char *key, const char *value)
{
    if (!cfg || !key || !value) return;

    int idx = config_find(cfg, key);
    if (idx >= 0) {
        snprintf(cfg->entries[idx].value, CONFIG_VAL_MAX, "%s", value);
    } else if (cfg->count < CONFIG_MAX_ENTRIES) {
        snprintf(cfg->entries[cfg->count].key,   CONFIG_KEY_MAX, "%s", key);
        snprintf(cfg->entries[cfg->count].value,  CONFIG_VAL_MAX, "%s", value);
        cfg->count++;
    }
}

void config_set_int(config_t *cfg, const char *key, int value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    config_set_str(cfg, key, buf);
}

int config_save(const config_t *cfg, const char *path)
{
    if (!cfg || !path) return -1;

    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    for (int i = 0; i < cfg->count; i++) {
        fprintf(fp, "%s=%s\n", cfg->entries[i].key, cfg->entries[i].value);
    }

    fclose(fp);
    return 0;
}
