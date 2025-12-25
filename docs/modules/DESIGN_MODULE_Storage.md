# THIẾT KẾ MODULE: Storage

**Dự án:** Artnet-Node-2RDM  
**Module:** Storage Management (LittleFS & NVS)  
**Phiên bản:** 1.0  
**Ngày tạo:** 25/12/2025

---

## 1. Mục đích

Module Storage quản lý lưu trữ dữ liệu persistent:
- **LittleFS**: File system cho config.json và web files
- **NVS**: Non-Volatile Storage cho binary config backup
- **OTA**: Firmware update partitions

---

## 2. Storage Layout

### 2.1. Flash Partition Table

```
# Name,     Type, SubType,  Offset,   Size,     Flags
nvs,        data, nvs,      0x9000,   0x6000,
otadata,    data, ota,      0xF000,   0x2000,
phy_init,   data, phy,      0x11000,  0x1000,
ota_0,      app,  ota_0,    0x20000,  0x300000,
ota_1,      app,  ota_1,    0x320000, 0x300000,
littlefs,   data, spiffs,   0x620000, 0x100000,
```

**Total: 16MB Flash**
- NVS: 24 KB (configuration backup)
- OTA Data: 8 KB (boot partition info)
- PHY Init: 4 KB (WiFi calibration)
- OTA_0: 3 MB (primary firmware)
- OTA_1: 3 MB (backup firmware for OTA)
- LittleFS: 1 MB (config + web files)

---

## 3. LittleFS Structure

```
/littlefs/
├── config.json           # Main configuration file (2-4 KB)
├── config.json.bak       # Backup configuration (2-4 KB)
├── web/
│   ├── index.html        # Main web page (10-20 KB)
│   ├── script.js         # JavaScript (20-50 KB)
│   ├── style.css         # CSS (5-10 KB)
│   ├── favicon.ico       # Icon (1-2 KB)
│   └── logo.png          # Logo image (5-10 KB)
└── logs/
    └── system.log        # System log (max 50 KB, circular)
```

---

## 4. API Public

### 4.1. Initialization

```c
/**
 * Khởi tạo Storage module
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_init(void);

/**
 * Mount LittleFS
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_mount_littlefs(void);

/**
 * Unmount LittleFS
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_unmount_littlefs(void);

/**
 * Format LittleFS partition
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_format_littlefs(void);
```

### 4.2. File Operations

```c
/**
 * Read file from LittleFS
 * @param path File path
 * @param buffer Buffer to store data
 * @param max_size Maximum size to read
 * @return Number of bytes read, or -1 on error
 */
int storage_read_file(const char *path, uint8_t *buffer, size_t max_size);

/**
 * Write file to LittleFS
 * @param path File path
 * @param data Data to write
 * @param size Data size
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_write_file(const char *path, const uint8_t *data, size_t size);

/**
 * Delete file from LittleFS
 * @param path File path
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_delete_file(const char *path);

/**
 * Check if file exists
 * @param path File path
 * @return true if exists
 */
bool storage_file_exists(const char *path);

/**
 * Get file size
 * @param path File path
 * @return File size in bytes, or -1 on error
 */
int storage_get_file_size(const char *path);
```

### 4.3. NVS Operations

```c
/**
 * Write binary data to NVS
 * @param namespace NVS namespace
 * @param key Key name
 * @param data Data to write
 * @param size Data size
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_nvs_write(const char *namespace, const char *key,
                            const void *data, size_t size);

/**
 * Read binary data from NVS
 * @param namespace NVS namespace
 * @param key Key name
 * @param data Buffer to store data
 * @param size Buffer size
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_nvs_read(const char *namespace, const char *key,
                           void *data, size_t *size);

/**
 * Delete key from NVS
 * @param namespace NVS namespace
 * @param key Key name
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_nvs_delete(const char *namespace, const char *key);

/**
 * Erase NVS namespace
 * @param namespace NVS namespace
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_nvs_erase_namespace(const char *namespace);
```

### 4.4. Configuration Management

```c
/**
 * Load configuration from storage
 * Priority: NVS -> LittleFS -> Defaults
 * @param config Buffer to store configuration
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_load_config(system_config_t *config);

/**
 * Save configuration to storage (both NVS and LittleFS)
 * @param config Configuration to save
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_save_config(const system_config_t *config);

/**
 * Backup current config file
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_backup_config(void);

/**
 * Restore config from backup
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_restore_config_backup(void);
```

