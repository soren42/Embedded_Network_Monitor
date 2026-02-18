/**
 * @file data_store.h
 * @brief Persistent history data save/load
 */

#ifndef DATA_STORE_H
#define DATA_STORE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load history data from disk (APP_HISTORY_PATH).
 * Does nothing if the file does not exist.
 */
void data_store_load(void);

/**
 * Save current history data to disk (APP_HISTORY_PATH).
 * Called on clean shutdown.
 */
void data_store_save(void);

#ifdef __cplusplus
}
#endif

#endif /* DATA_STORE_H */
