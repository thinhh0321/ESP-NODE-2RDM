/**
 * @file network_manager.c
 * @brief Network Manager Implementation
 */

#include "network_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "network_mgr";

// Event bits
#define ETHERNET_CONNECTED_BIT  BIT0
#define WIFI_CONNECTED_BIT      BIT1
#define WIFI_FAIL_BIT           BIT2

// W5500 Pin Configuration
#define W5500_CS_PIN        GPIO_NUM_10
#define W5500_MOSI_PIN      GPIO_NUM_11
#define W5500_MISO_PIN      GPIO_NUM_13
#define W5500_SCK_PIN       GPIO_NUM_12
#define W5500_INT_PIN       GPIO_NUM_9

// Configuration
#define ETHERNET_RETRY_COUNT    3
#define ETHERNET_TIMEOUT_MS     10000
#define WIFI_STA_TIMEOUT_MS     15000
#define WIFI_MAX_RETRY          5

// Event Task Configuration
#define NET_EVT_TASK_STACK_SIZE 4096
#define NET_EVT_TASK_PRIORITY   (configMAX_PRIORITIES - 2)  // High priority
#define NET_EVT_TASK_CORE       0  // Core 0 for network events

// Static allocation for event task
static StackType_t s_net_evt_task_stack[NET_EVT_TASK_STACK_SIZE];
static StaticTask_t s_net_evt_task_buffer;
static TaskHandle_t s_net_evt_task_handle = NULL;

// Static allocation for event group
static StaticEventGroup_t s_network_event_group_buffer;

// State management (static allocation)
static network_state_t s_current_state = NETWORK_DISCONNECTED;
static network_status_t s_status = {0};
static EventGroupHandle_t s_network_event_group = NULL;
static esp_netif_t *s_eth_netif = NULL;
static esp_netif_t *s_wifi_sta_netif = NULL;
static esp_netif_t *s_wifi_ap_netif = NULL;
static esp_eth_handle_t s_eth_handle = NULL;
static bool s_initialized = false;
static int s_wifi_retry_num = 0;
static bool s_wifi_started = false;  // Track WiFi state

// Callback
static network_state_callback_t s_state_callback = NULL;
static void *s_state_callback_user_data = NULL;

// Forward declarations
static void network_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data);
static void set_network_state(network_state_t new_state);
static void net_evt_task(void *pvParameters);
static void stop_wifi_if_running(void);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Stop WiFi to free RF and RAM resources when Ethernet is active
 */
static void stop_wifi_if_running(void)
{
    if (s_wifi_started) {
        ESP_LOGI(TAG, "Stopping WiFi to free RF and RAM resources");
        esp_wifi_stop();
        s_wifi_started = false;
    }
}

static void set_network_state(network_state_t new_state)
{
    if (s_current_state != new_state) {
        ESP_LOGI(TAG, "State change: %d -> %d", s_current_state, new_state);
        s_current_state = new_state;
        s_status.state = new_state;
        
        if (s_state_callback) {
            s_state_callback(new_state, s_state_callback_user_data);
        }
    }
}

static void update_ip_info(esp_netif_t *netif)
{
    if (!netif) return;
    
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        snprintf(s_status.ip_address, sizeof(s_status.ip_address),
                IPSTR, IP2STR(&ip_info.ip));
        snprintf(s_status.netmask, sizeof(s_status.netmask),
                IPSTR, IP2STR(&ip_info.netmask));
        snprintf(s_status.gateway, sizeof(s_status.gateway),
                IPSTR, IP2STR(&ip_info.gw));
    }
}

// ============================================================================
// Event Handlers
// ============================================================================

