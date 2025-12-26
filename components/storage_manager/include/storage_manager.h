#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>

#define STORAGE_BASE_PATH "/littlefs"

/**
 * @brief Initialize LittleFS storage
 * @return ESP_OK on success
 */
esp_err_t storage_init(void);

/**
 * @brief Deinitialize storage
 * @return ESP_OK on success
 */
esp_err_t storage_deinit(void);

/**
 * @brief Read file from storage
 * @param path File path relative to base path
 * @param buffer Output buffer
 * @param size Buffer size (in), bytes read (out)
 * @return ESP_OK on success
 */
esp_err_t storage_read_file(const char *path, char *buffer, size_t *size);

/**
 * @brief Write file to storage
 * @param path File path relative to base path
 * @param data Data to write
 * @param size Data size
 * @return ESP_OK on success
 */
esp_err_t storage_write_file(const char *path, const char *data, size_t size);

/**
 * @brief Check if file exists
 * @param path File path
 * @return true if exists
 */
bool storage_file_exists(const char *path);

/**
 * @brief Delete file
 * @param path File path
 * @return ESP_OK on success
 */
esp_err_t storage_delete_file(const char *path);

#endif
