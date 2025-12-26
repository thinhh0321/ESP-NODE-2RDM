# DMX Handler Testing Guide

## Overview

This document provides comprehensive testing procedures for the DMX Handler component. Tests are designed to verify all functionality with real hardware.

## Test Equipment Required

### Essential Equipment
1. **ESP32-S3 Development Board** - With ESP-NODE-2RDM firmware
2. **RS485 Transceivers** - 2x MAX485 or equivalent
3. **DMX512 Cables** - Standard 5-pin XLR cables
4. **DMX Tester/Analyzer** - Or DMX-capable lighting fixture
5. **Power Supply** - 5V for ESP32 and RS485 modules
6. **Multimeter** - For GPIO verification
7. **Oscilloscope** - (Optional) for timing verification

### Optional Equipment
- RDM-capable lighting fixtures
- DMX512 controller (for input testing)
- Logic analyzer (for protocol analysis)
- DMX terminator (120Ω resistor)

## Hardware Setup

### Wiring Diagram

```
ESP32-S3 Port 1:
┌──────────────┐      ┌───────────┐      ┌──────────┐
│   ESP32-S3   │      │  MAX485   │      │   DMX    │
│              │      │           │      │  Fixture │
│  GPIO17 (TX) ├─────→│ DI        │      │          │
│  GPIO16 (RX) ├─────→│ RO        │      │          │
│  GPIO21 (DIR)├─────→│ DE/RE     │      │          │
│              │      │           │      │          │
│          GND ├──┬───│ GND       │      │          │
│              │  │   │           │      │          │
│          5V  ├──┼───│ VCC       │      │          │
│              │  │   │           │      │          │
└──────────────┘  │   │ A (D+)    ├──────┤ DMX+ (3) │
                  │   │ B (D-)    ├──────┤ DMX- (2) │
                  │   │           │      │          │
                  └───┤ Common GND├──────┤ GND  (1) │
                      └───────────┘      └──────────┘
```

**Pin Connections:**
- ESP32 GPIO → MAX485:
  - TX → DI (Driver Input)
  - RX → RO (Receiver Output)
  - DIR → DE and RE tied together
  - GND → GND
  - 5V → VCC

- MAX485 → DMX Connector:
  - A (Data+) → Pin 3 (DMX+)
  - B (Data-) → Pin 2 (DMX-)
  - GND → Pin 1 (Common)

**Important Notes:**
- Use twisted pair cable for DMX signals
- Add 120Ω terminator at last device
- Keep RS485 module close to MCU
- Use separate power for fixtures

### Port 2 Setup
Same as Port 1 but with different GPIOs:
- TX: GPIO19
- RX: GPIO18
- DIR: GPIO20

## Pre-Test Checklist

- [ ] Firmware compiled and flashed successfully
- [ ] RS485 transceivers properly wired
- [ ] DMX cables connected
- [ ] Power supply stable (5V)
- [ ] Serial monitor connected (115200 baud)
- [ ] Test equipment ready

## Test Procedures

### Test 1: Component Initialization

**Objective**: Verify DMX handler initializes correctly.

**Procedure:**
1. Flash firmware and open serial monitor
2. Watch for initialization logs

**Expected Output:**
```
I (1234) main: Initializing DMX handler...
I (1235) dmx_handler: Initializing DMX handler...
I (1236) dmx_handler: DMX handler initialized successfully
I (1237) main: Configuring DMX ports...
I (1238) dmx_handler: Configuring port 1: mode=1, universe=0
I (1239) dmx_handler: Port 1 configured successfully
```

**Pass Criteria:**
- ✅ No error messages
- ✅ All ports configured successfully
- ✅ System continues to run

---

### Test 2: DMX Output - Basic Functionality

**Objective**: Verify DMX output transmits correctly.

**Setup:**
- Configure Port 1 in DMX_MODE_OUTPUT
- Connect DMX tester or fixture
- Set universe to 0

