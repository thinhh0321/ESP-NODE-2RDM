#ifndef SACN_RECEIVER_H
#define SACN_RECEIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sACN (E1.31) Receiver Module
 * 
 * This module receives and processes sACN (Streaming ACN / E1.31) protocol packets over UDP.
 * It handles multicast DMX data with priority and sequence number validation.
 * 
 * Protocol: ANSI E1.31 (sACN)
 * Port: UDP 5568
 * Multicast: 239.255.0.0 - 239.255.255.255
 * 
 * Features:
 * - Receives sACN DMX data packets
 * - Multicast universe subscription
 * - Priority handling (0-200)
 * - Sequence number validation
 * - Preview data detection
 * - Source name tracking
 */

// sACN constants
#define SACN_PORT 5568
#define SACN_MULTICAST_BASE "239.255.0.0"
#define SACN_MAX_UNIVERSES 8  // Maximum universes to subscribe

// sACN packet identifier
#define SACN_PACKET_IDENTIFIER "ASC-E1.17\0\0\0"

// sACN vectors
#define SACN_ROOT_VECTOR    0x00000004
#define SACN_FRAME_VECTOR   0x00000002
#define SACN_DMP_VECTOR     0x02

// sACN options flags
#define SACN_OPT_PREVIEW    0x80
#define SACN_OPT_STREAM_TERM 0x40

/**
 * @brief sACN Root Layer
 */
typedef struct __attribute__((packed)) {
    uint16_t preamble_size;      /**< Preamble size (0x0010) */
    uint16_t postamble_size;     /**< Postamble size (0x0000) */
    uint8_t acn_pid[12];         /**< ACN Packet Identifier */
    uint16_t flags_length;       /**< Flags (0x7) + PDU length */
    uint32_t vector;             /**< Root vector (0x00000004) */
    uint8_t cid[16];             /**< Component ID (UUID) */
} sacn_root_layer_t;

/**
 * @brief sACN Framing Layer
 */
typedef struct __attribute__((packed)) {
    uint16_t flags_length;       /**< Flags + PDU length */
    uint32_t vector;             /**< Framing vector (0x00000002) */
    char source_name[64];        /**< Source name (UTF-8, null terminated) */
    uint8_t priority;            /**< Priority (0-200, default 100) */
    uint16_t sync_address;       /**< Synchronization universe (0 = none) */
    uint8_t sequence_number;     /**< Sequence number (0-255) */
    uint8_t options;             /**< Options flags */
    uint16_t universe;           /**< Universe number (1-63999) */
} sacn_framing_layer_t;

/**
 * @brief sACN DMP Layer
 */
typedef struct __attribute__((packed)) {
    uint16_t flags_length;       /**< Flags + PDU length */
    uint8_t vector;              /**< DMP vector (0x02) */
    uint8_t address_type;        /**< Address type (0xA1) */
    uint16_t first_address;      /**< First property address (0x0000) */
    uint16_t address_increment;  /**< Address increment (0x0001) */
    uint16_t property_count;     /**< Property value count (1-513) */
    uint8_t start_code;          /**< DMX512 start code (0x00) */
    uint8_t data[512];           /**< DMX data (512 channels) */
} sacn_dmp_layer_t;

/**
 * @brief Complete sACN packet
 */
typedef struct __attribute__((packed)) {
    sacn_root_layer_t root;      /**< Root layer */
    sacn_framing_layer_t framing;/**< Framing layer */
    sacn_dmp_layer_t dmp;        /**< DMP layer */
} sacn_packet_t;

/**
 * @brief sACN receiver statistics
 */
typedef struct {
    uint32_t packets_received;   /**< Total packets received */
    uint32_t data_packets;        /**< Data packets received */
    uint32_t preview_packets;     /**< Preview packets received */
    uint32_t invalid_packets;     /**< Invalid packets */
    uint32_t sequence_errors;     /**< Sequence number errors */
} sacn_stats_t;

/**
 * @brief sACN DMX callback
 * @param universe Universe number (1-63999)
 * @param data DMX data (512 channels)
 * @param priority Priority (0-200)
 * @param sequence Sequence number
 * @param preview true if preview data
 * @param source_name Source name (null terminated)
 * @param user_data User data pointer
 */
typedef void (*sacn_dmx_callback_t)(uint16_t universe, const uint8_t *data,
                                    uint8_t priority, uint8_t sequence,
                                    bool preview, const char *source_name,
                                    void *user_data);

/**
 * @brief Initialize sACN receiver
 * 
 * Initializes the sACN receiver module. Must be called before any other functions.
 * 
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_NO_MEM if memory allocation failed
 *     - ESP_ERR_INVALID_STATE if already initialized
 */
esp_err_t sacn_receiver_init(void);

/**
 * @brief Deinitialize sACN receiver
 * 
 * Stops the receiver and frees resources.
 * 
 * @return
 *     - ESP_OK on success
 */
esp_err_t sacn_receiver_deinit(void);

/**
 * @brief Start sACN receiver
 * 
 * Starts the UDP receiver task on port 5568.
 * Receiver must be initialized first.
 * 
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_STATE if not initialized
 *     - ESP_FAIL if socket creation failed
 */
esp_err_t sacn_receiver_start(void);

/**
 * @brief Stop sACN receiver
 * 
 * Stops the receiver task and closes the socket.
 * Unsubscribes from all multicast groups.
 * 
 * @return
 *     - ESP_OK on success
 */
esp_err_t sacn_receiver_stop(void);

/**
 * @brief Subscribe to a universe
 * 
 * Joins the multicast group for the specified universe.
 * Universe 1 = 239.255.0.1, Universe 256 = 239.255.1.0, etc.
 * 
 * @param universe Universe number (1-63999)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if universe out of range
 *     - ESP_ERR_INVALID_STATE if not running
 *     - ESP_ERR_NO_MEM if maximum universes reached
 *     - ESP_FAIL if multicast join failed
 */
esp_err_t sacn_receiver_subscribe_universe(uint16_t universe);

/**
 * @brief Unsubscribe from a universe
 * 
 * Leaves the multicast group for the specified universe.
 * 
 * @param universe Universe number (1-63999)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if universe out of range
 *     - ESP_ERR_INVALID_STATE if not running
 *     - ESP_ERR_NOT_FOUND if not subscribed to universe
 */
esp_err_t sacn_receiver_unsubscribe_universe(uint16_t universe);

/**
 * @brief Register DMX data callback
 * 
 * Registers a callback to be called when sACN data packets are received.
 * Only one callback can be registered at a time.
 * 
 * @param callback Callback function
 * @param user_data User data pointer passed to callback
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if callback is NULL
 *     - ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t sacn_receiver_set_callback(sacn_dmx_callback_t callback, void *user_data);

/**
 * @brief Get receiver statistics
 * 
 * Retrieves current receiver statistics.
 * 
 * @param stats Pointer to statistics structure
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if stats is NULL
 *     - ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t sacn_receiver_get_stats(sacn_stats_t *stats);

/**
 * @brief Check if receiver is running
 * 
 * @return true if running, false otherwise
 */
bool sacn_receiver_is_running(void);

/**
 * @brief Get number of subscribed universes
 * 
 * @return Number of currently subscribed universes
 */
uint8_t sacn_receiver_get_subscription_count(void);

#ifdef __cplusplus
}
#endif

#endif // SACN_RECEIVER_H
