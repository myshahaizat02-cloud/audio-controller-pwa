/* ═══════════════════════════════════════════════════════════════
   AUDIO CONTROLLER - APP
   MQTT WebSocket client for ESP32 control
   ═══════════════════════════════════════════════════════════════ */

// MQTT Configuration
const MQTT_BROKER = 'wss://broker.hivemq.com:8884/mqtt';
const TOPIC_CONTROL = 'audio/control';
const TOPIC_STATUS = 'audio/status';
const TOPIC_SCHEDULE = 'audio/schedule';

let client = null;
let isConnected = false;

// ═══════════════════════════════════════════════════════════════
// MQTT Connection
// ═══════════════════════════════════════════════════════════════

function connectMQTT() {
    const clientId = 'web_' + Math.random().toString(16).substr(2, 8);

    addLog('Connecting to MQTT broker...');

    client = mqtt.connect(MQTT_BROKER, {
        clientId: clientId,
        clean: true,
        connectTimeout: 4000,
        reconnectPeriod: 5000
    });

    client.on('connect', () => {
        isConnected = true;
        updateConnectionStatus(true);
        addLog('Connected to MQTT broker', 'success');

        // Subscribe to status topic
        client.subscribe(TOPIC_STATUS, (err) => {
            if (!err) {
                addLog('Subscribed to status updates', 'success');
            }
        });
    });

    client.on('message', (topic, message) => {
        const payload = message.toString();
        console.log('Received:', topic, payload);

        if (topic === TOPIC_STATUS) {
            handleStatusUpdate(payload);
        }
    });

    client.on('error', (err) => {
        console.error('MQTT Error:', err);
        addLog('Connection error: ' + err.message, 'error');
    });

    client.on('close', () => {
        isConnected = false;
        updateConnectionStatus(false);
        addLog('Disconnected from broker', 'error');
    });

    client.on('reconnect', () => {
        addLog('Reconnecting...');
    });
}

// ═══════════════════════════════════════════════════════════════
// UI Updates
// ═══════════════════════════════════════════════════════════════

function updateConnectionStatus(connected) {
    const statusEl = document.getElementById('connectionStatus');
    const textEl = statusEl.querySelector('.status-text');

    if (connected) {
        statusEl.classList.add('connected');
        textEl.textContent = 'Connected';
        document.getElementById('connStatus').textContent = 'Online';
    } else {
        statusEl.classList.remove('connected');
        textEl.textContent = 'Disconnected';
        document.getElementById('connStatus').textContent = 'Offline';
    }
}

function updatePowerStatus(isOn) {
    const powerIcon = document.getElementById('powerIcon');
    const powerLabel = document.getElementById('powerLabel');
    const audioStatus = document.getElementById('audioStatus');

    if (isOn) {
        powerIcon.classList.add('on');
        powerIcon.classList.remove('off');
        powerLabel.textContent = 'ON';
        powerLabel.style.color = '#10b981';
        audioStatus.textContent = 'Playing 10kHz';
        audioStatus.style.color = '#10b981';
    } else {
        powerIcon.classList.remove('on');
        powerIcon.classList.add('off');
        powerLabel.textContent = 'OFF';
        powerLabel.style.color = '#71717a';
        audioStatus.textContent = 'Silent';
        audioStatus.style.color = '#71717a';
    }
}

function updateMode(mode) {
    const modeBadge = document.getElementById('modeBadge');
    modeBadge.textContent = mode;

    if (mode === 'MANUAL') {
        modeBadge.classList.add('manual');
    } else {
        modeBadge.classList.remove('manual');
    }
}