**Procedure:**
1. Configure port for DMX output:
```c
port_config_t config = {
    .mode = DMX_MODE_OUTPUT,
    .universe_primary = 0,
    .rdm_enabled = false
};
dmx_handler_configure_port(DMX_PORT_1, &config);
dmx_handler_start_port(DMX_PORT_1);
```

2. Set test pattern:
```c
// Set channels 1-3 to 255, 128, 64
dmx_handler_set_channel(DMX_PORT_1, 1, 255);
dmx_handler_set_channel(DMX_PORT_1, 2, 128);
dmx_handler_set_channel(DMX_PORT_1, 3, 64);
```

3. Measure with DMX tester

**Expected Results:**
- Channel 1: 255 (100%)
- Channel 2: 128 (50%)
- Channel 3: 64 (25%)
- Update rate: ~44 Hz
- No frame errors

**Pass Criteria:**
- ✅ DMX tester shows correct values
- ✅ Frame rate between 40-44 Hz
- ✅ No flickering on fixture
- ✅ Values stable for 1 minute

---

### Test 3: DMX Output - All Channels

**Objective**: Test full 512-channel transmission.

**Procedure:**
1. Create test pattern:
```c
uint8_t dmx_data[512];
for (int i = 0; i < 512; i++) {
    dmx_data[i] = (i % 256);
}
dmx_handler_send_dmx(DMX_PORT_1, dmx_data);
```

2. Verify with DMX analyzer

**Expected Results:**
- All 512 channels transmitted
- Pattern matches: Ch1=0, Ch2=1, ..., Ch256=255, Ch257=0, ...
- Frame rate: ~44 Hz
- Frame size: 513 bytes (start code + 512 channels)

**Pass Criteria:**
- ✅ All channels present
- ✅ Values correct
- ✅ No missing frames
- ✅ Timing accurate

---

### Test 4: DMX Output - Blackout

**Objective**: Verify blackout function.

**Procedure:**
1. Set some channels to non-zero values:
```c
dmx_handler_set_channel(DMX_PORT_1, 1, 255);
dmx_handler_set_channel(DMX_PORT_1, 2, 128);
```

2. Wait 5 seconds

3. Call blackout:
```c
dmx_handler_blackout(DMX_PORT_1);
```

4. Verify with DMX tester

**Expected Results:**
- Before blackout: Ch1=255, Ch2=128
- After blackout: All channels = 0
- Transition immediate (< 1 frame time)

**Pass Criteria:**
- ✅ All channels become 0
- ✅ Fixture turns off
- ✅ DMX continues transmitting (not disabled)

---

### Test 5: DMX Output - Frame Rate

**Objective**: Verify DMX refresh rate accuracy.

**Equipment:** Oscilloscope or logic analyzer

**Procedure:**
1. Connect oscilloscope to TX pin (GPIO17)
2. Trigger on DMX break
3. Measure time between breaks

**Expected Results:**
- Break-to-break time: ~23ms (±1ms)
- Frequency: ~43-44 Hz
- Break duration: 88-120 μs
- MAB duration: 8-16 μs

**Pass Criteria:**
- ✅ Frame rate 40-44 Hz
- ✅ Timing within DMX512 spec
- ✅ Stable over 5 minutes

---

### Test 6: DMX Input - Reception

**Objective**: Verify DMX input reception.

**Setup:**
- Configure Port 1 in DMX_MODE_INPUT
- Connect DMX controller as source

**Procedure:**
1. Configure port for input:
```c
port_config_t config = {
    .mode = DMX_MODE_INPUT,
    .universe_primary = 0
};
dmx_handler_configure_port(DMX_PORT_1, &config);
dmx_handler_start_port(DMX_PORT_1);
```