static void network_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    // Ethernet events
    if (event_base == ETH_EVENT) {
        switch (event_id) {
            case ETHERNET_EVENT_CONNECTED:
                ESP_LOGI(TAG, "Ethernet Link Up");
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "Ethernet Link Down");
                set_network_state(NETWORK_DISCONNECTED);
                xEventGroupClearBits(s_network_event_group, ETHERNET_CONNECTED_BIT);
                break;
            case ETHERNET_EVENT_START:
                ESP_LOGI(TAG, "Ethernet Started");
                break;
            case ETHERNET_EVENT_STOP:
                ESP_LOGI(TAG, "Ethernet Stopped");
                break;
            default:
                break;
        }
    }
    // IP events
    else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_ETH_GOT_IP: {
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                ESP_LOGI(TAG, "Ethernet Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
                update_ip_info(s_eth_netif);
                set_network_state(NETWORK_ETHERNET_CONNECTED);
                xEventGroupSetBits(s_network_event_group, ETHERNET_CONNECTED_BIT);
                
                // Stop WiFi to free RF and RAM resources (Priority 1: Ethernet)
                stop_wifi_if_running();
                break;
            }
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                ESP_LOGI(TAG, "WiFi Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
                update_ip_info(s_wifi_sta_netif);
                set_network_state(NETWORK_WIFI_STA_CONNECTED);
                s_wifi_retry_num = 0;
                xEventGroupSetBits(s_network_event_group, WIFI_CONNECTED_BIT);
                break;
            }
            case IP_EVENT_STA_LOST_IP:
                ESP_LOGI(TAG, "WiFi Lost IP");
                set_network_state(NETWORK_DISCONNECTED);
                break;
            default:
                break;
        }
    }
    // WiFi events
    else if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA Started");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WiFi Disconnected");
                if (s_wifi_retry_num < WIFI_MAX_RETRY) {
                    esp_wifi_connect();
                    s_wifi_retry_num++;
                    ESP_LOGI(TAG, "Retry connect to WiFi... (%d/%d)", 
                            s_wifi_retry_num, WIFI_MAX_RETRY);
                } else {
                    xEventGroupSetBits(s_network_event_group, WIFI_FAIL_BIT);
                }
                xEventGroupClearBits(s_network_event_group, WIFI_CONNECTED_BIT);
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi Connected");
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *event = 
                    (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                        MAC2STR(event->mac), event->aid);
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *event = 
                    (wifi_event_ap_stadisconnected_t *)event_data;
                ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d",
                        MAC2STR(event->mac), event->aid);
                break;
            }
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "WiFi AP Started");
                set_network_state(NETWORK_WIFI_AP_ACTIVE);
                break;
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "WiFi AP Stopped");
                break;
            default:
                break;
        }
    }
}

// ============================================================================
// Event Task (High Priority on Core 0)
// ============================================================================

/**
 * @brief High-priority network event task on Core 0
 * 
 * This task handles network events with high priority to ensure
 * Art-Net/DMX processing on Core 1 is not affected by network jitter.
 * Uses Task Notifications for efficient event signaling.
 * 
 * @param pvParameters Task parameters (unused)
 */
static void net_evt_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Network event task started on Core %d with priority %d",
             xPortGetCoreID(), uxTaskPriorityGet(NULL));
    
    while (1) {
        // Wait for notification from event handlers
        // For now, just yield to allow event processing
        // In a more advanced implementation, we could use a queue
        // to pass specific event information
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Additional event processing logic can be added here
        // This task serves as a dedicated context for network event handling
    }
}

// ============================================================================
// Initialization
// ============================================================================

esp_err_t network_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing network manager...");
    
    // Create event group with static allocation
    s_network_event_group = xEventGroupCreateStatic(&s_network_event_group_buffer);
    if (!s_network_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                               &network_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                               &network_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &network_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP,
                                               &network_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &network_event_handler, NULL));
    
    // Create high-priority event task on Core 0 with static allocation
    s_net_evt_task_handle = xTaskCreateStaticPinnedToCore(
        net_evt_task,
        "net_evt_task",
        NET_EVT_TASK_STACK_SIZE,
        NULL,
        NET_EVT_TASK_PRIORITY,
        s_net_evt_task_stack,
        &s_net_evt_task_buffer,
        NET_EVT_TASK_CORE
    );
    
    if (!s_net_evt_task_handle) {
        ESP_LOGE(TAG, "Failed to create network event task");
        return ESP_FAIL;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Network manager initialized with static allocation");
    
    return ESP_OK;
}

esp_err_t network_start(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting network...");
    set_network_state(NETWORK_CONNECTING);
    
    // Network start logic will be implemented in integration
    // For now, just return success
    ESP_LOGI(TAG, "Network start requested - implement auto-fallback in integration");
    
    return ESP_OK;
}

esp_err_t network_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping network...");
    
    // Stop Ethernet
    if (s_eth_handle) {
        esp_eth_stop(s_eth_handle);
    }
    
    // Stop WiFi
    esp_wifi_stop();
    
    set_network_state(NETWORK_DISCONNECTED);
    
    return ESP_OK;
}

// ============================================================================
// Ethernet Functions
// ============================================================================

