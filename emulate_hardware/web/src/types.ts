/**
 * TypeScript type definitions for the Hardware Emulator Web UI
 */

export type MotorDirection = 'STOPPED' | 'OPENING' | 'CLOSING' | 'BRAKE';
export type DoorState = 'OPEN' | 'CLOSED' | 'OPENING' | 'CLOSING' | 'STOPPED' | 'UNKNOWN';

/**
 * Monitored signals from the main controller
 */
export interface MonitoredSignals {
  pump_active: boolean;
  light_active: boolean;
  light_brightness: number;  // 0-100
  motor_pos_active: boolean;
  motor_neg_active: boolean;
  motor_direction: MotorDirection;
  buzzer_active: boolean;
  buzzer_duration_ms: number;
  wifi_led_active: boolean;
}

/**
 * Emulated outputs sent to main controller
 */
export interface EmulatedOutputs {
  water_flow_enabled: boolean;
  flow_rate_gpm: number;
  channel1_pulses: number;
  channel2_pulses: number;
  door_state: DoorState;
  door_position: number;  // 0-100
  hall_open_active: boolean;
  hall_close_active: boolean;
  manual_switch_pressed: boolean;
  door_fault_active: boolean;
}

/**
 * Emulator configuration
 */
export interface EmulatorConfig {
  door_travel_time_ms: number;
  auto_simulate_door: boolean;
  pulses_per_gallon: number;
  flow_rate_gpm: number;
  auto_generate_pulses: boolean;
  inject_door_fault: boolean;
  simulate_frozen_line: boolean;
  simulate_door_stuck: boolean;
}

/**
 * Full emulator status response
 */
export interface EmulatorStatus {
  monitored: MonitoredSignals;
  emulated: EmulatedOutputs;
  config: EmulatorConfig;
}

/**
 * System status response
 */
export interface SystemStatus {
  uptime_seconds: number;
  uptime_formatted: string;
  heap_free: number;
  heap_size: number;
  heap_used_percent: number;
  chip_model: string;
  cpu_freq_mhz: number;
  flash_size: number;
  firmware_version: string;
  hostname: string;
  wifi_ssid?: string;
  wifi_rssi?: number;
  wifi_ip?: string;
}

/**
 * Settings response
 */
export interface EmulatorSettings {
  ssid: string;
  ap_mode: boolean;
  door_travel_time_ms: number;
  pulses_per_gallon: number;
  flow_rate_gpm: number;
  auto_simulate_door: boolean;
  auto_generate_pulses: boolean;
  log_level: string;
  hostname: string;
  firmware_version: string;
}

/**
 * API response wrapper
 */
export interface ApiResponse {
  success: boolean;
  message?: string;
  error?: string;
}