2. Register callback:
```c
static void on_dmx_rx(uint8_t port, const uint8_t *data, size_t size, void *user_data) {
    ESP_LOGI("TEST", "Received DMX: Ch1=%d, Ch2=%d, Ch3=%d", 
             data[0], data[1], data[2]);
}
dmx_handler_register_rx_callback(DMX_PORT_1, on_dmx_rx, NULL);
```

3. Send DMX from controller with known values

**Expected Results:**
- Callback triggered for each frame
- Values match transmitted data
- Frame rate matches source

**Pass Criteria:**
- ✅ Callback triggered
- ✅ Values correct
- ✅ No frame loss for 1 minute

---

### Test 7: DMX Input - Read Function

**Objective**: Test synchronous read function.

**Procedure:**
1. Configure port for input (as Test 6)
2. Send DMX with test pattern
3. Read data:
```c
uint8_t dmx_data[512];
esp_err_t ret = dmx_handler_read_dmx(DMX_PORT_1, dmx_data, 1000);
if (ret == ESP_OK) {
    ESP_LOGI("TEST", "Read OK: Ch1=%d", dmx_data[0]);
}
```

**Expected Results:**
- Read returns ESP_OK
- Data matches source
- Timeout works if no data

**Pass Criteria:**
- ✅ Successful read within 1 second
- ✅ Data correct
- ✅ Timeout returns ESP_ERR_TIMEOUT if source disabled

---

### Test 8: Dual Port Operation

**Objective**: Verify both ports work independently.

**Setup:**
- Port 1: DMX Output
- Port 2: DMX Input (or second Output)

**Procedure:**
1. Configure both ports:
```c
// Port 1: Output
port_config_t config1 = {.mode = DMX_MODE_OUTPUT, .universe_primary = 0};
dmx_handler_configure_port(DMX_PORT_1, &config1);
dmx_handler_start_port(DMX_PORT_1);

// Port 2: Output
port_config_t config2 = {.mode = DMX_MODE_OUTPUT, .universe_primary = 1};
dmx_handler_configure_port(DMX_PORT_2, &config2);
dmx_handler_start_port(DMX_PORT_2);
```

2. Set different values:
```c
dmx_handler_set_channel(DMX_PORT_1, 1, 255);
dmx_handler_set_channel(DMX_PORT_2, 1, 128);
```

3. Verify with 2 DMX testers

**Expected Results:**
- Port 1 Ch1: 255
- Port 2 Ch1: 128
- Both transmitting at ~44 Hz
- No crosstalk between ports

**Pass Criteria:**
- ✅ Both ports active
- ✅ Independent data
- ✅ No interference
- ✅ Both ~44 Hz

---

### Test 9: Port Mode Switching

**Objective**: Test switching between modes.

**Procedure:**
1. Start in OUTPUT mode
2. Send some data
3. Stop port:
```c
dmx_handler_stop_port(DMX_PORT_1);
```

4. Reconfigure to INPUT:
```c
port_config_t config = {.mode = DMX_MODE_INPUT, .universe_primary = 0};
dmx_handler_configure_port(DMX_PORT_1, &config);
dmx_handler_start_port(DMX_PORT_1);
```

5. Verify input works

**Expected Results:**
- Output stops cleanly
- Input starts receiving
- No crashes or errors

**Pass Criteria:**
- ✅ Smooth transition
- ✅ No memory leaks
- ✅ Both modes work after switch

---

### Test 10: Statistics and Status

**Objective**: Verify status reporting.

**Procedure:**
1. Start port in OUTPUT mode
2. Let run for 1 minute
3. Get status:
```c
dmx_port_status_t status;
dmx_handler_get_port_status(DMX_PORT_1, &status);
ESP_LOGI("TEST", "Frames sent: %lu", status.stats.frames_sent);
```

**Expected Results:**
- Frames sent: ~2640 (44 Hz × 60 seconds)
- Mode correct
- Is_active = true
- Universe correct

**Pass Criteria:**
- ✅ Frame count reasonable (~44 Hz)
- ✅ Status fields accurate
- ✅ Counter increments