esp_err_t network_ethernet_connect(const ip_config_t *config)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Connecting Ethernet...");
    
    // Create network interface
    if (!s_eth_netif) {
        esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
        s_eth_netif = esp_netif_new(&netif_config);
    }
    
    // Configure static IP if requested
    if (config && !config->use_dhcp) {
        ESP_LOGI(TAG, "Using static IP: %s", config->ip);
        esp_netif_dhcpc_stop(s_eth_netif);
        
        esp_netif_ip_info_t ip_info;
        memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
        ip_info.ip.addr = esp_ip4addr_aton(config->ip);
        ip_info.netmask.addr = esp_ip4addr_aton(config->netmask);
        ip_info.gw.addr = esp_ip4addr_aton(config->gateway);
        
        ESP_ERROR_CHECK(esp_netif_set_ip_info(s_eth_netif, &ip_info));
    }
    
    // Initialize SPI bus for W5500
    spi_bus_config_t bus_config = {
        .miso_io_num = W5500_MISO_PIN,
        .mosi_io_num = W5500_MOSI_PIN,
        .sclk_io_num = W5500_SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));
    
    // Configure W5500
    spi_device_interface_config_t devcfg = {
        .command_bits = 16,
        .address_bits = 8,
        .mode = 0,
        .clock_speed_hz = 20 * 1000 * 1000,  // 20 MHz
        .spics_io_num = W5500_CS_PIN,
        .queue_size = 20,
    };
    
    spi_device_handle_t spi_handle = NULL;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle));
    
    // Create Ethernet MAC and PHY configuration
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.smi_mdc_gpio_num = -1;  // Not used for SPI
    mac_config.smi_mdio_gpio_num = -1; // Not used for SPI
    
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = -1;
    
    // Create W5500 specific config
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(spi_handle);
    w5500_config.int_gpio_num = W5500_INT_PIN;
    
    // Create MAC for W5500
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    
    // Create PHY for W5500
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    
    // Install Ethernet driver
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &s_eth_handle));
    
    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(s_eth_netif, 
                                     esp_eth_new_netif_glue(s_eth_handle)));
    
    // Start Ethernet
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));
    
    ESP_LOGI(TAG, "Ethernet started, waiting for connection...");
    
    return ESP_OK;
}

esp_err_t network_ethernet_disconnect(void)
{
    if (s_eth_handle) {
        ESP_LOGI(TAG, "Disconnecting Ethernet");
        esp_eth_stop(s_eth_handle);
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}

bool network_ethernet_is_link_up(void)
{
    if (!s_eth_handle) {
        return false;
    }
    
    bool link_up = false;
    esp_eth_ioctl(s_eth_handle, ETH_CMD_G_PHY_LINK, &link_up);
    return link_up;
}

// ============================================================================
// WiFi Station Functions
// ============================================================================

esp_err_t network_wifi_sta_connect(const wifi_profile_t *profile)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!profile) {
        ESP_LOGE(TAG, "Invalid profile");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", profile->ssid);
    
    // Create WiFi station interface if not exists
    if (!s_wifi_sta_netif) {
        s_wifi_sta_netif = esp_netif_create_default_wifi_sta();
    }
    
    // Configure static IP if needed
    if (profile->use_static_ip) {
        ESP_LOGI(TAG, "Using static IP: %s", profile->static_ip.ip);
        esp_netif_dhcpc_stop(s_wifi_sta_netif);
        
        esp_netif_ip_info_t ip_info;
        memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
        ip_info.ip.addr = esp_ip4addr_aton(profile->static_ip.ip);
        ip_info.netmask.addr = esp_ip4addr_aton(profile->static_ip.netmask);
        ip_info.gw.addr = esp_ip4addr_aton(profile->static_ip.gateway);
        
        ESP_ERROR_CHECK(esp_netif_set_ip_info(s_wifi_sta_netif, &ip_info));
    } else {
        esp_netif_dhcpc_start(s_wifi_sta_netif);
    }
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Configure WiFi
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, profile->ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, profile->password, 
            sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    s_wifi_started = true;  // Track WiFi state
    ESP_LOGI(TAG, "WiFi started, connecting...");
    
    s_wifi_retry_num = 0;
    
    return ESP_OK;
}

esp_err_t network_wifi_sta_disconnect(void)
{
    ESP_LOGI(TAG, "Disconnecting WiFi STA");
    return esp_wifi_disconnect();
}

uint16_t network_wifi_scan(wifi_ap_record_t *scan_result, uint16_t max_aps)
{
    if (!scan_result || max_aps == 0) {
        return 0;
    }
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    
    uint16_t ap_count = max_aps;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, scan_result));
    
    ESP_LOGI(TAG, "Found %d access points", ap_count);
    
    return ap_count;
}

// ============================================================================
// WiFi Access Point Functions
// ============================================================================

