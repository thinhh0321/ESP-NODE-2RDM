# Bugs and Fixes Report

**Date:** 2025-12-27  
**Version:** Pre-Phase 8 Code Review

## Summary

This document details bugs found during code review and their fixes. All issues have been categorized by severity and component.

---

## Critical Issues (Must Fix Before Release)

### 1. Source IP Not Passed to Merge Engine

**Location:** `main/main.c` lines 66, 95  
**Severity:** HIGH  
**Impact:** Merge engine cannot properly track and identify sources

**Problem:**
```c
// Art-Net callback
uint32_t source_ip = 0;  // Placeholder - should be extracted from packet

// sACN callback  
uint32_t source_ip = 0;  // Placeholder
```

**Root Cause:**
- Callbacks don't receive source IP from receiver modules
- Callback function signatures don't include source IP parameter
- Receivers have the information but don't pass it

**Solution:**
1. Extend callback signatures to include source IP/address
2. Update artnet_receiver and sacn_receiver to pass source address
3. Update main.c callbacks to use actual source IP

**Status:** FIXED (See PR commits)

---

## Medium Priority Issues

### 2. RDM Functions Not Implemented

**Location:** `components/dmx_handler/dmx_handler.c` lines 632, 687, 709  
**Severity:** MEDIUM  
**Impact:** RDM discovery and parameter control unavailable

**Problem:**
```c
// TODO: Implement RDM discovery using esp-dmx API
// TODO: Implement RDM GET using esp-dmx API  
// TODO: Implement RDM SET using esp-dmx API
```

**Root Cause:**
- RDM functionality depends on esp-dmx library integration
- API stubs exist but no implementation

**Solution:**
- Implement using esp-dmx library when integrated
- For now: Document as known limitation
- Add to Phase 8 integration tasks

**Status:** DOCUMENTED - Deferred to Phase 8

---

### 3. Web Server Config Update Not Implemented

**Location:** `components/web_server/web_server.c` line 486  
**Severity:** MEDIUM  
**Impact:** Cannot update configuration via REST API

**Problem:**
```c
// TODO: Apply configuration changes
// This would update config_manager with new values
```

**Root Cause:**
- Stub accepts JSON but doesn't parse or apply changes
- config_manager API exists but not called

**Solution:**
1. Parse incoming JSON configuration
2. Validate parameters
3. Call config_manager update functions
4. Return proper error codes

**Status:** FIXED (See PR commits)

---

### 4. WebSocket DMX Command Handling Missing

**Location:** `components/web_server/web_server.c` line 766  
**Severity:** LOW  
**Impact:** Cannot send DMX test commands via WebSocket

**Problem:**
```c
// TODO: Handle DMX test commands from client
```

**Root Cause:**
- WebSocket framework complete but command parsing not implemented
- No specification for command format

**Solution:**
1. Define JSON command format
2. Implement command parser
3. Add DMX test channel setter
4. Document API in web interface

**Status:** FIXED (See PR commits)

---

## Code Quality Issues

### 5. Potential Memory Leak in WebSocket Handler

**Location:** `components/web_server/web_server.c` line 757  
**Severity:** LOW  
**Impact:** Memory leak if malloc succeeds but recv fails

**Problem:**
```c
uint8_t *buf = malloc(ws_pkt.len + 1);
if (buf) {
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret == ESP_OK) {
        // ... process ...
    }
    free(buf);  // Only freed if malloc succeeded
}
```

**Analysis:**
- Actually NOT a bug - free() is called regardless of recv result
- Code is correct but could be more explicit

**Solution:**
- No changes needed, code is correct
- Add comment for clarity

**Status:** VERIFIED SAFE

---

### 6. Inconsistent Error Handling

**Location:** Multiple components  
**Severity:** LOW  
**Impact:** Inconsistent error messages and recovery

**Problem:**
- Some functions return ESP_FAIL, others ESP_ERR_*
- Error logging varies (LOGE vs LOGW vs LOGI)
- Some errors not logged at all

**Solution:**
1. Standardize on ESP_ERR_* return codes
2. Use consistent log levels:
   - LOGE for errors that prevent operation
   - LOGW for recoverable issues
   - LOGI for important state changes
3. Always log errors before returning

**Status:** IMPROVED (Standardized in key functions)

---

## Design Improvements

### 7. Mutex Cleanup on Error Paths

**Location:** Various components  
**Severity:** LOW  
**Impact:** Potential deadlock if error occurs while holding mutex

**Problem:**
```c
xSemaphoreTake(mutex, portMAX_DELAY);
if (some_error) {
    return ESP_ERR_*;  // Mutex not released!
}
xSemaphoreGive(mutex);
```

**Review Findings:**
- Checked all components with mutexes
- Most code properly releases mutexes
- Few edge cases found and fixed

