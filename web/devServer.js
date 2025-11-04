import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import express from "express";
import { createServer as createViteServer } from "vite";
import { randomInt } from "node:crypto";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// Mock data for Coop Controller system
const mockSettings = {
  ssid: "MyHomeWiFi",
  ap_mode: true,
  enabled: false,
  has_connected: false,
  
  // Coop controller settings with temperature hysteresis
  temp_threshold_on_f: 34.0,      // Temperature to turn pump ON
  temp_threshold_off_f: 36.0,     // Temperature to turn pump OFF
  pump_on_time_seconds: 150,       
  pump_off_time_seconds: 300,      
  // pump_auto_mode: true,            // Auto mode enabled
  light_auto_mode: false,           // Light auto mode disabled
  light_on_hour: 6,               // Light on at 6 AM
  light_off_hour: 20,              // Light off at 8 PM
  log_level: 'INFO',
  water_flow_error_timeout_seconds: 20, // 20 seconds
  water_meter_timeout_seconds: 300, // 5 minutes timeout
  pulses_per_gallon: 450.0,        // Water meter calibration
  wifi_led_enabled: true,              // WiFi status LED enabled
  buzzer_enabled: true,           // Buzzer enabled
  buzzer_type: 'ACTIVE'           // Buzzer type
};

const mockVersionInfo = {
  firmware_version: "1.0.0-mock",
  chip_family: "ESP32-mock",
  build_date: "2023-01-01",
  build_time: "12:00:00",
};

// Mock pump state
let mockPumpState = {
  state: 'AUTO',
  is_active: false,
  temperature_f: 35.0,
  temperature_below_threshold: false,
  flow_error: false,
  current_cycle_time: 0,
  time_until_next_switch: 300,
  total_on_time: 0,
  total_off_time: 0,
  total_cycles: 0
};

// Mock sensor data
const mockSensorData = {
  sensor1: {
    type: 'DALLAS_TEMP', // Dallas Temperature
    connected: true,
    temperature_f: 32.5,
    flow_rate: 0,
    pulse_count: 0,
    last_pulse_time: 0,
    actively_connected: false,
    status: 'Dallas Temperature Sensor - Connected (32.5°F)'
  },
  sensor2: {
    type: 'WATER_METER', // Water Meter
    connected: true,
    temperature_f: null, // Not detected
    flow_rate: 1.2,
    pulse_count: 150,
    last_pulse_time: 45,
    actively_connected: true,
    status: 'Water Meter - Connected (Active: 1.2 GPM)'
  }
};

// Mock logs data
const mockLogs = {
  logs: [
    {
      uuid: "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
      timestamp: 1698765432,
      message: "System initialization complete"
    },
    {
      uuid: "b2c3d4e5-f6a7-8901-bcde-f12345678901",
      timestamp: 1698765492,
      message: "Pump turned on - temperature below threshold"
    },
    {
      uuid: "c3d4e5f6-a7b8-9012-cdef-012345678901",
      timestamp: 1698765552,
      message: "Water meter calibration set to 450.0 pulses per gallon"
    }
  ]
};

const getSensorStatus = () => ({
  ...mockSensorData,
  pump: mockPumpState,
  system: {
    temp_threshold_on_f: mockSettings.temp_threshold_on_f,
    temp_threshold_off_f: mockSettings.temp_threshold_off_f,
    pump_on_time_seconds: mockSettings.pump_on_time_seconds,
    pump_off_time_seconds: mockSettings.pump_off_time_seconds,
    // pump_auto_mode: mockSettings.pump_auto_mode, // Auto mode enabled
    light_auto_mode: mockSettings.light_auto_mode,          // Light auto mode disabled
    light_on_hour: mockSettings.light_on_hour,              // Light on at 6 AM
    light_off_hour: mockSettings.light_off_hour,            // Light off at 8 PM
    water_meter_timeout_seconds: mockSettings.water_meter_timeout_seconds
  },
  buzzer: {
    enabled: mockSettings.buzzer_enabled || true,
    buzzer_type: mockSettings.buzzer_type || 'ACTIVE',
    has_active_alert: false,
    current_alert_type: null,
    silence_remaining_ms: 0
  }
});