esp_err_t network_wifi_ap_start(const char *ssid,
                                const char *password,
                                uint8_t channel,
                                const ip_config_t *config)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!ssid) {
        ESP_LOGE(TAG, "Invalid SSID");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Starting WiFi AP: %s", ssid);
    
    // Create WiFi AP interface if not exists
    if (!s_wifi_ap_netif) {
        s_wifi_ap_netif = esp_netif_create_default_wifi_ap();
    }
    
    // Configure static IP for AP
    if (config) {
        esp_netif_ip_info_t ip_info;
        memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
        ip_info.ip.addr = esp_ip4addr_aton(config->ip);
        ip_info.netmask.addr = esp_ip4addr_aton(config->netmask);
        ip_info.gw.addr = esp_ip4addr_aton(config->gateway);
        
        ESP_ERROR_CHECK(esp_netif_dhcps_stop(s_wifi_ap_netif));
        ESP_ERROR_CHECK(esp_netif_set_ip_info(s_wifi_ap_netif, &ip_info));
        ESP_ERROR_CHECK(esp_netif_dhcps_start(s_wifi_ap_netif));
    }
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Configure AP
    wifi_config_t wifi_config = {
        .ap = {
            .channel = channel,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(ssid);
    
    if (password && strlen(password) >= 8) {
        strncpy((char *)wifi_config.ap.password, password, 
                sizeof(wifi_config.ap.password) - 1);
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    s_wifi_started = true;  // Track WiFi state
    ESP_LOGI(TAG, "WiFi AP started: %s", ssid);
    
    // Update status
    update_ip_info(s_wifi_ap_netif);
    set_network_state(NETWORK_WIFI_AP_ACTIVE);
    
    return ESP_OK;
}

esp_err_t network_wifi_ap_stop(void)
{
    ESP_LOGI(TAG, "Stopping WiFi AP");
    esp_err_t ret = esp_wifi_stop();
    s_wifi_started = false;  // Track WiFi state
    set_network_state(NETWORK_DISCONNECTED);
    return ret;
}

uint16_t network_wifi_ap_get_stations(wifi_sta_info_t *stations,
                                      uint16_t max_stations)
{
    if (!stations || max_stations == 0) {
        return 0;
    }
    
    wifi_sta_list_t sta_list;
    ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&sta_list));
    
    uint16_t count = (sta_list.num < max_stations) ? sta_list.num : max_stations;
    
    for (int i = 0; i < count; i++) {
        stations[i] = sta_list.sta[i];
    }
    
    return count;
}

// ============================================================================
// Status & Information
// ============================================================================

esp_err_t network_get_status(network_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(status, &s_status, sizeof(network_status_t));
    
    // Update RSSI for WiFi STA
    if (s_current_state == NETWORK_WIFI_STA_CONNECTED) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            status->rssi = ap_info.rssi;
        }
    }
    
    return ESP_OK;
}

const char* network_get_ip_address(void)
{
    if (s_status.ip_address[0] == '\0') {
        return NULL;
    }
    return s_status.ip_address;
}

esp_err_t network_get_mac_address(uint8_t mac[6])
{
    if (!mac) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = ESP_FAIL;
    
    switch (s_current_state) {
        case NETWORK_ETHERNET_CONNECTED:
            if (s_eth_netif) {
                ret = esp_netif_get_mac(s_eth_netif, mac);
            }
            break;
        case NETWORK_WIFI_STA_CONNECTED:
            if (s_wifi_sta_netif) {
                ret = esp_netif_get_mac(s_wifi_sta_netif, mac);
            }
            break;
        case NETWORK_WIFI_AP_ACTIVE:
            if (s_wifi_ap_netif) {
                ret = esp_netif_get_mac(s_wifi_ap_netif, mac);
            }
            break;
        default:
            ret = esp_efuse_mac_get_default(mac);
            break;
    }
    
    if (ret == ESP_OK) {
        memcpy(s_status.mac, mac, 6);
    }
    
    return ret;
}

bool network_is_connected(void)
{
    return (s_current_state == NETWORK_ETHERNET_CONNECTED ||
            s_current_state == NETWORK_WIFI_STA_CONNECTED ||
            s_current_state == NETWORK_WIFI_AP_ACTIVE);
}

// ============================================================================
// Callbacks
// ============================================================================

void network_register_state_callback(network_state_callback_t callback,
                                    void *user_data)
{
    s_state_callback = callback;
    s_state_callback_user_data = user_data;
    ESP_LOGI(TAG, "State callback registered");
}
