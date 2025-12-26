#ifndef ARTNET_RECEIVER_H
#define ARTNET_RECEIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Art-Net Receiver Module
 * 
 * This module receives and processes Art-Net v4 protocol packets over UDP.
 * It handles ArtDmx (DMX data), ArtPoll (discovery), and ArtPollReply packets.
 * 
 * Protocol: Art-Net v4
 * Port: UDP 6454
 * Broadcast: 2.255.255.255 or 10.255.255.255
 * 
 * Features:
 * - Receives ArtDmx packets with DMX512 data
 * - Responds to ArtPoll discovery requests
 * - Universe routing (0-32767)
 * - Sequence number tracking
 * - Source tracking
 */

// Art-Net constants
#define ARTNET_PORT 6454
#define ARTNET_HEADER "Art-Net\0"
#define ARTNET_PROTOCOL_VERSION 14
#define ARTNET_MAX_UNIVERSES 4  // Maximum universes to track

// Art-Net OpCodes
#define ARTNET_OP_POLL        0x2000
#define ARTNET_OP_POLL_REPLY  0x2100
#define ARTNET_OP_DMX         0x5000
#define ARTNET_OP_ADDRESS     0x6000
#define ARTNET_OP_SYNC        0x5200

/**
 * @brief Art-Net DMX packet structure
 */
typedef struct __attribute__((packed)) {
    uint8_t id[8];           /**< "Art-Net\0" */
    uint16_t opcode;         /**< OpCode (0x5000 for ArtDmx) */
    uint8_t prot_ver_hi;     /**< Protocol version high byte (0) */
    uint8_t prot_ver_lo;     /**< Protocol version low byte (14) */
    uint8_t sequence;        /**< Sequence number */
    uint8_t physical;        /**< Physical port */
    uint16_t universe;       /**< Universe (15-bit) */
    uint16_t length;         /**< DMX data length (2-512, high byte first) */
    uint8_t data[512];       /**< DMX data */
} artnet_dmx_packet_t;

/**
 * @brief Art-Net Poll packet structure
 */
typedef struct __attribute__((packed)) {
    uint8_t id[8];           /**< "Art-Net\0" */
    uint16_t opcode;         /**< OpCode (0x2000 for ArtPoll) */
    uint8_t prot_ver_hi;     /**< Protocol version high byte */
    uint8_t prot_ver_lo;     /**< Protocol version low byte */
    uint8_t flags;           /**< Talk flags */
    uint8_t priority;        /**< Diagnostics priority */
} artnet_poll_packet_t;

/**
 * @brief Art-Net PollReply packet structure
 */
typedef struct __attribute__((packed)) {
    uint8_t id[8];              /**< "Art-Net\0" */
    uint16_t opcode;            /**< OpCode (0x2100) */
    uint8_t ip[4];              /**< IP address */
    uint16_t port;              /**< Port number (6454) */
    uint16_t version_info;      /**< Firmware version */
    uint8_t net_switch;         /**< Net switch */
    uint8_t sub_switch;         /**< Sub-net switch */
    uint16_t oem;               /**< OEM code */
    uint8_t ubea_version;       /**< UBEA version */
    uint8_t status1;            /**< Status 1 */
    uint16_t esta_man;          /**< ESTA manufacturer code */
    char short_name[18];        /**< Short name (null terminated) */
    char long_name[64];         /**< Long name (null terminated) */
    char node_report[64];       /**< Node report */
    uint16_t num_ports;         /**< Number of ports (high byte first) */
    uint8_t port_types[4];      /**< Port types */
    uint8_t good_input[4];      /**< Input status */
    uint8_t good_output[4];     /**< Output status */
    uint8_t swin[4];            /**< Input universe */
    uint8_t swout[4];           /**< Output universe */
    uint8_t sw_video;           /**< Video switch */
    uint8_t sw_macro;           /**< Macro switch */
    uint8_t sw_remote;          /**< Remote switch */
    uint8_t spare[3];           /**< Spare */
    uint8_t style;              /**< Node style */
    uint8_t mac[6];             /**< MAC address */
    uint8_t bind_ip[4];         /**< Bind IP */
    uint8_t bind_index;         /**< Bind index */
    uint8_t status2;            /**< Status 2 */
    uint8_t filler[26];         /**< Filler */
} artnet_poll_reply_packet_t;

/**
 * @brief Art-Net receiver statistics
 */
typedef struct {
    uint32_t packets_received;   /**< Total packets received */
    uint32_t dmx_packets;         /**< DMX packets received */
    uint32_t poll_packets;        /**< Poll packets received */
    uint32_t poll_replies_sent;   /**< Poll replies sent */
    uint32_t invalid_packets;     /**< Invalid packets */
    uint32_t sequence_errors;     /**< Sequence number errors */
} artnet_stats_t;

/**
 * @brief Art-Net DMX callback
 * @param universe Universe number (0-32767)
 * @param data DMX data (512 channels)
 * @param length Data length (actual length, 2-512)
 * @param sequence Sequence number
 * @param user_data User data pointer
 */
typedef void (*artnet_dmx_callback_t)(uint16_t universe, const uint8_t *data, 
                                       uint16_t length, uint8_t sequence, 
                                       void *user_data);

/**
 * @brief Initialize Art-Net receiver
 * 
 * Initializes the Art-Net receiver module. Must be called before any other functions.
 * 
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_NO_MEM if memory allocation failed
 *     - ESP_ERR_INVALID_STATE if already initialized
 */
esp_err_t artnet_receiver_init(void);

/**
 * @brief Deinitialize Art-Net receiver
 * 
 * Stops the receiver and frees resources.
 * 
 * @return
 *     - ESP_OK on success
 */
esp_err_t artnet_receiver_deinit(void);

/**
 * @brief Start Art-Net receiver
 * 
 * Starts the UDP receiver task on port 6454.
 * Receiver must be initialized first.
 * 
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_STATE if not initialized
 *     - ESP_FAIL if socket creation failed
 */
esp_err_t artnet_receiver_start(void);

/**
 * @brief Stop Art-Net receiver
 * 
 * Stops the receiver task and closes the socket.
 * 
 * @return
 *     - ESP_OK on success
 */
esp_err_t artnet_receiver_stop(void);

/**
 * @brief Register DMX data callback
 * 
 * Registers a callback to be called when ArtDmx packets are received.
 * Only one callback can be registered at a time.
 * 
 * @param callback Callback function
 * @param user_data User data pointer passed to callback
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if callback is NULL
 *     - ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t artnet_receiver_set_callback(artnet_dmx_callback_t callback, void *user_data);

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
esp_err_t artnet_receiver_get_stats(artnet_stats_t *stats);

/**
 * @brief Enable or disable ArtPollReply responses
 * 
 * Controls whether the receiver responds to ArtPoll requests.
 * Enabled by default.
 * 
 * @param enable true to enable, false to disable
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t artnet_receiver_enable_poll_reply(bool enable);

/**
 * @brief Check if receiver is running
 * 
 * @return true if running, false otherwise
 */
bool artnet_receiver_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // ARTNET_RECEIVER_H