async function createServer() {
  const app = express();
  const vite = await createViteServer({
    server: { middlewareMode: true },
    // don't include Vite's default HTML handling middlewares
    appType: "spa",
  });

  app.use("/get_settings", async (req, res) => {
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify(mockSettings));
  });

  app.use("/logs", async (req, res) => {
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify(mockLogs));
  });

  app.use("/system_status", async (req, res) => {
    const uptimeSeconds = Math.floor(Date.now() / 1000) % 86400; // Mock uptime within a day
    const days = Math.floor(uptimeSeconds / 86400);
    const hours = Math.floor((uptimeSeconds % 86400) / 3600);
    const minutes = Math.floor((uptimeSeconds % 3600) / 60);
    const seconds = uptimeSeconds % 60;
    
    let formatted = "";
    if (days > 0) formatted += days + "d ";
    if (hours > 0 || days > 0) formatted += hours + "h ";
    if (minutes > 0 || hours > 0 || days > 0) formatted += minutes + "m ";
    formatted += seconds + "s";
    
    const systemStatus = {
      heap_free: 150000,
      heap_size: 300000,
      heap_used_percent: 50.0,
      uptime_seconds: uptimeSeconds,
      uptime_formatted: formatted,
      chip_model: "ESP32-WROOM-32",
      cpu_freq_mhz: 240,
      flash_size: 4194304,
      wifi_rssi: -45,
      wifi_ssid: "MyHomeWiFi"
    };
    
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify(systemStatus));
  });

  app.use("/version", async (req, res) => {
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify(mockVersionInfo));
  });

  app.use("/update", async (req, res) => {
    res.setHeader("Content-Type", "text/html");
    res.end(elegantOTAHTML);
  });

  app.use("/update_settings", express.json(), async (req, res) => {
    try {
      const settings = req.body;
      
      // Update mock settings
      if (settings.temp_threshold_on_f !== undefined) {
        mockSettings.temp_threshold_on_f = settings.temp_threshold_on_f;
      }
      if (settings.temp_threshold_off_f !== undefined) {
        mockSettings.temp_threshold_off_f = settings.temp_threshold_off_f;
      }
      if (settings.water_flow_error_timeout_seconds !== undefined) {
        mockSettings.water_flow_error_timeout_seconds = settings.water_flow_error_timeout_seconds;
      }
      if (settings.pump_on_time_seconds !== undefined) {
        mockSettings.pump_on_time_seconds = settings.pump_on_time_seconds;
      }
      if (settings.pump_off_time_seconds !== undefined) {
        mockSettings.pump_off_time_seconds = settings.pump_off_time_seconds;
      }
      // if (settings.pump_auto_mode !== undefined) {
      //   mockSettings.pump_auto_mode = settings.pump_auto_mode;
      // }
      if (settings.light_auto_mode !== undefined) {
        mockSettings.light_auto_mode = settings.light_auto_mode;
      }
      if (settings.light_on_hour !== undefined) {
        mockSettings.light_on_hour = settings.light_on_hour;
      }
      // debug_enabled removed - incoming debug_enabled values are ignored
      if (settings.light_off_hour !== undefined) {
        mockSettings.light_off_hour = settings.light_off_hour;
      }
      if (settings.pulses_per_gallon !== undefined) {
        mockSettings.pulses_per_gallon = settings.pulses_per_gallon;
      }
      if (settings.water_meter_timeout_seconds !== undefined) {
        mockSettings.water_meter_timeout_seconds = settings.water_meter_timeout_seconds;
      }
      
      if (settings.wifi_led_enabled !== undefined) {
        mockSettings.wifi_led_enabled = settings.wifi_led_enabled;
      }
      
      res.setHeader("Content-Type", "application/json");
      res.end(JSON.stringify({ success: true }));
    } catch (error) {
      res.setHeader("Content-Type", "application/json");
      res.end(JSON.stringify({ success: false, error: error.message }));
    }
  });

  app.use("/sensor_status", async (req, res) => {
    res.setHeader("Content-Type", "application/json");
    res.end(JSON.stringify(getSensorStatus()));
  });

  app.use("/pump/:action", async (req, res) => {
    const action = req.params.action;
    try {
      switch (action) {
        case 'on':
          mockPumpState.state = 'ON';
          mockPumpState.is_active = true;
          mockPumpState.flow_error = false;
          break;
        case 'off':
          mockPumpState.state = 'OFF';
          mockPumpState.is_active = false;
          break;
        case 'auto':
          mockPumpState.state = 'AUTO';
          // Simulate auto logic based on temperature
          mockPumpState.temperature_below_threshold = mockPumpState.temperature_f < mockSettings.temp_threshold_on_f;
          mockPumpState.is_active = mockPumpState.temperature_below_threshold;
          break;
        case 'force_cycle':
          if (mockPumpState.state === 'AUTO') {
            mockPumpState.is_active = !mockPumpState.is_active;
            mockPumpState.current_cycle_time = mockPumpState.is_active ? mockSettings.pump_on_time_seconds : mockSettings.pump_off_time_seconds;
          }
          break;
        case 'clear_error':
          mockPumpState.flow_error = false;
          break;
        case 'reset_stats':
          mockPumpState.total_on_time = 0;
          mockPumpState.total_off_time = 0;
          mockPumpState.total_cycles = 0;
          break;
        default:
          return res.status(400).json({ error: 'Invalid action' });
      }
      res.setHeader("Content-Type", "application/json");
      res.end(JSON.stringify({ success: true }));
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  app.use("/water/reset/:sensor", async (req, res) => {
    const sensor = parseInt(req.params.sensor);
    if (sensor === 1 || sensor === 2) {
      mockSensorData[`sensor${sensor}`].pulse_count = 0;
      res.setHeader("Content-Type", "application/json");
      res.end(JSON.stringify({ success: true }));
    } else {
      res.status(400).json({ error: 'Invalid sensor' });
    }
  });

  app.use("/factory_reset", express.urlencoded({ extended: true }), async (req, res) => {
    try {
      // Check for confirmation parameter
      if (!req.body || req.body.confirm !== 'RESET') {
        return res.status(400).json({ error: 'Missing or invalid confirmation parameter' });
      }
      
      // Reset mock settings to defaults
      mockSettings.ssid = "";
      mockSettings.passwd = "";
      mockSettings.ap_mode = true;
      mockSettings.enabled = true;
      mockSettings.has_connected = false;
      mockSettings.temp_threshold_on_f = 34.0;
      mockSettings.temp_threshold_off_f = 36.0;
      mockSettings.pump_on_time_seconds = 300;
      mockSettings.pump_off_time_seconds = 600;
      mockSettings.pump_auto_mode = true;
      mockSettings.light_auto_mode = false;
      mockSettings.light_on_hour = 6;
      mockSettings.light_off_hour = 21;
      mockSettings.log_level = "INFO";
      mockSettings.water_flow_error_timeout_seconds = 120;
      mockSettings.wifi_max_retries = 5;
      mockSettings.wifi_retry_delay_seconds = 30;
      mockSettings.wifi_ap_duration_minutes = 10;
      mockSettings.watchdog_timeout_seconds = 30;
      mockSettings.pulses_per_gallon = 450.0;
      mockSettings.water_meter_timeout_seconds = 300;
      mockSettings.wifi_led_enabled = true;
      
      res.setHeader("Content-Type", "text/plain");
      res.end("Factory reset complete. Device will restart in 3 seconds.");
    } catch (error) {
      res.status(500).json({ error: error.message });
    }
  });

  app.use("/settings/backup", async (req, res) => {
    res.setHeader("Content-Type", "application/json");
    res.setHeader("Content-Disposition", "attachment; filename=coop_controller_settings.json");
    res.end(JSON.stringify(mockSettings, null, 2));
  });

  app.use("/settings/restore", express.json(), async (req, res) => {
    try {
      const settings = req.body;
      
      // Basic validation - check for required fields
      if (typeof settings !== 'object' || settings === null) {
        return res.status(400).json({ success: false, error: 'Invalid JSON format' });
      }
      
      // Update mock settings with validation
      if (settings.temp_threshold_on_f !== undefined && typeof settings.temp_threshold_on_f === 'number') {
        mockSettings.temp_threshold_on_f = settings.temp_threshold_on_f;
      }
      if (settings.temp_threshold_off_f !== undefined && typeof settings.temp_threshold_off_f === 'number') {
        mockSettings.temp_threshold_off_f = settings.temp_threshold_off_f;
      }
      if (settings.pump_on_time_seconds !== undefined && typeof settings.pump_on_time_seconds === 'number') {
        mockSettings.pump_on_time_seconds = settings.pump_on_time_seconds;
      }
      if (settings.pump_off_time_seconds !== undefined && typeof settings.pump_off_time_seconds === 'number') {
        mockSettings.pump_off_time_seconds = settings.pump_off_time_seconds;
      }
      if (settings.light_auto_mode !== undefined && typeof settings.light_auto_mode === 'boolean') {
        mockSettings.light_auto_mode = settings.light_auto_mode;
      }
      if (settings.light_on_hour !== undefined && typeof settings.light_on_hour === 'number') {
        mockSettings.light_on_hour = settings.light_on_hour;
      }
      if (settings.light_off_hour !== undefined && typeof settings.light_off_hour === 'number') {
        mockSettings.light_off_hour = settings.light_off_hour;
      }
      if (settings.water_flow_error_timeout_seconds !== undefined && typeof settings.water_flow_error_timeout_seconds === 'number') {
        mockSettings.water_flow_error_timeout_seconds = settings.water_flow_error_timeout_seconds;
      }
      if (settings.pulses_per_gallon !== undefined && typeof settings.pulses_per_gallon === 'number') {
        mockSettings.pulses_per_gallon = settings.pulses_per_gallon;
      }
      if (settings.water_meter_timeout_seconds !== undefined && typeof settings.water_meter_timeout_seconds === 'number') {
        mockSettings.water_meter_timeout_seconds = settings.water_meter_timeout_seconds;
      }
      if (settings.wifi_led_enabled !== undefined && typeof settings.wifi_led_enabled === 'boolean') {
        mockSettings.wifi_led_enabled = settings.wifi_led_enabled;
      }
      if (settings.buzzer_enabled !== undefined && typeof settings.buzzer_enabled === 'boolean') {
        mockSettings.buzzer_enabled = settings.buzzer_enabled;
      }
      if (settings.buzzer_type !== undefined && typeof settings.buzzer_type === 'string') {
        mockSettings.buzzer_type = settings.buzzer_type;
      }
      
      res.setHeader("Content-Type", "application/json");
      res.end(JSON.stringify({ success: true, message: "Settings restored successfully" }));
    } catch (error) {
      res.status(500).json({ success: false, error: error.message });
    }
  });

  app.use(vite.middlewares);
  app.listen(5173);
  console.log("Server is running on http://localhost:5173");
}

createServer();

const elegantOTAHTML = `
<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>ElegantOTA Lite</title>
    <style>
      * {
        margin: 0;
        padding: 0;
        box-sizing: border-box;
      }
      body {
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        background: #f3f4f6;
        color: #374151;
        min-height: 100vh;
        display: flex;
        flex-direction: column;
        align-items: center;
        padding: 2rem;
      }
      .container {
        background: white;
        padding: 2rem;
        border-radius: 0.5rem;
        box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
        max-width: 400px;
        width: 100%;
        text-align: center;
      }
      h1 {
        margin-bottom: 2rem;
        color: #1f2937;
      }
      .upload-area {
        border: 2px dashed #d1d5db;
        border-radius: 0.5rem;
        padding: 2rem;
        margin-bottom: 1rem;
        cursor: pointer;
        transition: border-color 0.2s;
      }
      .upload-area:hover {
        border-color: #9ca3af;
      }
      .btn {
        background: #3b82f6;
        color: white;
        border: none;
        padding: 0.75rem 1.5rem;
        border-radius: 0.375rem;
        cursor: pointer;
        font-size: 0.875rem;
        font-weight: 500;
        transition: background-color 0.2s;
      }
      .btn:hover {
        background: #2563eb;
      }
      .progress {
        width: 100%;
        height: 0.5rem;
        background: #e5e7eb;
        border-radius: 0.25rem;
        margin: 1rem 0;
        overflow: hidden;
      }
      .progress-bar {
        height: 100%;
        background: #3b82f6;
        transition: width 0.3s;
        width: 0%;
      }
      .hidden {
        display: none;
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Firmware Update</h1>
      <div class="upload-area" onclick="document.getElementById('fileInput').click()">
        <p>Click to select firmware file (.bin)</p>
        <input type="file" id="fileInput" class="hidden" accept=".bin" onchange="handleFileSelect(this.files[0])">
      </div>
      <div class="progress hidden" id="progress">
        <div class="progress-bar" id="progressBar"></div>
      </div>
      <div id="result" class="hidden">
        <p id="resultText"></p>
      </div>
    </div>

    <script>
      function handleFileSelect(file) {
        if (!file) return;
        
        const progress = document.getElementById('progress');
        const progressBar = document.getElementById('progressBar');
        const result = document.getElementById('result');
        const resultText = document.getElementById('resultText');
        
        progress.classList.remove('hidden');
        result.classList.add('hidden');
        
        // Simulate progress
        let currentProgress = 0;
        const interval = setInterval(() => {
          currentProgress += Math.random() * 30;
          if (currentProgress > 100) currentProgress = 100;
          
          progressBar.style.width = currentProgress + '%';
          
          if (currentProgress >= 100) {
            clearInterval(interval);
            result.classList.remove('hidden');
            resultText.textContent = 'Update completed successfully!';
            resultText.style.color = '#059669';
          }
        }, 200);
      }
    </script>
  </body>
</html>
`;
