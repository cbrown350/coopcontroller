export type SensorType = "DALLAS_TEMP" | "WATER_METER" | "UNKNOWN" | number;
export type LogLevel = "VERBOSE" | "DEBUG" | "INFO" | "WARNING" | "ERROR";

export interface SensorData {
  type: SensorType;
  connected: boolean;
  temperature_f: number | null;
  flow_rate: number;
  pulse_count: number;
  last_pulse_time: number;
  actively_connected: boolean;
  status: string;
}

export interface PumpStatus {
  state: string;
  is_active: boolean;
  temperature_f: number;
  temperature_below_threshold: boolean;
  flow_error: boolean;
  current_cycle_time: number;
  time_until_next_switch: number;
  time_until_retry: number;
  total_on_time: number;
  total_off_time: number;
  total_cycles: number;
  pump_off_flow_detected?: boolean;
  scheduled_cycle_active?: boolean;
  time_until_next_scheduled?: number;
}

export interface SystemSettings {
  temp_threshold_on_f: number;
  temp_threshold_off_f: number;
  pump_on_time_seconds: number;
  pump_off_time_seconds: number;
  light_auto_mode: boolean;
  light_on_hour: number;
  light_off_hour: number;
  water_meter_timeout_seconds: number;
}

export interface FullSensorStatus {
  sensor1: SensorData;
  sensor2: SensorData;
  pump: PumpStatus;
  system: SystemSettings;
  buzzer: BuzzerStatus;
  light?: LightStatus;
}

export interface BuzzerStatus {
  enabled: boolean;
  buzzer_type: string;
  has_active_alert: boolean;
  current_alert_type?: string;
  silence_remaining_ms?: number;
}

export interface LightStatus {
  state: string; // "OFF" | "ON" | "FADING_IN" | "FADING_OUT" | "FAULT"
  current_brightness: number;
  target_brightness: number;
  max_brightness: number;
  fade_progress: number;
  auto_mode: boolean;
  test_mode: boolean;
  total_on_time: number;
  total_cycles: number;
  next_scheduled_action: string;
}

export interface SystemStatus {
  heap_free: number;
  heap_size: number;
  heap_used_percent: number;
  uptime_seconds: number;
  uptime_formatted: string;
  chip_model: string;
  cpu_freq_mhz: number;
  flash_size: number;
  wifi_rssi?: number;
  wifi_ssid?: string;
  wifi_ip?: string;
  wifi_mac?: string;
  wifi_bssid?: string;
}

export interface DoorStatus {
  state: string;
  position: string;
  progress: number;
  auto_mode: boolean;
  auto_open_enabled: boolean;
  auto_close_enabled: boolean;
  test_mode: boolean;
  lockout_enabled: boolean;
  hall_open: boolean;
  hall_closed: boolean;
  total_open_time: number;
  total_close_time: number;
  total_cycles: number;
  next_scheduled_action: string;
  weather_postponed?: boolean;
  auto_calc_timeout_enabled: boolean;
  recommended_open_timeout: number;
  recommended_close_timeout: number;
}

export interface Settings {
  ssid?: string;
  ap_mode?: boolean;
  temp_threshold_on_f?: number;
  temp_threshold_off_f?: number;
  pump_on_time_seconds?: number;
  pump_off_time_seconds?: number;
  light_auto_mode?: boolean;
  light_on_hour?: number;
  light_off_hour?: number;
  light_brightness_percent?: number;
  light_transition_duration_minutes?: number;
  water_flow_error_timeout_seconds?: number;
  log_level?: LogLevel | string;
  pulses_per_gallon?: number;
  water_meter_timeout_seconds?: number;
  wifi_led_enabled?: boolean;
  buzzer_enabled?: boolean;
  buzzer_type?: string;
  water_meter_per_pulse_calculation_enabled?: boolean;
  pump_off_flow_monitoring_enabled?: boolean;
  pump_off_flow_grace_period_seconds?: number;
  pump_off_flow_pulse_threshold?: number;
  pump_min_daily_cycles_enabled?: boolean;
  pump_min_daily_cycles?: number;
  pump_min_cycle_run_seconds?: number;
  api_auth_enabled?: boolean;
  api_username?: string;
  api_password?: string;
  door_open_timeout_seconds?: number;
  door_close_timeout_seconds?: number;
  door_auto_open_enabled?: boolean;
  door_auto_open_offset_minutes?: number;
  door_auto_open_days?: boolean[];
  door_auto_close_enabled?: boolean;
  door_auto_close_offset_minutes?: number;
  door_auto_close_days?: boolean[];
  door_lockout_enabled?: boolean;
  door_timeout_auto_calc_enabled?: boolean;
  wifi_bssid_preference?: string;
  syslog_server?: string;
  syslog_port?: number;
  flow_calculation_interval_seconds?: number;
  history_enabled?: boolean;
  history_temp_min_interval_seconds?: number;
  history_flow_min_interval_seconds?: number;
  history_buffer_size?: number;
  auto_update_enabled?: boolean;
  update_check_interval_hours?: number;
  weather_enabled?: boolean;
  weather_api_key?: string;
  weather_units?: string;
  weather_update_interval_minutes?: number;
  llm_enabled?: boolean;
  llm_provider_type?: string;
  llm_base_url?: string;
  llm_api_key?: string;
  llm_model?: string;
  llm_timeout_seconds?: number;
}

export interface WeatherCurrent {
  condition: string; // "GOOD" | "INCLEMENT" | "UNKNOWN"
  description: string;
  icon: string;
  temp?: number;
  feels_like?: number;
  humidity?: number;
  wind_speed?: number;
  pressure?: number;
  cloudiness?: number;
  fetch_time: number;
}

export interface WeatherForecast {
  dt: number;
  temp?: number;
  wind_speed?: number;
  precip_prob?: number;
  description: string;
}

export interface WeatherStatus {
  enabled: boolean;
  configured: boolean;
  units: string;
  gate_active: boolean;
  good_for_opening: boolean;
  decider?: string;
  decision_reason?: string;
  update_interval_minutes: number;
  successful_fetches: number;
  failed_fetches: number;
  last_error?: string;
  current?: WeatherCurrent;
  forecast?: WeatherForecast[];
}

export interface WeatherTestResult {
  success: boolean;
  error?: string;
  status?: WeatherStatus;
}

export interface LlmTestConnectionResult {
  success: boolean;
  error?: string;
}