#include "storage_manager.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

static const char *TAG = "storage";
static bool s_initialized = false;

esp_err_t storage_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    esp_vfs_littlefs_conf_t conf = {
        .base_path = STORAGE_BASE_PATH,
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount LittleFS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_littlefs_info("littlefs", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS: total=%d KB, used=%d KB", total/1024, used/1024);
    }
    
    s_initialized = true;
    return ESP_OK;
}

esp_err_t storage_deinit(void)
{
    if (!s_initialized) return ESP_OK;
    
    esp_err_t ret = esp_vfs_littlefs_unregister("littlefs");
    s_initialized = false;
    return ret;
}

esp_err_t storage_read_file(const char *path, char *buffer, size_t *size)
{
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_BASE_PATH, path);
    
    FILE *f = fopen(full_path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", full_path);
        return ESP_FAIL;
    }
    
    size_t read_size = fread(buffer, 1, *size - 1, f);
    buffer[read_size] = '\0';
    *size = read_size;
    
    fclose(f);
    ESP_LOGI(TAG, "Read %d bytes from %s", read_size, path);
    return ESP_OK;
}

esp_err_t storage_write_file(const char *path, const char *data, size_t size)
{
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_BASE_PATH, path);
    
    FILE *f = fopen(full_path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create file: %s", full_path);
        return ESP_FAIL;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written != size) {
        ESP_LOGE(TAG, "Write size mismatch: %d != %d", written, size);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Wrote %d bytes to %s", written, path);
    return ESP_OK;
}

bool storage_file_exists(const char *path)
{
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_BASE_PATH, path);
    
    struct stat st;
    return (stat(full_path, &st) == 0);
}

esp_err_t storage_delete_file(const char *path)
{
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", STORAGE_BASE_PATH, path);
    
    if (unlink(full_path) == 0) {
        ESP_LOGI(TAG, "Deleted file: %s", path);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to delete: %s", path);
    return ESP_FAIL;
}