function handleStatusUpdate(payload) {
    try {
        const data = JSON.parse(payload);

        // Update power status
        if (data.audio) {
            updatePowerStatus(data.audio === 'ON');
        }

        // Update mode
        if (data.mode) {
            updateMode(data.mode);
        }

        // Update IP
        if (data.ip) {
            document.getElementById('ipAddress').textContent = data.ip;
        }

        // Update time
        if (data.time) {
            document.getElementById('deviceTime').textContent = data.time;
        }

        // Update schedule display
        if (data.schedule) {
            document.getElementById('scheduleInfo').textContent =
                `Auto ON at ${data.schedule.split('-')[0]}, OFF at ${data.schedule.split('-')[1]}`;
        }

        // Update last update time
        const now = new Date();
        document.getElementById('lastUpdate').textContent =
            now.toLocaleTimeString('en-GB', { hour: '2-digit', minute: '2-digit' });

        // Log status
        if (data.status === 'audio_on') {
            addLog('🔊 Tweeter turned ON', 'success');
        } else if (data.status === 'audio_off') {
            addLog('🔇 Tweeter turned OFF');
        } else if (data.status === 'online') {
            addLog('📡 Device online', 'success');
        }

    } catch (e) {
        console.error('Error parsing status:', e);
    }
}

// ═══════════════════════════════════════════════════════════════
// Commands
// ═══════════════════════════════════════════════════════════════

function sendCommand(command) {
    if (!isConnected) {
        addLog('Not connected to broker', 'error');
        return;
    }

    client.publish(TOPIC_CONTROL, command);
    addLog(`📤 Sent command: ${command}`);

    // Visual feedback
    const btn = document.getElementById('btn' + command.charAt(0).toUpperCase() + command.slice(1).toLowerCase());
    if (btn) {
        btn.style.transform = 'scale(0.95)';
        setTimeout(() => {
            btn.style.transform = '';
        }, 100);
    }
}

function setSchedule() {
    if (!isConnected) {
        addLog('Not connected to broker', 'error');
        return;
    }

    const onTime = document.getElementById('onTime').value;
    const duration = document.getElementById('duration').value;

    if (!onTime || !duration) {
        addLog('Please set ON time and duration', 'error');
        return;
    }

    // Validate duration (1-60 minutes)
    const durationNum = parseInt(duration);
    if (durationNum < 1 || durationNum > 60) {
        addLog('Duration must be 1-60 minutes', 'error');
        return;
    }

    const schedule = `${onTime},${duration}`;
    client.publish(TOPIC_SCHEDULE, schedule);

    addLog(`📅 Schedule set: ON at ${onTime} for ${duration} minutes`, 'success');
    document.getElementById('scheduleInfo').textContent =
        `Auto ON at ${onTime} for ${duration} minutes`;
}

// ═══════════════════════════════════════════════════════════════
// Activity Log
// ═══════════════════════════════════════════════════════════════

function addLog(message, type = '') {
    const container = document.getElementById('logContainer');
    const now = new Date();
    const time = now.toLocaleTimeString('en-GB', {
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });

    const logItem = document.createElement('div');
    logItem.className = 'log-item';
    logItem.innerHTML = `
        <span class="log-time">${time}</span>
        <span class="log-message ${type}">${message}</span>
    `;

    container.insertBefore(logItem, container.firstChild);

    // Keep only last 50 logs
    while (container.children.length > 50) {
        container.removeChild(container.lastChild);
    }
}

function clearLog() {
    const container = document.getElementById('logContainer');
    container.innerHTML = '';
    addLog('Log cleared');
}

// ═══════════════════════════════════════════════════════════════
// Initialization
// ═══════════════════════════════════════════════════════════════

document.addEventListener('DOMContentLoaded', () => {
    // Connect to MQTT
    connectMQTT();

    // Update local time display
    setInterval(() => {
        const now = new Date();
        // Update footer or any local time display if needed
    }, 1000);

    // Schedule toggle handler
    document.getElementById('scheduleToggle').addEventListener('change', (e) => {
        if (e.target.checked) {
            sendCommand('AUTO');
            addLog('Switched to AUTO mode (schedule enabled)');
        } else {
            addLog('Schedule disabled - using manual control');
        }
    });
});

// Service Worker Registration (for PWA)
if ('serviceWorker' in navigator) {
    window.addEventListener('load', () => {
        navigator.serviceWorker.register('sw.js')
            .then(reg => console.log('SW registered'))
            .catch(err => console.log('SW registration failed'));
    });
}
