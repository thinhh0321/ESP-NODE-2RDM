/**
 * ESP-NODE-2RDM Web Application
 * Single Page Application for device management
 */

const app = {
    ws: null,
    wsReconnectTimer: null,
    updateInterval: null,
    lastFrameCount: { port1: 0, port2: 0 },
    lastFrameTime: Date.now(),
    
    /**
     * Initialize application
     */
    init() {
        console.log('Initializing ESP-NODE-2RDM Web UI...');
        
        // Setup tab navigation
        this.setupTabs();
        
        // Setup DHCP checkbox handler
        this.setupDHCPToggle();
        
        // Load initial data
        this.loadSystemInfo();
        this.loadConfig();
        this.loadPortsStatus();
        
        // Setup periodic updates
        this.updateInterval = setInterval(() => {
            this.updateDashboard();
        }, 2000);
        
        // Try to connect WebSocket
        this.connectWebSocket();
        
        // Update connection status
        this.updateConnectionStatus(true);
    },
    
    /**
     * Setup tab navigation
     */
    setupTabs() {
        const tabs = document.querySelectorAll('.tab');
        const tabContents = document.querySelectorAll('.tab-content');
        
        tabs.forEach(tab => {
            tab.addEventListener('click', () => {
                const targetTab = tab.dataset.tab;
                
                // Remove active class from all tabs
                tabs.forEach(t => t.classList.remove('active'));
                tabContents.forEach(tc => tc.classList.remove('active'));
                
                // Add active class to clicked tab
                tab.classList.add('active');
                document.getElementById(targetTab).classList.add('active');
            });
        });
    },
    
    /**
     * Setup DHCP checkbox toggle
     */
    setupDHCPToggle() {
        const dhcpCheckbox = document.querySelector('input[name="dhcp"]');
        const staticIPDiv = document.querySelector('.static-ip');
        
        if (dhcpCheckbox) {
            dhcpCheckbox.addEventListener('change', (e) => {
                staticIPDiv.style.display = e.target.checked ? 'none' : 'block';
            });
        }
    },
    
    /**
     * Update connection status indicator
     */
    updateConnectionStatus(connected) {
        const indicator = document.getElementById('connectionStatus');
        const text = document.getElementById('connectionText');
        
        if (connected) {
            indicator.classList.add('connected');
            indicator.classList.remove('disconnected');
            text.textContent = 'Connected';
        } else {
            indicator.classList.remove('connected');
            indicator.classList.add('disconnected');
            text.textContent = 'Offline';
        }
    },
    
    /**
     * Connect to WebSocket
     */
    connectWebSocket() {
        try {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${window.location.host}/ws`;
            
            this.ws = new WebSocket(wsUrl);
            
            this.ws.onopen = () => {
                console.log('WebSocket connected');
                this.showToast('WebSocket connected', 'success');
                clearTimeout(this.wsReconnectTimer);
            };
            
            this.ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    this.handleWebSocketMessage(data);
                } catch (e) {
                    console.error('WebSocket message parse error:', e);
                }
            };
            
            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
            };
            
            this.ws.onclose = () => {
                console.log('WebSocket disconnected');
                this.ws = null;
                // Attempt reconnection after 5 seconds
                this.wsReconnectTimer = setTimeout(() => {
                    this.connectWebSocket();
                }, 5000);
            };
        } catch (e) {
            console.error('WebSocket connection error:', e);
        }
    },
    
    /**
     * Handle WebSocket messages
     */
    handleWebSocketMessage(data) {
        // Handle real-time updates from device
        console.log('WebSocket data:', data);
    },
    
    /**
     * Make API request
     */
    async apiRequest(url, options = {}) {
        try {
            const response = await fetch(url, {
                ...options,
                headers: {
                    'Content-Type': 'application/json',
                    ...options.headers
                }
            });
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            return await response.json();
        } catch (error) {
            console.error('API request failed:', error);
            this.showToast(`Request failed: ${error.message}`, 'error');
            throw error;
        }
    },
    
    /**
     * Load system information
     */
    async loadSystemInfo() {
        try {
            const data = await this.apiRequest('/api/system/info');
            
            // Update system info fields
            document.getElementById('sysFirmware').textContent = data.firmware_version || '--';
            document.getElementById('sysHardware').textContent = data.hardware || '--';
            document.getElementById('sysIDF').textContent = data.idf_version || '--';
            document.getElementById('sysFreeHeap').textContent = this.formatBytes(data.free_heap);
            document.getElementById('sysUptime').textContent = this.formatUptime(data.uptime_sec);
            
            // Also update dashboard
            document.getElementById('firmware').textContent = data.firmware_version || '--';
            document.getElementById('hardware').textContent = data.hardware || '--';
            document.getElementById('freeHeap').textContent = this.formatBytes(data.free_heap);
            document.getElementById('uptime').textContent = this.formatUptime(data.uptime_sec);
            
            this.updateConnectionStatus(true);
        } catch (error) {
            this.updateConnectionStatus(false);
        }
    },
    
    /**
     * Load configuration
     */
    async loadConfig() {
        try {
            const data = await this.apiRequest('/api/config');
            
            // Update port configs
            if (data.port1) {
                const port1Form = document.getElementById('port1Form');
                if (port1Form) {
                    port1Form.universe.value = data.port1.universe_primary || 0;
                    port1Form.mode.value = data.port1.mode || 1;
                }
                document.getElementById('port1MergeMode').value = data.port1.merge_mode || 0;
            }
            
            if (data.port2) {
                const port2Form = document.getElementById('port2Form');
                if (port2Form) {
                    port2Form.universe.value = data.port2.universe_primary || 1;
                    port2Form.mode.value = data.port2.mode || 1;
                }
                document.getElementById('port2MergeMode').value = data.port2.merge_mode || 0;
            }
        } catch (error) {
            console.error('Failed to load config:', error);
        }
    },
    
    /**
     * Load ports status
     */
    async loadPortsStatus() {
        try {
            const data = await this.apiRequest('/api/ports/status');
            
            if (Array.isArray(data) && data.length >= 2) {
                const port1 = data[0];
                const port2 = data[1];
                
                // Update port 1
                if (port1) {
                    document.getElementById('port1Mode').textContent = this.getModeName(port1.mode);
                    document.getElementById('port1Universe').textContent = port1.universe || '--';
                    document.getElementById('port1Frames').textContent = port1.frames_sent || 0;
                }
                
                // Update port 2
                if (port2) {
                    document.getElementById('port2Mode').textContent = this.getModeName(port2.mode);
                    document.getElementById('port2Universe').textContent = port2.universe || '--';
                    document.getElementById('port2Frames').textContent = port2.frames_sent || 0;
                }
            }
        } catch (error) {
            console.error('Failed to load ports status:', error);
        }
    },
    
    /**
     * Update dashboard with latest data
     */
    async updateDashboard() {
        try {
            // Update system info
            const sysInfo = await this.apiRequest('/api/system/info');
            document.getElementById('freeHeap').textContent = this.formatBytes(sysInfo.free_heap);
            document.getElementById('uptime').textContent = this.formatUptime(sysInfo.uptime_sec);
            document.getElementById('sysFreeHeap').textContent = this.formatBytes(sysInfo.free_heap);
            document.getElementById('sysUptime').textContent = this.formatUptime(sysInfo.uptime_sec);
            
            // Update network status
            try {
                const netStatus = await this.apiRequest('/api/network/status');
                document.getElementById('netMode').textContent = netStatus.mode || 'Unknown';
                document.getElementById('ipAddress').textContent = netStatus.ip || 'Not connected';
                document.getElementById('netStatus').textContent = netStatus.connected ? 'Connected' : 'Disconnected';
            } catch (e) {
                // Network API might not be implemented
                document.getElementById('netMode').textContent = 'N/A';
                document.getElementById('ipAddress').textContent = window.location.hostname;
                document.getElementById('netStatus').textContent = 'Connected';
            }
            
            // Update protocol stats
            const stats = await this.apiRequest('/api/system/stats');
            
            if (stats.artnet) {
                document.getElementById('artnetPackets').textContent = stats.artnet.packets || 0;
                document.getElementById('artnetDmx').textContent = stats.artnet.dmx_packets || 0;
            }
            
            if (stats.sacn) {
                document.getElementById('sacnPackets').textContent = stats.sacn.packets || 0;
                document.getElementById('sacnData').textContent = stats.sacn.data_packets || 0;
            }
            
            // Update ports status
            const portsStatus = await this.apiRequest('/api/ports/status');
            
            if (Array.isArray(portsStatus) && portsStatus.length >= 2) {
                const port1 = portsStatus[0];
                const port2 = portsStatus[1];
                
                // Calculate refresh rate
                const now = Date.now();
                const timeDelta = (now - this.lastFrameTime) / 1000;
                
                if (port1) {
                    const framesDelta1 = port1.frames_sent - this.lastFrameCount.port1;
                    const rate1 = timeDelta > 0 ? (framesDelta1 / timeDelta).toFixed(1) : '0.0';
                    
                    document.getElementById('port1Frames').textContent = port1.frames_sent || 0;
                    document.getElementById('port1Rate').textContent = rate1 + ' Hz';
                    this.lastFrameCount.port1 = port1.frames_sent;
                }
                
                if (port2) {
                    const framesDelta2 = port2.frames_sent - this.lastFrameCount.port2;
                    const rate2 = timeDelta > 0 ? (framesDelta2 / timeDelta).toFixed(1) : '0.0';
                    
                    document.getElementById('port2Frames').textContent = port2.frames_sent || 0;
                    document.getElementById('port2Rate').textContent = rate2 + ' Hz';
                    this.lastFrameCount.port2 = port2.frames_sent;
                }
                
                this.lastFrameTime = now;
            }
            
            // Update DMX channel visualization
            this.updateDMXVisualization();
            
            this.updateConnectionStatus(true);
        } catch (error) {
            this.updateConnectionStatus(false);
        }
    },
    
    /**
     * Update DMX channel visualization with simulated or real data
     */
    async updateDMXVisualization() {
        try {
            // For now, simulate DMX data since we don't have a real endpoint yet
            // In production, this would fetch from /api/ports/1/dmx and /api/ports/2/dmx
            
            // Simulate realistic DMX data for demonstration
            // Replace with actual API call when backend is ready
            const port1Data = this.generateSimulatedDMX(1);
            const port2Data = this.generateSimulatedDMX(2);
            
            // Update Port 1 channels
            for (let i = 1; i <= 8; i++) {
                this.updateDMXChannel(1, i, port1Data[i - 1]);
            }
            
            // Update Port 2 channels
            for (let i = 1; i <= 8; i++) {
                this.updateDMXChannel(2, i, port2Data[i - 1]);
            }
            
            // Update signal strength indicators
            this.updateSignalStrength(1, port1Data);
            this.updateSignalStrength(2, port2Data);
            
        } catch (error) {
            console.error('Failed to update DMX visualization:', error);
        }
    },
    
    /**
     * Update individual DMX channel display
     */
    updateDMXChannel(port, channel, value) {
        const barElement = document.getElementById(`dmx${port}-ch${channel}`);
        const pctElement = document.getElementById(`dmx${port}-ch${channel}-pct`);
        
        if (barElement && pctElement) {
            const percentage = ((value / 255) * 100).toFixed(0);
            const valueSpan = barElement.querySelector('.dmx-value');
            
            barElement.style.height = percentage + '%';
            
            // Change color for high values (>200)
            if (value > 200) {
                barElement.classList.add('high');
            } else {
                barElement.classList.remove('high');
            }
            
            if (valueSpan) {
                valueSpan.textContent = value;
            }
            
            pctElement.textContent = percentage + '%';
        }
    },
    
    /**
     * Update signal strength indicator
     */
    updateSignalStrength(port, dmxData) {
        const fillElement = document.getElementById(`port${port}SignalFill`);
        const textElement = document.getElementById(`port${port}SignalText`);
        
        if (fillElement && textElement) {
            // Calculate average signal strength (0-100%)
            const avgValue = dmxData.reduce((sum, val) => sum + val, 0) / dmxData.length;
            const strength = ((avgValue / 255) * 100).toFixed(0);
            
            fillElement.style.width = strength + '%';
            textElement.textContent = strength + '%';
            
            // Change color based on strength
            fillElement.classList.remove('weak', 'medium');
            if (strength < 30) {
                fillElement.classList.add('weak');
            } else if (strength < 70) {
                fillElement.classList.add('medium');
            }
        }
    },
    
    /**
     * Generate simulated DMX data for demonstration
     * In production, replace with actual API call to fetch real DMX data
     */
    generateSimulatedDMX(port) {
        // Generate semi-realistic DMX values that change over time
        const data = [];
        const time = Date.now() / 1000;
        
        for (let i = 0; i < 8; i++) {
            // Create different patterns for each channel
            let value;
            
            if (i === 0) {
                // CH1: Sine wave (0-255)
                value = Math.floor((Math.sin(time + port) + 1) * 127.5);
            } else if (i === 1) {
                // CH2: Fast pulse
                value = Math.floor((Math.sin(time * 2 + port) + 1) * 127.5);
            } else if (i === 2) {
                // CH3: Slow ramp
                value = Math.floor(((time * 0.5 + port) % 1) * 255);
            } else if (i === 3) {
                // CH4: Random with smoothing
                value = Math.floor(Math.random() * 128 + 64);
            } else if (i === 4) {
                // CH5: Triangle wave
                const t = (time + port) % 2;
                value = Math.floor(t < 1 ? t * 255 : (2 - t) * 255);
            } else if (i === 5) {
                // CH6: Step function
                value = Math.floor((time + port) % 2) < 1 ? 200 : 50;
            } else if (i === 6) {
                // CH7: Cosine wave
                value = Math.floor((Math.cos(time * 1.5 + port) + 1) * 127.5);
            } else {
                // CH8: Combination
                value = Math.floor((Math.sin(time) * Math.cos(time * 0.7 + port) + 1) * 127.5);
            }
            
            data.push(Math.max(0, Math.min(255, value)));
        }
        
        return data;
    },
    
    /**
     * Save network configuration
     */
    async saveNetworkConfig(event) {
        event.preventDefault();
        
        const formData = new FormData(event.target);
        const config = {
            network: {
                mode: formData.get('netMode'),
                wifi_ssid: formData.get('wifi_ssid'),
                wifi_password: formData.get('wifi_password'),
                use_dhcp: formData.get('dhcp') === 'on',
                static_ip: formData.get('static_ip'),
                gateway: formData.get('gateway'),
                netmask: formData.get('netmask')
            }
        };
        
        try {
            await this.apiRequest('/api/config', {
                method: 'POST',
                body: JSON.stringify(config)
            });
            
            this.showToast('Network configuration saved successfully', 'success');
        } catch (error) {
            this.showToast('Failed to save network configuration', 'error');
        }
        
        return false;
    },
    
    /**
     * Load network configuration
     */
    async loadNetworkConfig() {
        try {
            const config = await this.apiRequest('/api/config');
            
            if (config.network) {
                const form = document.getElementById('networkForm');
                if (form) {
                    form.netMode.value = config.network.mode || 'sta';
                    form.wifi_ssid.value = config.network.wifi_ssid || '';
                    form.wifi_password.value = '';  // Don't populate password
                    form.dhcp.checked = config.network.use_dhcp !== false;
                    form.static_ip.value = config.network.static_ip || '';
                    form.gateway.value = config.network.gateway || '';
                    form.netmask.value = config.network.netmask || '';
                    
                    // Update static IP visibility
                    document.querySelector('.static-ip').style.display = 
                        form.dhcp.checked ? 'none' : 'block';
                }
            }
            
            this.showToast('Network configuration loaded', 'info');
        } catch (error) {
            this.showToast('Failed to load network configuration', 'error');
        }
    },
    
    /**
     * Save port configuration
     */
    async savePortConfig(event, port) {
        event.preventDefault();
        
        const formData = new FormData(event.target);
        const config = {
            [`port${port}`]: {
                mode: parseInt(formData.get('mode')),
                universe_primary: parseInt(formData.get('universe')),
                priority: parseInt(formData.get('priority'))
            }
        };
        
        try {
            await this.apiRequest('/api/config', {
                method: 'POST',
                body: JSON.stringify(config)
            });
            
            this.showToast(`Port ${port} configuration saved`, 'success');
            this.loadPortsStatus();  // Refresh port status
        } catch (error) {
            this.showToast(`Failed to save Port ${port} configuration`, 'error');
        }
        
        return false;
    },
    
    /**
     * Save merge mode
     */
    async saveMergeMode(port, mode) {
        const config = {
            [`port${port}`]: {
                merge_mode: parseInt(mode)
            }
        };
        
        try {
            await this.apiRequest('/api/config', {
                method: 'POST',
                body: JSON.stringify(config)
            });
            
            this.showToast(`Port ${port} merge mode updated`, 'success');
        } catch (error) {
            this.showToast(`Failed to update merge mode`, 'error');
        }
    },
    
    /**
     * Blackout port
     */
    async blackoutPort(port) {
        try {
            await this.apiRequest(`/api/ports/${port}/blackout`, {
                method: 'POST'
            });
            
            this.showToast(`Port ${port} blackout activated`, 'warning');
        } catch (error) {
            this.showToast(`Failed to blackout Port ${port}`, 'error');
        }
    },
    
    /**
     * Start RDM discovery
     */
    async startRDMDiscovery() {
        const statusDiv = document.getElementById('rdmStatus');
        const tableBody = document.getElementById('rdmDevicesTable');
        
        statusDiv.textContent = 'Scanning for RDM devices...';
        statusDiv.style.color = 'var(--info)';
        
        try {
            // This is a placeholder - actual RDM discovery API needs to be implemented
            const devices = await this.apiRequest('/api/rdm/discover', {
                method: 'POST'
            });
            
            if (devices && devices.length > 0) {
                statusDiv.textContent = `Found ${devices.length} device(s)`;
                statusDiv.style.color = 'var(--success)';
                
                // Populate table
                tableBody.innerHTML = devices.map(device => `
                    <tr>
                        <td>${device.port}</td>
                        <td>${device.uid}</td>
                        <td>${device.label}</td>
                        <td>${device.dmx_address}</td>
                        <td>${device.manufacturer}</td>
                        <td>${device.model}</td>
                    </tr>
                `).join('');
            } else {
                statusDiv.textContent = 'No RDM devices found';
                statusDiv.style.color = 'var(--text-muted)';
                tableBody.innerHTML = '<tr><td colspan="6" class="no-data">No devices discovered</td></tr>';
            }
        } catch (error) {
            statusDiv.textContent = 'RDM discovery failed or not yet implemented';
            statusDiv.style.color = 'var(--danger)';
            this.showToast('RDM discovery is not yet implemented', 'warning');
        }
    },
    
    /**
     * Reboot device
     */
    async rebootDevice() {
        if (!confirm('Are you sure you want to reboot the device?')) {
            return;
        }
        
        try {
            await this.apiRequest('/api/system/restart', {
                method: 'POST'
            });
            
            this.showToast('Device is rebooting...', 'info');
            
            // Show countdown
            let countdown = 10;
            const countdownToast = setInterval(() => {
                countdown--;
                if (countdown <= 0) {
                    clearInterval(countdownToast);
                    window.location.reload();
                } else {
                    this.showToast(`Reconnecting in ${countdown} seconds...`, 'info');
                }
            }, 1000);
        } catch (error) {
            this.showToast('Failed to reboot device', 'error');
        }
    },
    
    /**
     * Factory reset
     */
    async factoryReset() {
        if (!confirm('Are you sure you want to factory reset? All settings will be lost!')) {
            return;
        }
        
        if (!confirm('This action cannot be undone. Continue?')) {
            return;
        }
        
        try {
            await this.apiRequest('/api/system/factory-reset', {
                method: 'POST'
            });
            
            this.showToast('Factory reset initiated', 'warning');
        } catch (error) {
            this.showToast('Factory reset not yet implemented', 'warning');
        }
    },
    
    /**
     * Upload firmware
     */
    async uploadFirmware(event) {
        event.preventDefault();
        
        const fileInput = document.getElementById('firmwareFile');
        const file = fileInput.files[0];
        
        if (!file) {
            this.showToast('Please select a firmware file', 'error');
            return false;
        }
        
        if (!file.name.endsWith('.bin')) {
            this.showToast('Invalid file type. Please select a .bin file', 'error');
            return false;
        }
        
        const progress = document.getElementById('uploadProgress');
        const progressBar = document.getElementById('uploadProgressBar');
        const progressText = document.getElementById('uploadProgressText');
        
        progress.style.display = 'block';
        progressBar.style.width = '0%';
        progressText.textContent = '0%';
        
        try {
            // Simulate upload progress (replace with actual OTA upload)
            for (let i = 0; i <= 100; i += 10) {
                await new Promise(resolve => setTimeout(resolve, 500));
                progressBar.style.width = i + '%';
                progressText.textContent = i + '%';
            }
            
            this.showToast('Firmware upload not yet implemented', 'warning');
            progress.style.display = 'none';
        } catch (error) {
            this.showToast('Firmware upload failed', 'error');
            progress.style.display = 'none';
        }
        
        return false;
    },
    
    /**
     * Show toast notification
     */
    showToast(message, type = 'info') {
        const container = document.getElementById('toastContainer');
        const toast = document.createElement('div');
        toast.className = `toast ${type}`;
        
        const header = document.createElement('div');
        header.className = 'toast-header';
        header.textContent = type.charAt(0).toUpperCase() + type.slice(1);
        
        const body = document.createElement('div');
        body.className = 'toast-body';
        body.textContent = message;
        
        toast.appendChild(header);
        toast.appendChild(body);
        container.appendChild(toast);
        
        // Auto remove after 5 seconds
        setTimeout(() => {
            toast.style.opacity = '0';
            setTimeout(() => {
                container.removeChild(toast);
            }, 300);
        }, 5000);
    },
    
    /**
     * Format bytes to human readable
     */
    formatBytes(bytes) {
        if (!bytes) return '--';
        const sizes = ['B', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(1024));
        return Math.round(bytes / Math.pow(1024, i) * 100) / 100 + ' ' + sizes[i];
    },
    
    /**
     * Format uptime to human readable
     */
    formatUptime(seconds) {
        if (!seconds) return '--';
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const secs = seconds % 60;
        
        if (days > 0) {
            return `${days}d ${hours}h ${minutes}m`;
        } else if (hours > 0) {
            return `${hours}h ${minutes}m ${secs}s`;
        } else if (minutes > 0) {
            return `${minutes}m ${secs}s`;
        } else {
            return `${secs}s`;
        }
    },
    
    /**
     * Get mode name from mode value
     */
    getModeName(mode) {
        const modes = {
            0: 'Disabled',
            1: 'DMX Output',
            2: 'DMX Input',
            3: 'RDM Master',
            4: 'RDM Responder'
        };
        return modes[mode] || 'Unknown';
    }
};

// Initialize app when DOM is loaded
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => app.init());
} else {
    app.init();
}

// Make app globally accessible for onclick handlers
window.app = app;