### 4.5. Statistics

```c
/**
 * Get LittleFS usage statistics
 * @param total_bytes Total partition size
 * @param used_bytes Used space
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_get_usage(size_t *total_bytes, size_t *used_bytes);

/**
 * Get NVS statistics
 * @param namespace NVS namespace
 * @param used_entries Used entries
 * @param free_entries Free entries
 * @return ESP_OK nếu thành công
 */
esp_err_t storage_nvs_get_stats(const char *namespace,
                               size_t *used_entries,
                               size_t *free_entries);
```

---

## 5. Implementation

### 5.1. LittleFS Mount

```c
static bool littlefs_mounted = false;

esp_err_t storage_mount_littlefs(void) {
    if (littlefs_mounted) {
        return ESP_OK;
    }
    
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", 
                    esp_err_to_name(ret));
        }
        return ret;
    }
    
    // Get partition info
    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS: total=%d, used=%d", total, used);
    }
    
    littlefs_mounted = true;
    return ESP_OK;
}
```

### 5.2. File I/O

```c
int storage_read_file(const char *path, uint8_t *buffer, size_t max_size) {
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "/littlefs/%s", path);
    
    FILE *f = fopen(full_path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", full_path);
        return -1;
    }
    
    size_t read_size = fread(buffer, 1, max_size, f);
    fclose(f);
    
    return read_size;
}

esp_err_t storage_write_file(const char *path, const uint8_t *data, 
                             size_t size) {
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "/littlefs/%s", path);
    
    FILE *f = fopen(full_path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create file: %s", full_path);
        return ESP_FAIL;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written != size) {
        ESP_LOGE(TAG, "Write error: expected %d, wrote %d", size, written);
        return ESP_FAIL;
    }
    
    // Sync to flash
    fsync(fileno(f));
    
    return ESP_OK;
}
```

### 5.3. NVS Operations

```c
esp_err_t storage_nvs_write(const char *namespace, const char *key,
                           const void *data, size_t size) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open(namespace, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_set_blob(nvs_handle, key, data, size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }
    
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    return err;
}

esp_err_t storage_nvs_read(const char *namespace, const char *key,
                          void *data, size_t *size) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open(namespace, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_get_blob(nvs_handle, key, data, size);
    nvs_close(nvs_handle);
    
    return err;
}
```

### 5.4. Configuration Save/Load

```c
esp_err_t storage_save_config(const system_config_t *config) {
    esp_err_t ret;
    
    // 1. Save to NVS (binary)
    ret = storage_nvs_write("config", "system_config", 
                           config, sizeof(system_config_t));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save config to NVS: %s", 
                esp_err_to_name(ret));
    }
    
    // 2. Backup existing config file
    if (storage_file_exists("config.json")) {
        storage_backup_config();
    }
    
    // 3. Save to LittleFS (JSON)
    cJSON *root = cJSON_CreateObject();
    config_to_json(config, root);
    
    char *json_str = cJSON_Print(root);
    ret = storage_write_file("config.json", (uint8_t*)json_str, 
                            strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save config to file");
        return ret;
    }
    
    ESP_LOGI(TAG, "Configuration saved successfully");
    return ESP_OK;
}

esp_err_t storage_load_config(system_config_t *config) {
    esp_err_t ret;
    
    // 1. Try NVS first (fastest)
    size_t size = sizeof(system_config_t);
    ret = storage_nvs_read("config", "system_config", config, &size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Config loaded from NVS");
        return ESP_OK;
    }
    
    // 2. Try LittleFS
    uint8_t buffer[4096];
    int read_size = storage_read_file("config.json", buffer, sizeof(buffer));
    if (read_size > 0) {
        buffer[read_size] = '\0';
        cJSON *root = cJSON_Parse((char*)buffer);
        if (root) {
            ret = json_to_config(root, config);
            cJSON_Delete(root);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Config loaded from file");
                return ESP_OK;
            }
        }
    }
    
    // 3. Load defaults
    ESP_LOGW(TAG, "Loading default configuration");
    config_load_defaults(config);
    
    return ESP_OK;
}
```