---

### Test 11: RDM Discovery (Basic)

**Objective**: Test RDM discovery initialization.

**Setup:**
- Port 1 in RDM_MODE_MASTER
- RDM-capable fixture connected

**Procedure:**
1. Configure for RDM:
```c
port_config_t config = {
    .mode = DMX_MODE_RDM_MASTER,
    .universe_primary = 0,
    .rdm_enabled = true
};
dmx_handler_configure_port(DMX_PORT_1, &config);
dmx_handler_start_port(DMX_PORT_1);
```

2. Start discovery:
```c
ESP_LOGI("TEST", "Starting RDM discovery...");
esp_err_t ret = dmx_handler_rdm_discover(DMX_PORT_1);
ESP_LOGI("TEST", "Discovery result: %s", esp_err_to_name(ret));
```

**Expected Results:**
- Discovery starts without error
- Function returns ESP_OK
- (Full RDM testing requires library integration)

**Pass Criteria:**
- ✅ No errors starting discovery
- ✅ Port remains active
- ✅ DMX continues after RDM

---

### Test 12: Long-Term Stability

**Objective**: Verify stable operation over time.

**Procedure:**
1. Configure port for OUTPUT
2. Set test pattern
3. Let run for 24 hours
4. Monitor for errors

**Monitoring:**
- Check serial logs every hour
- Verify DMX tester shows stable output
- Check CPU temperature
- Monitor frame counters

**Expected Results:**
- No errors in logs
- DMX output stable
- Frame counter increasing linearly
- Memory usage stable

**Pass Criteria:**
- ✅ No crashes
- ✅ No error messages
- ✅ Output stable
- ✅ Frame rate consistent
- ✅ Memory not increasing

---

### Test 13: Error Handling

**Objective**: Test error conditions.

**Test Cases:**

**13.1 Invalid Port Number:**
```c
esp_err_t ret = dmx_handler_start_port(99);
assert(ret == ESP_ERR_INVALID_ARG);
```

**13.2 Not Initialized:**
```c
dmx_handler_deinit();
esp_err_t ret = dmx_handler_start_port(DMX_PORT_1);
assert(ret == ESP_ERR_INVALID_STATE);
```

**13.3 Invalid Channel:**
```c
esp_err_t ret = dmx_handler_set_channel(DMX_PORT_1, 513, 100);
assert(ret == ESP_ERR_INVALID_ARG);
```

**Pass Criteria:**
- ✅ All return correct error codes
- ✅ No crashes
- ✅ Error messages logged

---

### Test 14: Performance Under Load

**Objective**: Measure CPU and memory usage.

**Procedure:**
1. Enable both ports in OUTPUT mode
2. Update DMX data rapidly:
```c
for (int i = 0; i < 1000; i++) {
    uint8_t value = (i % 256);
    dmx_handler_set_channel(DMX_PORT_1, 1, value);
    dmx_handler_set_channel(DMX_PORT_2, 1, value);
    vTaskDelay(pdMS_TO_TICKS(10));
}
```

3. Monitor CPU usage and free heap

**Expected Results:**
- CPU usage < 10%
- Heap usage stable
- DMX output not affected
- Frame rate maintained

**Pass Criteria:**
- ✅ CPU < 10%
- ✅ No memory leaks
- ✅ Frame rate stable
- ✅ No frame drops

---

## Test Results Template

