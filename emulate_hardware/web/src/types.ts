/**
 * TypeScript type definitions for the Hardware Emulator Web UI
 */

export type MotorDirection = 'STOPPED' | 'OPENING' | 'CLOSING' | 'BRAKE';
export type DoorState = 'OPEN' | 'CLOSED' | 'OPENING' | 'CLOSING' | 'STOPPED' | 'UNKNOWN';
export type SwitchPressType = 'NONE' | 'SHORT' | 'LONG';

/**
 * Signal pattern tracking for buzzer or LED
 */
export interface SignalPattern {
  is_blinking: boolean;
  frequency_hz: number;
  period_ms: number;
  on_time_ms: number;
  off_time_ms: number;
  duty_cycle: number;  // 0-100
  cycle_count: number;
}

/**
 * Enhanced manual switch state
 */
export interface ManualSwitchState {
  is_pressed: boolean;
  press_type: SwitchPressType;
  press_duration_ms: number;
  short_threshold_ms: number;
  long_threshold_ms: number;
}

/**
 * Manual override configuration
 */
export interface OverrideConfig {
  enabled: boolean;
  hall_open: boolean;
  hall_close: boolean;
  door_fault: boolean;
  manual_switch: boolean;
  water_pulse_1: boolean;
  water_pulse_2: boolean;
}

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
  buzzer_pattern?: SignalPattern;
  led_pattern?: SignalPattern;
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
  manual_switch_pressed: boolean;  // Legacy compatibility
  manual_switch?: ManualSwitchState;  // Enhanced state
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
  short_press_ms?: number;
  long_press_ms?: number;
}

/**
 * Full emulator status response
 */
export interface EmulatorStatus {
  monitored: MonitoredSignals;
  emulated: EmulatedOutputs;
  config: EmulatorConfig;
  override?: OverrideConfig;
  scenario?: Scenario;
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

/**
 * Predefined scenario IDs
 */
export enum ScenarioId {
  NORMAL = 0,
  FREEZE_CONDITION = 1,
  DOOR_STUCK_OPEN = 2,
  DOOR_STUCK_CLOSED = 3,
  MOTOR_FAULT = 4,
  FROZEN_WATER_LINE = 5,
  PUMP_FAILURE = 6,
  CUSTOM = 7
}

/**
 * Scenario configuration
 */
export interface Scenario {
  id: ScenarioId;
  name: string;
  description: string;
  auto_simulate_door: boolean;
  simulate_door_stuck: boolean;
  door_position: number;
  door_state: DoorState;
  auto_generate_pulses: boolean;
  simulate_frozen_line: boolean;
  flow_rate_gpm: number;
  inject_door_fault: boolean;
  enable_override: boolean;
  override_hall_open?: boolean;
  override_hall_close?: boolean;
  override_door_fault?: boolean;
  override_manual_switch?: boolean;
  // Custom scenario fields
  is_custom?: boolean;
  index?: number;
}

/**
 * Custom scenario for creation/editing
 */
export interface CustomScenarioInput {
  name: string;
  description: string;
  auto_simulate_door: boolean;
  simulate_door_stuck: boolean;
  door_position: number;
  door_state: string;
  auto_generate_pulses: boolean;
  simulate_frozen_line: boolean;
  flow_rate_gpm: number;
  inject_door_fault: boolean;
  enable_override: boolean;
  override_hall_open?: boolean;
  override_hall_close?: boolean;
  override_door_fault?: boolean;
  override_manual_switch?: boolean;
}

// ============================================================================
// Log Recording & Playback types
// ============================================================================

export interface RecordingMetadata {
  id: string;
  filename: string;
  duration_ms: number;
  sample_count: number;
  created_at: number;
  label: string;
}

export interface RecordingStatus {
  recording: {
    state: 'IDLE' | 'RECORDING' | 'PAUSED';
    sample_count: number;
    duration_ms: number;
  };
  playback: {
    state: 'IDLE' | 'PLAYING' | 'PAUSED';
    id: string;
    position_ms: number;
    duration_ms: number;
    speed_percent: number;
  };
}

// ============================================================================
// Temperature Sensor Emulation types
// ============================================================================

export interface TempSensorConfig {
  enabled: boolean;
  temperature_c: number;
  disconnected: boolean;
  drift_enabled: boolean;
  drift_amplitude_c: number;
  drift_period_ms: number;
}

export interface TempSensorState {
  sensor1: TempSensorConfig;
  sensor2: TempSensorConfig;
}
