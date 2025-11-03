export type SensorType = "DALLAS_TEMP" | "WATER_METER" | "UNKNOWN" | number;
export type LogLevel = "VERBOSE" | "DEBUG" | "INFO" | "WARNING" | "ERROR";

export interface SensorData {
  type: SensorType;
  connected: boolean;
  temperature_f: number;
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
  water_flow_error_timeout_seconds?: number;
  log_level?: LogLevel | string;
  pulses_per_gallon?: number;
  water_meter_timeout_seconds?: number;
}