```
TEST REPORT: DMX Handler Component
Date: ___________
Tester: ___________
Firmware Version: ___________

Test 1: Component Initialization          [ PASS / FAIL ]
Test 2: DMX Output - Basic               [ PASS / FAIL ]
Test 3: DMX Output - All Channels        [ PASS / FAIL ]
Test 4: DMX Output - Blackout            [ PASS / FAIL ]
Test 5: DMX Output - Frame Rate          [ PASS / FAIL ]
Test 6: DMX Input - Reception            [ PASS / FAIL ]
Test 7: DMX Input - Read Function        [ PASS / FAIL ]
Test 8: Dual Port Operation              [ PASS / FAIL ]
Test 9: Port Mode Switching              [ PASS / FAIL ]
Test 10: Statistics and Status           [ PASS / FAIL ]
Test 11: RDM Discovery (Basic)           [ PASS / FAIL ]
Test 12: Long-Term Stability             [ PASS / FAIL ]
Test 13: Error Handling                  [ PASS / FAIL ]
Test 14: Performance Under Load          [ PASS / FAIL ]

Overall Result: [ PASS / FAIL ]

Notes:
_________________________________
_________________________________
_________________________________
```

## Troubleshooting

### Issue: No DMX Output

**Symptoms:** DMX tester shows no signal

**Checks:**
1. Verify RS485 wiring (especially DE/RE pin)
2. Check GPIO pins with multimeter
3. Verify DMX cable and termination
4. Check port is started and active
5. Verify mode is OUTPUT or RDM_MASTER

**Debug:**
```c
dmx_port_status_t status;
dmx_handler_get_port_status(DMX_PORT_1, &status);
ESP_LOGI("DEBUG", "Port active: %d, Mode: %d", status.is_active, status.mode);
ESP_LOGI("DEBUG", "Frames sent: %lu", status.stats.frames_sent);
```

### Issue: DMX Input Not Receiving

**Symptoms:** Callback not triggered, read returns timeout

**Checks:**
1. Verify RX pin connection
2. Check source is transmitting
3. Verify DMX cable polarity (A/B, +/-)
4. Check callback is registered
5. Verify port mode is INPUT

**Debug:**
```c
// Check if frames being received
dmx_port_status_t status;
dmx_handler_get_port_status(DMX_PORT_1, &status);
ESP_LOGI("DEBUG", "Frames received: %lu, Errors: %lu", 
         status.stats.frames_received, status.stats.error_count);
```

### Issue: Incorrect DMX Values

**Symptoms:** Values don't match expected

**Checks:**
1. Verify channel numbering (1-based vs 0-based)
2. Check DMX address on fixture
3. Verify universe assignment
4. Check for interference (use terminator)

### Issue: Low Frame Rate

**Symptoms:** Frame rate < 40 Hz

**Checks:**
1. Check CPU load (other tasks?)
2. Verify task priority (should be 10)
3. Check FreeRTOS tick rate (should be 1000 Hz)
4. Look for blocking operations

### Issue: RDM Not Working

**Note:** Full RDM support requires esp-dmx library integration.

**For basic testing:**
1. Verify port mode is RDM_MASTER
2. Check direction pin switching
3. Verify RDM device is powered
4. Check for DMX terminator

## Performance Benchmarks

Target specifications:

| Metric | Target | Acceptable | Critical |
|--------|--------|------------|----------|
| DMX Frame Rate | 44 Hz | 40-44 Hz | > 35 Hz |
| Output Latency | < 1 ms | < 2 ms | < 5 ms |
| Input Latency | < 2 ms | < 5 ms | < 10 ms |
| CPU Usage (2 ports) | < 5% | < 10% | < 20% |
| Memory Usage | 13 KB | 15 KB | 20 KB |
| Frame Error Rate | 0% | < 0.1% | < 1% |

## Compliance Testing

For production validation, test against:
- DMX512 Standard (ANSI E1.11)
- RDM Standard (ANSI E1.20)

Use certified DMX test equipment for compliance verification.

## References

- [DMX512 Standard](https://tsp.esta.org/tsp/documents/docs/ANSI-ESTA_E1-11_2008R2018.pdf)
- [RDM Standard](https://tsp.esta.org/tsp/documents/docs/ANSI-ESTA_E1-20_2010.pdf)
- [ESP-DMX Library Docs](https://github.com/someweisguy/esp-dmx)
- [Component README](README.md)