**Status:** REVIEWED AND FIXED

---

### 8. Magic Numbers in Code

**Location:** Multiple files  
**Severity:** LOW  
**Impact:** Reduces code maintainability

**Examples:**
```c
vTaskDelay(pdMS_TO_TICKS(23));  // Why 23?
uint8_t max_connections = 4;     // Why 4?
```

**Solution:**
- Define constants with meaningful names
- Add comments explaining values
- Document assumptions

**Status:** DOCUMENTED

---

## Security Considerations

### 9. No Input Validation in Web Server

**Location:** `components/web_server/web_server.c`  
**Severity:** MEDIUM  
**Impact:** Potential buffer overflow or injection attacks

**Problem:**
- Port numbers from URI not validated (could be negative, >2, etc.)
- JSON size not limited before parsing
- No rate limiting on requests

**Solution:**
1. Validate all inputs from HTTP requests
2. Limit JSON payload size
3. Add range checks for numeric parameters
4. Sanitize string inputs

**Status:** IMPROVED (Added validation)

---

### 10. No Authentication on Web Interface

**Location:** `components/web_server/`  
**Severity:** HIGH (for production)  
**Impact:** Anyone can control device

**Problem:**
- All endpoints publicly accessible
- No password protection
- No API keys

**Solution:**
- Add HTTP Basic Authentication
- Implement API key system
- Add configurable password
- Consider HTTPS for production

**Status:** DOCUMENTED (Future enhancement)

---

## Testing Gaps

### 11. No Automated Tests

**Location:** Entire project  
**Severity:** MEDIUM  
**Impact:** Difficult to verify functionality, risk of regressions

**Problem:**
- No unit tests for components
- No integration tests
- Only manual testing available

**Solution:**
1. Create test framework using Unity
2. Add unit tests for critical functions
3. Add integration test scenarios
4. Document manual test procedures

**Status:** TEST GUIDE CREATED

---

## Documentation Improvements Needed

### 12. Missing API Examples

**Location:** Component headers  
**Severity:** LOW  
**Impact:** Harder for developers to use APIs

**Solution:**
- Add usage examples in header comments
- Create integration examples
- Document common pitfalls

**Status:** IMPROVED

---

## Performance Considerations

### 13. DMX Output Task Timing

**Location:** `main/main.c` line 128  
**Severity:** LOW  
**Impact:** Slight DMX timing jitter

**Problem:**
```c
vTaskDelay(pdMS_TO_TICKS(23));  // ~44Hz target
```

**Analysis:**
- Task delay is approximate
- Actual rate depends on execution time
- May drift over time

**Solution:**
- Use absolute timing (vTaskDelayUntil)
- Measure actual frame rate
- Add statistics

**Status:** NOTED (Good enough for V1)

---

### 14. JSON Parsing in Request Handler

**Location:** `components/web_server/web_server.c`  
**Severity:** LOW  
**Impact:** Blocks server thread during parsing

**Problem:**
- Large JSON configs block HTTP server
- No streaming parser
- Full JSON held in memory

**Solution:**
- Limit JSON size (already done)
- Consider streaming parser for future
- Current implementation acceptable for small configs

**Status:** ACCEPTABLE FOR V1

---

## Summary Statistics

| Severity | Count | Fixed | Remaining |
|----------|-------|-------|-----------|
| Critical | 1     | 1     | 0         |
| High     | 1     | 0     | 1 (deferred) |
| Medium   | 4     | 2     | 2         |
| Low      | 8     | 4     | 4         |
| **Total**| **14**| **7** | **7**     |

---

## Fixes Applied in This PR

1. ✅ Added source IP tracking to merge engine
2. ✅ Extended protocol receiver callbacks with source address
3. ✅ Implemented config update in web server
4. ✅ Implemented WebSocket DMX command handling
5. ✅ Added input validation in web server
6. ✅ Improved error handling consistency
7. ✅ Fixed mutex cleanup in error paths

---

## Deferred Items (Future Releases)

1. 📋 RDM implementation (Phase 8 - requires esp-dmx library)
2. 📋 Authentication system (Phase 9 - security hardening)
3. 📋 Automated testing framework (Phase 9)
4. 📋 Performance optimization (Phase 9)

---

## Testing Verification

All fixes have been:
- ✅ Code reviewed
- ✅ Compiled successfully
- ✅ Syntax checked
- ⏳ Hardware testing pending (Phase 8)

---

## Next Steps

1. Complete this PR merge
2. Update TESTING_GUIDE.md with new test cases
3. Begin Phase 8 hardware integration
4. Validate all fixes on real hardware
5. Performance testing and optimization

---

**Reviewed by:** GitHub Copilot Agent  
**Review Date:** 2025-12-27  
**Status:** Ready for merge pending hardware testing
