/* ═══════════════════════════════════════════════════════════════
   AUDIO CONTROLLER - APP v3.0 (DFPlayer Edition)
   MQTT WebSocket client for ESP32 + DFPlayer control
   ═══════════════════════════════════════════════════════════════ */

// MQTT Configuration - MUST MATCH ESP32 SETTINGS
const MQTT_BROKER = 'wss://c13890d5b97c47339681ed1f71e7026f.s1.eu.hivemq.cloud:8884/mqtt';
const MQTT_USERNAME = 'dante';
const MQTT_PASSWORD = 'Dante1234';
const TOPIC_CONTROL = 'audio/control';
const TOPIC_STATUS = 'audio/status';
const TOPIC_SCHEDULE = 'audio/schedule';
const TOPIC_SOUND = 'audio/sound';
const TOPIC_ALARM = 'audio/alarm';

let client = null;
let isConnected = false;
let isPlaying = false;
let currentVolume = 25;

// ═══════════════════════════════════════════════════════════════
// MQTT Connection
// ═══════════════════════════════════════════════════════════════

function connectMQTT() {
    const clientId = 'web_' + Math.random().toString(16).substr(2, 8);

    addLog('Connecting to HiveMQ Cloud...');
    console.log('Connecting to:', MQTT_BROKER);

    client = mqtt.connect(MQTT_BROKER, {
        clientId: clientId,
        username: MQTT_USERNAME,
        password: MQTT_PASSWORD,
        clean: true,
        connectTimeout: 10000,
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
    const audioStatusBadge = document.getElementById('audioStatusBadge');
    const btnPlayPause = document.getElementById('btnPlayPause');

    if (isOn) {
        powerIcon.classList.add('on');
        powerIcon.classList.remove('off');
        powerLabel.textContent = 'ON';
        powerLabel.style.color = '#10b981';
        audioStatus.textContent = 'Playing';
        audioStatus.style.color = '#10b981';
        audioStatusBadge.textContent = 'ON';
        audioStatusBadge.style.color = '#10b981';
        btnPlayPause.textContent = '⏸️';
        isPlaying = true;
    } else {
        powerIcon.classList.remove('on');
        powerIcon.classList.add('off');
        powerLabel.textContent = 'OFF';
        powerLabel.style.color = '#71717a';
        audioStatus.textContent = 'Silent';
        audioStatus.style.color = '#71717a';
        audioStatusBadge.textContent = 'OFF';
        audioStatusBadge.style.color = '#71717a';
        btnPlayPause.textContent = '▶️';
        isPlaying = false;
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

        // Update volume
        if (data.volume !== undefined) {
            currentVolume = data.volume;
            document.getElementById('volumeSlider').value = data.volume;
            document.getElementById('volumeValue').textContent = data.volume;
            document.getElementById('volumeStatus').textContent = data.volume;
        }

        // Update track
        if (data.track !== undefined) {
            document.getElementById('currentTrack').textContent = data.track;
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
            addLog('🔊 Audio turned ON', 'success');
        } else if (data.status === 'audio_off') {
            addLog('🔇 Audio turned OFF');
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
    addLog(`📤 Sent: ${command.toUpperCase()}`);
}

function togglePower() {
    if (isPlaying) {
        sendCommand('OFF');
    } else {
        sendCommand('ON');
    }
}

function togglePlayPause() {
    if (isPlaying) {
        sendCommand('pause');
        addLog('⏸️ Paused');
    } else {
        sendCommand('play');
        addLog('▶️ Playing');
    }
}

function setVolume(value) {
    if (!isConnected) {
        addLog('Not connected to broker', 'error');
        return;
    }

    client.publish(TOPIC_CONTROL, `vol_${value}`);
    addLog(`🔊 Volume: ${value}`);
}

function updateVolumeDisplay(value) {
    document.getElementById('volumeValue').textContent = value;
}

function playTrack(trackNumber) {
    if (!isConnected) {
        addLog('Not connected to broker', 'error');
        return;
    }

    client.publish(TOPIC_SOUND, trackNumber.toString());
    addLog(`🎵 Playing track ${trackNumber}`, 'success');
}

// ═══════════════════════════════════════════════════════════════
// Alarm System
// ═══════════════════════════════════════════════════════════════

let alarms = JSON.parse(localStorage.getItem('audioAlarms')) || [];
let activeAlarmId = null;
const DAY_NAMES = ['S', 'M', 'T', 'W', 'T', 'F', 'S'];
const DAY_FULL = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

// Format time helper
function formatTime(h, m) {
    return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`;
}

// Add new alarm
function addAlarm() {
    const hour = parseInt(document.getElementById('alarmHour').value) || 0;
    const minute = parseInt(document.getElementById('alarmMinute').value) || 0;
    const label = document.getElementById('alarmLabel').value.trim();

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        addLog('Invalid time (0-23 hours, 0-59 minutes)', 'error');
        return;
    }

    // Get selected days
    const selectedDays = [];
    document.querySelectorAll('.day-btn.active').forEach(btn => {
        selectedDays.push(parseInt(btn.dataset.day));
    });

    const alarm = {
        id: Date.now(),
        hour,
        minute,
        label: label || 'Alarm',
        days: selectedDays,
        enabled: true
    };

    alarms.push(alarm);
    saveAlarms();
    renderAlarms();

    // Reset form
    document.getElementById('alarmLabel').value = '';
    document.querySelectorAll('.day-btn').forEach(btn => btn.classList.remove('active'));

    addLog(`⏰ Alarm added: ${formatTime(hour, minute)}`, 'success');

    // Send to ESP32 via MQTT
    syncAlarmsToESP32();
}

// Toggle alarm on/off
function toggleAlarm(id) {
    const alarm = alarms.find(a => a.id === id);
    if (alarm) {
        alarm.enabled = !alarm.enabled;
        saveAlarms();
        renderAlarms();
        addLog(`Alarm ${formatTime(alarm.hour, alarm.minute)} ${alarm.enabled ? 'enabled' : 'disabled'}`);
        syncAlarmsToESP32();
    }
}

// Delete alarm
function deleteAlarm(id) {
    const alarm = alarms.find(a => a.id === id);
    if (alarm) {
        alarms = alarms.filter(a => a.id !== id);
        saveAlarms();
        renderAlarms();
        addLog(`Alarm ${formatTime(alarm.hour, alarm.minute)} deleted`);
        syncAlarmsToESP32();
    }
}

// Save alarms to localStorage
function saveAlarms() {
    localStorage.setItem('audioAlarms', JSON.stringify(alarms));
}

// Sync alarms to ESP32 via MQTT
function syncAlarmsToESP32() {
    if (!isConnected) return;

    const enabledAlarms = alarms.filter(a => a.enabled);
    const scheduleData = enabledAlarms.map(a => {
        const daysStr = a.days.length > 0 ? a.days.join(',') : '*';
        return `${formatTime(a.hour, a.minute)}|${daysStr}`;
    }).join(';');

    client.publish(TOPIC_SCHEDULE, scheduleData);
}

// Render alarms list
function renderAlarms() {
    const container = document.getElementById('alarmsList');
    const emptyState = document.getElementById('emptyAlarms');

    // Clear existing cards
    container.querySelectorAll('.alarm-card').forEach(card => card.remove());

    if (alarms.length === 0) {
        emptyState.style.display = 'block';
        return;
    }

    emptyState.style.display = 'none';

    // Sort by time
    const sorted = [...alarms].sort((a, b) => {
        if (a.hour !== b.hour) return a.hour - b.hour;
        return a.minute - b.minute;
    });

    sorted.forEach(alarm => {
        const card = document.createElement('div');
        card.className = `alarm-card${alarm.enabled ? '' : ' disabled'}`;

        // Days badges
        let daysHtml = '<div class="alarm-card-days">';
        if (alarm.days.length === 0) {
            daysHtml += '<span class="alarm-day-badge active">Once</span>';
        } else {
            for (let i = 0; i < 7; i++) {
                const isActive = alarm.days.includes(i);
                daysHtml += `<span class="alarm-day-badge${isActive ? ' active' : ''}">${DAY_NAMES[i]}</span>`;
            }
        }
        daysHtml += '</div>';

        card.innerHTML = `
            <div class="alarm-card-info">
                <div class="alarm-card-time">${formatTime(alarm.hour, alarm.minute)}</div>
                <div class="alarm-card-label">${alarm.label}</div>
                ${daysHtml}
            </div>
            <div class="alarm-card-controls">
                <label class="alarm-toggle">
                    <input type="checkbox" ${alarm.enabled ? 'checked' : ''}>
                    <span class="alarm-toggle-slider"></span>
                </label>
                <button class="alarm-delete-btn">🗑️</button>
            </div>
        `;

        // Event listeners
        card.querySelector('.alarm-toggle input').addEventListener('change', () => toggleAlarm(alarm.id));
        card.querySelector('.alarm-delete-btn').addEventListener('click', () => deleteAlarm(alarm.id));

        container.appendChild(card);
    });
}

// Check alarms every second
function checkAlarms() {
    const now = new Date();
    const currentHour = now.getHours();
    const currentMinute = now.getMinutes();
    const currentSecond = now.getSeconds();
    const currentDay = now.getDay();

    if (currentSecond !== 0) return;

    alarms.forEach(alarm => {
        if (!alarm.enabled) return;

        if (alarm.hour === currentHour && alarm.minute === currentMinute) {
            if (alarm.days.length === 0 || alarm.days.includes(currentDay)) {
                triggerAlarm(alarm);
            }
        }
    });
}

// Trigger alarm
function triggerAlarm(alarm) {
    activeAlarmId = alarm.id;
    const timeStr = formatTime(alarm.hour, alarm.minute);

    document.getElementById('alarmModalTitle').textContent = alarm.label;
    document.getElementById('alarmModalTime').textContent = timeStr;
    document.getElementById('alarmModal').classList.add('active');

    // Turn ON audio via MQTT
    if (isConnected) {
        sendCommand('ON');
        addLog(`⏰ ALARM: ${alarm.label} - Audio ON`, 'success');
    }

    // Browser notification
    if ('Notification' in window && Notification.permission === 'granted') {
        new Notification(alarm.label, { body: `Alarm: ${timeStr}` });
    }
}

// Dismiss alarm
function dismissAlarm() {
    document.getElementById('alarmModal').classList.remove('active');

    // Turn OFF audio
    if (isConnected) {
        sendCommand('OFF');
    }

    // If it's a one-time alarm, disable it
    if (activeAlarmId) {
        const alarm = alarms.find(a => a.id === activeAlarmId);
        if (alarm && alarm.days.length === 0) {
            alarm.enabled = false;
            saveAlarms();
            renderAlarms();
        }
    }

    activeAlarmId = null;
    addLog('Alarm dismissed');
}

// Snooze alarm (5 minutes)
function snoozeAlarm() {
    document.getElementById('alarmModal').classList.remove('active');

    // Turn OFF audio temporarily
    if (isConnected) {
        sendCommand('OFF');
    }

    // Create snooze alarm
    const now = new Date();
    now.setMinutes(now.getMinutes() + 5);

    const originalAlarm = alarms.find(a => a.id === activeAlarmId);
    const snoozeAlarm = {
        id: Date.now(),
        hour: now.getHours(),
        minute: now.getMinutes(),
        label: `${originalAlarm ? originalAlarm.label : 'Alarm'} (Snoozed)`,
        days: [],
        enabled: true
    };

    alarms.push(snoozeAlarm);
    saveAlarms();
    renderAlarms();

    activeAlarmId = null;
    addLog(`⏰ Snoozed for 5 minutes`, 'success');
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
    connectMQTT();

    // Schedule toggle (AUTO/MANUAL mode)
    document.getElementById('scheduleToggle').addEventListener('change', (e) => {
        if (e.target.checked) {
            sendCommand('AUTO');
            addLog('Switched to AUTO mode');
        } else {
            sendCommand('MANUAL');
            addLog('Switched to MANUAL mode');
        }
    });

    // Alarm form - Add button
    document.getElementById('btnAddAlarm').addEventListener('click', addAlarm);

    // Day selector buttons
    document.querySelectorAll('.day-btn').forEach(btn => {
        btn.addEventListener('click', () => btn.classList.toggle('active'));
    });

    // Alarm modal buttons
    document.getElementById('btnDismiss').addEventListener('click', dismissAlarm);
    document.getElementById('btnSnooze').addEventListener('click', snoozeAlarm);

    // Input validation
    document.getElementById('alarmHour').addEventListener('input', (e) => {
        let val = parseInt(e.target.value);
        if (val < 0) e.target.value = 0;
        if (val > 23) e.target.value = 23;
    });

    document.getElementById('alarmMinute').addEventListener('input', (e) => {
        let val = parseInt(e.target.value);
        if (val < 0) e.target.value = 0;
        if (val > 59) e.target.value = 59;
    });

    // Check alarms every second
    setInterval(checkAlarms, 1000);

    // Request notification permission
    if ('Notification' in window && Notification.permission === 'default') {
        Notification.requestPermission();
    }

    // Render saved alarms
    renderAlarms();
});

// Service Worker Registration
if ('serviceWorker' in navigator) {
    window.addEventListener('load', () => {
        navigator.serviceWorker.register('sw.js')
            .then(reg => console.log('SW registered'))
            .catch(err => console.log('SW registration failed'));
    });
}