---

## 6. Error Handling

| Error | Action |
|-------|--------|
| LittleFS mount fail | Format and retry |
| File not found | Return error or use defaults |
| Write fail | Retry 3 times |
| JSON parse error | Use backup or defaults |
| NVS full | Erase old data |
| Partition not found | Log error, use NVS only |

---

## 7. Wear Leveling

### 7.1. LittleFS
- Built-in wear leveling
- No special handling needed

### 7.2. NVS
- Flash wear leveling managed by ESP-IDF
- Avoid excessive writes
- Update only when config actually changes

### 7.3. Write Frequency Limits

```c
// Rate limiting for config saves
static uint64_t last_save_time = 0;
#define MIN_SAVE_INTERVAL_MS  5000  // 5 seconds

esp_err_t storage_save_config_rate_limited(const system_config_t *config) {
    uint64_t now = esp_timer_get_time() / 1000;  // ms
    
    if (now - last_save_time < MIN_SAVE_INTERVAL_MS) {
        ESP_LOGW(TAG, "Config save rate limited");
        return ESP_ERR_INVALID_STATE;
    }
    
    last_save_time = now;
    return storage_save_config(config);
}
```

---

## 8. Backup & Recovery

### 8.1. Config Backup

```c
esp_err_t storage_backup_config(void) {
    uint8_t buffer[4096];
    int size = storage_read_file("config.json", buffer, sizeof(buffer));
    
    if (size > 0) {
        return storage_write_file("config.json.bak", buffer, size);
    }
    
    return ESP_FAIL;
}

esp_err_t storage_restore_config_backup(void) {
    uint8_t buffer[4096];
    int size = storage_read_file("config.json.bak", buffer, sizeof(buffer));
    
    if (size > 0) {
        return storage_write_file("config.json", buffer, size);
    }
    
    return ESP_FAIL;
}
```

### 8.2. Factory Reset

```c
esp_err_t storage_factory_reset(void) {
    // 1. Erase NVS
    storage_nvs_erase_namespace("config");
    
    // 2. Delete config file
    storage_delete_file("config.json");
    storage_delete_file("config.json.bak");
    
    // 3. Format LittleFS (optional - keeps web files)
    // storage_format_littlefs();
    
    ESP_LOGI(TAG, "Factory reset completed");
    
    return ESP_OK;
}
```

---

## 9. Dependencies

- **ESP-IDF Components**:
  - esp_littlefs
  - nvs_flash
  - vfs (Virtual File System)
- **External Libraries**:
  - cJSON (for JSON parsing)

---

## 10. Testing Points

1. LittleFS mount/unmount
2. LittleFS format
3. File read/write/delete
4. File existence check
5. NVS read/write/delete
6. Config save to NVS
7. Config save to file
8. Config load priority (NVS -> File -> Defaults)
9. Config backup/restore
10. Factory reset
11. Wear leveling verification
12. Rate limiting
13. Storage usage statistics

---

## 11. Memory Usage

- LittleFS buffers: 8 KB
- NVS handles: 1 KB
- JSON buffers: 4 KB (temporary)
- **Total: ~13 KB**

---

## 12. Performance

- File read: ~100 KB/s
- File write: ~50 KB/s
- NVS read: < 1ms
- NVS write: 10-50ms
- JSON parse: ~5ms for config file

---

## 13. Logging

```c
// Optional: System log to file
esp_err_t storage_log_write(const char *message) {
    FILE *f = fopen("/littlefs/logs/system.log", "a");
    if (!f) return ESP_FAIL;
    
    // Add timestamp
    time_t now;
    time(&now);
    char timestr[32];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", 
            localtime(&now));
    
    fprintf(f, "[%s] %s\n", timestr, message);
    fclose(f);
    
    return ESP_OK;
}

// Log rotation
esp_err_t storage_log_rotate(void) {
    int size = storage_get_file_size("logs/system.log");
    
    if (size > 50000) {  // 50 KB limit
        storage_delete_file("logs/system.log.old");
        // Rename current to old
        rename("/littlefs/logs/system.log", 
               "/littlefs/logs/system.log.old");
    }
    
    return ESP_OK;
}
```
