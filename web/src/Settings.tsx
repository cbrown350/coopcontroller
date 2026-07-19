import { createSignal, onMount, Show } from 'solid-js'
import { authenticatedFetch, setAuthCredentials, clearAuthCredentials } from './utils/api'

function Settings() {
  const [ssid, setSsid] = createSignal('')
  const [password, setPassword] = createSignal('')
  const [showPassword, setShowPassword] = createSignal(false)
  const [clearPassword, setClearPassword] = createSignal(false)
  const [passwordError, setPasswordError] = createSignal('')
  const [loading, setLoading] = createSignal(true)
  const [loaded, setLoaded] = createSignal(false)
  const [error, setError] = createSignal('')
  const [saveSuccess, setSaveSuccess] = createSignal(false)
  const [apMode, setApMode] = createSignal<boolean | null>(null)
  const [hostname, setHostname] = createSignal('CoopController')

  // Backup/Restore state
  const [backupLoading, setBackupLoading] = createSignal(false)
  const [restoreLoading, setRestoreLoading] = createSignal(false)
  const [showRestoreDialog, setShowRestoreDialog] = createSignal(false)
  const [restoreFile, setRestoreFile] = createSignal<File | null>(null)

  // Coop controller settings
  const [tempThresholdOnF, setTempThresholdOnF] = createSignal<number | null>(null)
  const [tempThresholdOffF, setTempThresholdOffF] = createSignal<number | null>(null)
  const [pumpOnTimeSeconds, setPumpOnTimeSeconds] = createSignal<number | null>(null)
  const [pumpOffTimeSeconds, setPumpOffTimeSeconds] = createSignal<number | null>(null)
  const [lightAutoMode, setLightAutoMode] = createSignal<boolean | null>(null)
  const [lightOnHour, setLightOnHour] = createSignal<number | null>(null)
  const [lightOnMinute, setLightOnMinute] = createSignal<number | null>(null)
  const [lightOnMode, setLightOnMode] = createSignal<string | null>(null) // 'fixed' or 'sunset_offset'
  const [lightOnSunsetOffsetMinutes, setLightOnSunsetOffsetMinutes] = createSignal<number | null>(null)
  const [lightOffHour, setLightOffHour] = createSignal<number | null>(null)
  const [lightBrightnessPercent, setLightBrightnessPercent] = createSignal<number | null>(null)
  const [lightTransitionDurationMinutes, setLightTransitionDurationMinutes] = createSignal<number | null>(null)
  const [waterFlowErrorTimeoutSeconds, setWaterFlowErrorTimeoutSeconds] = createSignal<number | null>(null)

  // Water meter calibration
  const [pulsesPerGallon, setPulsesPerGallon] = createSignal<number | null>(null)
  
  // Per-pulse flow calculation
  const [waterMeterPerPulseCalculationEnabled, setWaterMeterPerPulseCalculationEnabled] = createSignal<boolean | null>(null)
  
  // Pump off flow monitoring
  const [pumpOffFlowMonitoringEnabled, setPumpOffFlowMonitoringEnabled] = createSignal<boolean | null>(null)
  const [pumpOffFlowGracePeriodSeconds, setPumpOffFlowGracePeriodSeconds] = createSignal<number | null>(null)
  const [pumpOffFlowPulseThreshold, setPumpOffFlowPulseThreshold] = createSignal<number | null>(5)

  // Pump minimum daily cycles
  const [pumpMinDailyCyclesEnabled, setPumpMinDailyCyclesEnabled] = createSignal<boolean | null>(null)
  const [pumpMinDailyCycles, setPumpMinDailyCycles] = createSignal<number | null>(null)
  const [pumpMinCycleRunSeconds, setPumpMinCycleRunSeconds] = createSignal<number | null>(null)
  
  // Water meter timeout
  const [waterMeterTimeoutSeconds, setWaterMeterTimeoutSeconds] = createSignal<number | null>(null)

  // Log level (string enum from backend)
  const [showResetDialog, setShowResetDialog] = createSignal(false)
  const [showRebootDialog, setShowRebootDialog] = createSignal(false)
  const [logLevel, setLogLevel] = createSignal<string | null>(null)
  
  // WiFi LED control
  const [wifiLedEnabled, setWifiLedEnabled] = createSignal<boolean | null>(null)
  
  // Buzzer settings
  const [buzzerEnabled, setBuzzerEnabled] = createSignal<boolean | null>(null)
  const [buzzerType, setBuzzerType] = createSignal<string | null>(null)
  
  // Door control settings
  const [doorOpenTimeoutSeconds, setDoorOpenTimeoutSeconds] = createSignal<number | null>(null)
  const [doorCloseTimeoutSeconds, setDoorCloseTimeoutSeconds] = createSignal<number | null>(null)
  const [doorAutoOpenEnabled, setDoorAutoOpenEnabled] = createSignal<boolean | null>(null)
  const [doorAutoOpenOffsetMinutes, setDoorAutoOpenOffsetMinutes] = createSignal<number | null>(null)
  const [doorAutoOpenDays, setDoorAutoOpenDays] = createSignal<boolean[]>([true, true, true, true, true, true, true])
  const [doorAutoCloseEnabled, setDoorAutoCloseEnabled] = createSignal<boolean | null>(null)
  const [doorAutoCloseOffsetMinutes, setDoorAutoCloseOffsetMinutes] = createSignal<number | null>(null)
  const [doorAutoCloseDays, setDoorAutoCloseDays] = createSignal<boolean[]>([true, true, true, true, true, true, true])
  
  // Location settings
  const [latitude, setLatitude] = createSignal<number | null>(null)
  const [longitude, setLongitude] = createSignal<number | null>(null)
  const [timezoneOffsetHours, setTimezoneOffsetHours] = createSignal<number | null>(null)
  const [timezonePosix, setTimezonePosix] = createSignal<string>("")

  // Sunrise/sunset data for preview
  // Door auto-open/close offset bounds (mirror DoorController::DOOR_OFFSET_*).
  const DOOR_OFFSET_MIN = -240
  const DOOR_OFFSET_MAX = 780
  const clampOffset = (v: number) =>
    isNaN(v) ? 0 : Math.max(DOOR_OFFSET_MIN, Math.min(DOOR_OFFSET_MAX, v))

  // Helper function to get timezone display
  const getTimezoneDisplay = () => {
    const tz = timezonePosix();
    if (tz) return tz.split(',')[0]; // Show short name like "MST7MDT"
    const offset = timezoneOffsetHours() ?? 0;
    return `UTC${offset >= 0 ? '+' : ''}${offset}`;
  };
  const [sunriseData, setSunriseData] = createSignal<any>(null)
  const [sunsetData, setSunsetData] = createSignal<any>(null)
  
  // Door lockout and auto-calc
  const [doorLockoutEnabled, setDoorLockoutEnabled] = createSignal<boolean | null>(null)
  const [doorTimeoutAutoCalcEnabled, setDoorTimeoutAutoCalcEnabled] = createSignal<boolean | null>(null)

  // API Authentication settings
  const [apiAuthEnabled, setApiAuthEnabled] = createSignal<boolean | null>(null)
  const [apiUsername, setApiUsername] = createSignal('')
  const [apiPassword, setApiPassword] = createSignal('')
  const [showApiPassword, setShowApiPassword] = createSignal(false)

  // WiFi BSSID preference
  const [wifiBssidPreference, setWifiBssidPreference] = createSignal('')

  // Syslog configuration
  const [syslogServer, setSyslogServer] = createSignal('')
  const [syslogPort, setSyslogPort] = createSignal<number | null>(null)

  // Flow calculation interval
  const [flowCalculationIntervalSeconds, setFlowCalculationIntervalSeconds] = createSignal<number | null>(null)

  // History data settings
  const [historyEnabled, setHistoryEnabled] = createSignal<boolean | null>(null)
  const [historyTempMinIntervalSeconds, setHistoryTempMinIntervalSeconds] = createSignal<number | null>(null)
  const [historyFlowMinIntervalSeconds, setHistoryFlowMinIntervalSeconds] = createSignal<number | null>(null)
  const [historyBufferSize, setHistoryBufferSize] = createSignal<number | null>(null)

  // OTA Update settings
  const [autoUpdateEnabled, setAutoUpdateEnabled] = createSignal<boolean | null>(null)
  const [updateCheckIntervalHours, setUpdateCheckIntervalHours] = createSignal<number | null>(null)

  // Weather (OpenWeatherMap) settings
  const [weatherEnabled, setWeatherEnabled] = createSignal<boolean | null>(null)
  const [weatherApiKey, setWeatherApiKey] = createSignal('')
  const [showWeatherApiKey, setShowWeatherApiKey] = createSignal(false)
  const [weatherUnits, setWeatherUnits] = createSignal<string>('imperial')
  const [weatherUpdateIntervalMinutes, setWeatherUpdateIntervalMinutes] = createSignal<number | null>(10)
  const [weatherTestLoading, setWeatherTestLoading] = createSignal(false)
  const [weatherTestResult, setWeatherTestResult] = createSignal<{success: boolean, message: string} | null>(null)

  // LLM weather-decider settings (issue #6)
  const [llmEnabled, setLlmEnabled] = createSignal<boolean | null>(null)
  const [llmProviderType, setLlmProviderType] = createSignal<string>('openai_compatible')
  const [llmBaseUrl, setLlmBaseUrl] = createSignal('')
  const [llmApiKey, setLlmApiKey] = createSignal('')
  const [showLlmApiKey, setShowLlmApiKey] = createSignal(false)
  const [llmModel, setLlmModel] = createSignal('')
  const [llmTimeoutSeconds, setLlmTimeoutSeconds] = createSignal<number | null>(15)
  const [llmTestLoading, setLlmTestLoading] = createSignal(false)
  const [llmTestResult, setLlmTestResult] = createSignal<{success: boolean, message: string} | null>(null)

  // Notification settings - Telegram
  const [telegramEnabled, setTelegramEnabled] = createSignal<boolean | null>(null)
  const [telegramBotToken, setTelegramBotToken] = createSignal('')
  const [telegramChatId, setTelegramChatId] = createSignal('')
  const [showTelegramToken, setShowTelegramToken] = createSignal(false)
  const [telegramPollingInterval, setTelegramPollingInterval] = createSignal<number | null>(20)
  const [telegramTestLoading, setTelegramTestLoading] = createSignal(false)
  const [telegramTestResult, setTelegramTestResult] = createSignal<{success: boolean, message: string} | null>(null)

  // Notification settings - Email
  const [emailEnabled, setEmailEnabled] = createSignal<boolean | null>(null)
  const [emailSmtpServer, setEmailSmtpServer] = createSignal('')
  const [emailSmtpPort, setEmailSmtpPort] = createSignal<number | null>(587)
  const [emailSmtpUsername, setEmailSmtpUsername] = createSignal('')
  const [emailSmtpPassword, setEmailSmtpPassword] = createSignal('')
  const [showEmailPassword, setShowEmailPassword] = createSignal(false)
  const [emailFrom, setEmailFrom] = createSignal('')
  const [emailTo, setEmailTo] = createSignal('')
  const [emailTestLoading, setEmailTestLoading] = createSignal(false)
  const [emailTestResult, setEmailTestResult] = createSignal<{success: boolean, message: string} | null>(null)

  // Notification settings - MQTT
  const [mqttEnabled, setMqttEnabled] = createSignal<boolean | null>(null)
  const [mqttServer, setMqttServer] = createSignal('')
  const [mqttPort, setMqttPort] = createSignal<number | null>(1883)
  const [mqttUsername, setMqttUsername] = createSignal('')
  const [mqttPassword, setMqttPassword] = createSignal('')

  // Notification preferences
  const [notifyPumpError, setNotifyPumpError] = createSignal<boolean | null>(true)
  const [notifySensorError, setNotifySensorError] = createSignal<boolean | null>(true)
  const [notifyDoorFault, setNotifyDoorFault] = createSignal<boolean | null>(true)
  const [notifyWifiDisconnect, setNotifyWifiDisconnect] = createSignal<boolean | null>(false)
  const [notifySystemError, setNotifySystemError] = createSignal<boolean | null>(true)

  // Unsaved changes tracking
  const [hasUnsavedChanges, setHasUnsavedChanges] = createSignal(false)

  // Load settings from server
  onMount(async () => {
    try {
      setLoading(true)
      const response = await fetch('/get_settings')
      if (!response.ok) {
        throw new Error(`Failed to load settings: ${response.status} ${response.statusText}`)
      }
      const settings = await response.json()

      setSsid(settings.ssid ?? '')
      setPassword('') // never load password back
      setApMode(settings.ap_mode ?? null)

      setTempThresholdOnF(settings.temp_threshold_on_f ?? null)
      setTempThresholdOffF(settings.temp_threshold_off_f ?? null)
      setPumpOnTimeSeconds(settings.pump_on_time_seconds ?? null)
      setPumpOffTimeSeconds(settings.pump_off_time_seconds ?? null)
      setLightAutoMode(settings.light_auto_mode ?? null)
      setLightOnHour(settings.light_on_hour ?? null)
      setLightOnMinute(settings.light_on_minute ?? 0)
      setLightOnMode(settings.light_on_mode ?? 'fixed')
      setLightOnSunsetOffsetMinutes(settings.light_on_sunset_offset_minutes ?? 0)
      setLightOffHour(settings.light_off_hour ?? null)
      setLightBrightnessPercent(settings.light_brightness_percent ?? null)
      setLightTransitionDurationMinutes(settings.light_transition_duration_minutes ?? null)
      setWaterFlowErrorTimeoutSeconds(settings.water_flow_error_timeout_seconds ?? null)
      setWaterMeterTimeoutSeconds(settings.water_meter_timeout_seconds ?? null)
      setWaterMeterPerPulseCalculationEnabled(settings.water_meter_per_pulse_calculation_enabled ?? false)
      setPumpOffFlowMonitoringEnabled(settings.pump_off_flow_monitoring_enabled ?? false)
      setPumpOffFlowGracePeriodSeconds(settings.pump_off_flow_grace_period_seconds ?? 30)
      setPumpOffFlowPulseThreshold(settings.pump_off_flow_pulse_threshold ?? 5)
      setPumpMinDailyCyclesEnabled(settings.pump_min_daily_cycles_enabled ?? false)
      setPumpMinDailyCycles(settings.pump_min_daily_cycles ?? 3)
      setPumpMinCycleRunSeconds(settings.pump_min_cycle_run_seconds ?? 120)
      setLogLevel(settings.log_level ?? 'INFO')
      setPulsesPerGallon(settings.pulses_per_gallon ?? null)
      setWifiLedEnabled(settings.wifi_led_enabled ?? true)
      setBuzzerEnabled(settings.buzzer_enabled ?? true)
      setBuzzerType(settings.buzzer_type ?? 'ACTIVE')
      
      // Load door control settings
      setDoorOpenTimeoutSeconds(settings.door_open_timeout_seconds ?? 30)
      setDoorCloseTimeoutSeconds(settings.door_close_timeout_seconds ?? 30)
      setDoorAutoOpenEnabled(settings.door_auto_open_enabled ?? false)
      setDoorAutoOpenOffsetMinutes(settings.door_auto_open_offset_minutes ?? 0)
      setDoorAutoOpenDays(Array.isArray(settings.door_auto_open_days) ? settings.door_auto_open_days.slice(0, 7) : [true, true, true, true, true, true, true])
      setDoorAutoCloseEnabled(settings.door_auto_close_enabled ?? false)
      setDoorAutoCloseOffsetMinutes(settings.door_auto_close_offset_minutes ?? 0)
      setDoorAutoCloseDays(Array.isArray(settings.door_auto_close_days) ? settings.door_auto_close_days.slice(0, 7) : [true, true, true, true, true, true, true])
      
      // Load location settings
      setLatitude(settings.latitude ?? 40.7128)
      setLongitude(settings.longitude ?? -74.0060)
      setTimezoneOffsetHours(settings.timezone_offset_hours ?? -5)
      let tz = settings.timezone_posix ?? ""
      if (!tz) {
        // Auto-detect timezone from browser when not set on device
        try {
          const ianaMap: Record<string, string> = {
            'Pacific/Honolulu': 'HST10',
            'America/Anchorage': 'AKST9AKDT,M3.2.0,M11.1.0',
            'America/Los_Angeles': 'PST8PDT,M3.2.0,M11.1.0',
            'America/Phoenix': 'MST7',
            'America/Denver': 'MST7MDT,M3.2.0,M11.1.0',
            'America/Boise': 'MST7MDT,M3.2.0,M11.1.0',
            'America/Chicago': 'CST6CDT,M3.2.0,M11.1.0',
            'America/New_York': 'EST5EDT,M3.2.0,M11.1.0',
            'America/Indiana/Indianapolis': 'EST5EDT,M3.2.0,M11.1.0',
            'America/Detroit': 'EST5EDT,M3.2.0,M11.1.0',
            'America/Halifax': 'AST4ADT,M3.2.0,M11.1.0',
            'Europe/London': 'GMT0BST,M3.5.0/1,M10.5.0',
            'Europe/Berlin': 'CET-1CEST,M3.5.0,M10.5.0/3',
            'Europe/Paris': 'CET-1CEST,M3.5.0,M10.5.0/3',
            'Europe/Helsinki': 'EET-2EEST,M3.5.0/3,M10.5.0/4',
            'Asia/Kolkata': 'IST-5:30',
            'Asia/Shanghai': 'CST-8',
            'Asia/Tokyo': 'JST-9',
            'Australia/Sydney': 'AEST-10AEDT,M10.1.0,M4.1.0/3',
            'Pacific/Auckland': 'NZST-12NZDT,M9.5.0,M4.1.0/3',
          }
          const ianaTz = Intl.DateTimeFormat().resolvedOptions().timeZone
          if (ianaTz && ianaMap[ianaTz]) tz = ianaMap[ianaTz]
        } catch (_) { /* ignore */ }
      }
      setTimezonePosix(tz)

      // Load door lockout and auto-calc settings
      setDoorLockoutEnabled(settings.door_lockout_enabled ?? false)
      setDoorTimeoutAutoCalcEnabled(settings.door_timeout_auto_calc_enabled ?? false)

      // Load API authentication settings
      setApiAuthEnabled(settings.api_auth_enabled ?? false)
      setApiUsername(settings.api_username ?? 'admin')
      // Never load password from server
      setApiPassword('')

      // Set hostname if available
      if (settings.hostname) {
        setHostname(settings.hostname)
      }

      // Load WiFi BSSID preference
      setWifiBssidPreference(settings.wifi_bssid_preference ?? '')

      // Load syslog configuration
      setSyslogServer(settings.syslog_server ?? '')
      setSyslogPort(settings.syslog_port ?? 514)

      // Load flow calculation interval
      setFlowCalculationIntervalSeconds(settings.flow_calculation_interval_seconds ?? 60)

      // Load history data settings
      setHistoryEnabled(settings.history_enabled ?? true)
      setHistoryTempMinIntervalSeconds(settings.history_temp_min_interval_seconds ?? 60)
      setHistoryFlowMinIntervalSeconds(settings.history_flow_min_interval_seconds ?? 10)
      setHistoryBufferSize(settings.history_buffer_size ?? 500)

      // Load OTA update settings
      setAutoUpdateEnabled(settings.auto_update_enabled ?? false)
      setUpdateCheckIntervalHours(settings.update_check_interval_hours ?? 24)

      // Load weather settings (API key never returned for security)
      setWeatherEnabled(settings.weather_enabled ?? false)
      setWeatherApiKey('')
      setWeatherUnits(settings.weather_units ?? 'imperial')
      setWeatherUpdateIntervalMinutes(settings.weather_update_interval_minutes ?? 10)

      // Load LLM weather-decider settings (API key never returned for security)
      setLlmEnabled(settings.llm_enabled ?? false)
      setLlmProviderType(settings.llm_provider_type ?? 'openai_compatible')
      setLlmBaseUrl(settings.llm_base_url ?? '')
      setLlmApiKey('')
      setLlmModel(settings.llm_model ?? '')
      setLlmTimeoutSeconds(settings.llm_timeout_seconds ?? 15)

      // Load notification settings
      setTelegramEnabled(settings.telegram_enabled ?? false)
      setTelegramBotToken('') // Never load token back for security
      setTelegramChatId(settings.telegram_chat_id ?? '')
      setTelegramPollingInterval(settings.telegram_polling_interval_seconds ?? 20)
      setEmailEnabled(settings.email_enabled ?? false)
      setEmailSmtpServer(settings.email_smtp_server ?? '')
      setEmailSmtpPort(settings.email_smtp_port ?? 587)
      setEmailSmtpUsername(settings.email_smtp_username ?? '')
      setEmailSmtpPassword('') // Never load password back
      setEmailFrom(settings.email_from ?? '')
      setEmailTo(settings.email_to ?? '')
      setMqttEnabled(settings.mqtt_enabled ?? false)
      setMqttServer(settings.mqtt_server ?? '')
      setMqttPort(settings.mqtt_port ?? 1883)
      setMqttUsername(settings.mqtt_username ?? '')
      setMqttPassword('') // Never pre-fill password from server
      setNotifyPumpError(settings.notify_pump_error ?? true)
      setNotifySensorError(settings.notify_sensor_error ?? true)
      setNotifyDoorFault(settings.notify_door_fault ?? true)
      setNotifyWifiDisconnect(settings.notify_wifi_disconnect ?? false)
      setNotifySystemError(settings.notify_system_error ?? true)

      setLoaded(true)
      setError('')
      setHasUnsavedChanges(false)
      // Fetch sunrise/sunset data after loading location
      fetchSunriseSunsetData()
    } catch (err: any) {
      setLoaded(false)
      setError(`Error loading settings: ${err.message || 'Unknown error'}`)
      console.error('Failed to load settings:', err)
    } finally {
      setLoading(false)
    }
  })

  const validateThresholds = () => {
    if (tempThresholdOnF() !== null && tempThresholdOffF() !== null) {
      if (tempThresholdOnF()! > tempThresholdOffF()!) {
        setError('ON threshold must be less than or equal to OFF threshold')
        return false
      } else {
        setError('')
        return true
      }
    }
    return true
  }

  const validatePassword = (pwd: string) => {
    if (pwd.length === 0) {
      setPasswordError('')
      return true
    }
    if (pwd.length < 5) {
      setPasswordError('Password must be at least 5 characters long')
      return false
    }
    setPasswordError('')
    return true
  }

  const handleSave = async () => {
    if (!loaded()) {
      setError('Settings not loaded. Please refresh the page.')
      return
    }

    if (!validateThresholds()) return

    // Validate password if not clearing it
    if (!clearPassword() && password().length > 0 && !validatePassword(password())) {
      return
    }

    try {
      setSaveSuccess(false)
      setError('')

      const settingsPayload: any = {
        hostname: hostname(),
        ssid: ssid(),
        temp_threshold_on_f: tempThresholdOnF() ?? 34.0,
        temp_threshold_off_f: tempThresholdOffF() ?? 36.0,
        pump_on_time_seconds: pumpOnTimeSeconds() ?? 150,
        pump_off_time_seconds: pumpOffTimeSeconds() ?? 300,
        light_auto_mode: lightAutoMode() ?? false,
        light_on_hour: lightOnHour() ?? 6,
        light_on_minute: lightOnMinute() ?? 0,
        light_on_mode: lightOnMode() ?? 'fixed',
        light_on_sunset_offset_minutes: lightOnSunsetOffsetMinutes() ?? 0,
        light_off_hour: lightOffHour() ?? 20,
        light_brightness_percent: lightBrightnessPercent() ?? 100,
        light_transition_duration_minutes: lightTransitionDurationMinutes() ?? 5,
        pulses_per_gallon: pulsesPerGallon() ?? 450.0,
        water_meter_per_pulse_calculation_enabled: waterMeterPerPulseCalculationEnabled() ?? false,
        pump_off_flow_monitoring_enabled: pumpOffFlowMonitoringEnabled() ?? false,
        pump_off_flow_grace_period_seconds: pumpOffFlowGracePeriodSeconds() ?? 30,
        pump_off_flow_pulse_threshold: pumpOffFlowPulseThreshold() ?? 5,
        pump_min_daily_cycles_enabled: pumpMinDailyCyclesEnabled() ?? false,
        pump_min_daily_cycles: pumpMinDailyCycles() ?? 3,
        pump_min_cycle_run_seconds: pumpMinCycleRunSeconds() ?? 120,
        wifi_led_enabled: wifiLedEnabled() ?? true,
        buzzer_enabled: buzzerEnabled() ?? true,
        buzzer_type: buzzerType() ?? 'ACTIVE',
        door_open_timeout_seconds: doorOpenTimeoutSeconds() ?? 30,
        door_close_timeout_seconds: doorCloseTimeoutSeconds() ?? 30,
        door_auto_open_enabled: doorAutoOpenEnabled() ?? false,
        door_auto_open_offset_minutes: doorAutoOpenOffsetMinutes() ?? 0,
        door_auto_open_days: doorAutoOpenDays(),
        door_auto_close_enabled: doorAutoCloseEnabled() ?? false,
        door_auto_close_offset_minutes: doorAutoCloseOffsetMinutes() ?? 0,
        door_auto_close_days: doorAutoCloseDays(),
        latitude: latitude() ?? 40.7128,
        longitude: longitude() ?? -74.0060,
        timezone_posix: timezonePosix() ?? "",
        door_lockout_enabled: doorLockoutEnabled() ?? false,
        door_timeout_auto_calc_enabled: doorTimeoutAutoCalcEnabled() ?? false,
        log_level: logLevel() ?? 'INFO',
        api_auth_enabled: apiAuthEnabled() ?? false,
        api_username: apiUsername() ?? 'admin',
        wifi_bssid_preference: wifiBssidPreference() ?? '',
        syslog_server: syslogServer() ?? '',
        syslog_port: syslogPort() ?? 514,
        flow_calculation_interval_seconds: flowCalculationIntervalSeconds() ?? 60,
        history_enabled: historyEnabled() ?? true,
        history_temp_min_interval_seconds: historyTempMinIntervalSeconds() ?? 60,
        history_flow_min_interval_seconds: historyFlowMinIntervalSeconds() ?? 10,
        history_buffer_size: historyBufferSize() ?? 500,
        auto_update_enabled: autoUpdateEnabled() ?? false,
        update_check_interval_hours: updateCheckIntervalHours() ?? 24,
        weather_enabled: weatherEnabled() ?? false,
        weather_units: weatherUnits() ?? 'imperial',
        weather_update_interval_minutes: weatherUpdateIntervalMinutes() ?? 10,
        llm_enabled: llmEnabled() ?? false,
        llm_provider_type: llmProviderType() ?? 'openai_compatible',
        llm_base_url: llmBaseUrl() ?? '',
        llm_model: llmModel() ?? '',
        llm_timeout_seconds: llmTimeoutSeconds() ?? 15,
        water_flow_error_timeout_seconds: waterFlowErrorTimeoutSeconds() ?? 120,
        water_meter_timeout_seconds: waterMeterTimeoutSeconds() ?? 300,
        telegram_enabled: telegramEnabled() ?? false,
        telegram_chat_id: telegramChatId() ?? '',
        telegram_polling_interval_seconds: telegramPollingInterval() ?? 20,
        email_enabled: emailEnabled() ?? false,
        email_smtp_server: emailSmtpServer() ?? '',
        email_smtp_port: emailSmtpPort() ?? 587,
        email_smtp_username: emailSmtpUsername() ?? '',
        email_from: emailFrom() ?? '',
        email_to: emailTo() ?? '',
        mqtt_enabled: mqttEnabled() ?? false,
        mqtt_server: mqttServer() ?? '',
        mqtt_port: mqttPort() ?? 1883,
        mqtt_username: mqttUsername() ?? '',
        notify_pump_error: notifyPumpError() ?? true,
        notify_sensor_error: notifySensorError() ?? true,
        notify_door_fault: notifyDoorFault() ?? true,
        notify_wifi_disconnect: notifyWifiDisconnect() ?? false,
        notify_system_error: notifySystemError() ?? true
      }

      // Handle WiFi password: either set new password, clear it, or don't change it
      if (clearPassword()) {
        settingsPayload['passwd'] = ''
      } else if (password().length >= 5) {
        settingsPayload['passwd'] = password()
      }

      // Handle API password: only include if provided (non-empty)
      if (apiPassword().length > 0) {
        settingsPayload['api_password'] = apiPassword()
      }

      // Handle weather API key: only include if provided (non-empty)
      if (weatherApiKey().length > 0) {
        settingsPayload['weather_api_key'] = weatherApiKey()
      }

      // Handle LLM API key: only include if provided (non-empty)
      if (llmApiKey().length > 0) {
        settingsPayload['llm_api_key'] = llmApiKey()
      }

      // Handle Telegram bot token: only include if provided (non-empty)
      if (telegramBotToken().length > 0) {
        settingsPayload['telegram_bot_token'] = telegramBotToken()
      }

      // Handle email SMTP password: only include if provided (non-empty)
      if (emailSmtpPassword().length > 0) {
        settingsPayload['email_smtp_password'] = emailSmtpPassword()
      }

      // Handle MQTT password: only include if provided (non-empty)
      if (mqttPassword().length > 0) {
        settingsPayload['mqtt_password'] = mqttPassword()
      }

      // Cache credentials for authenticated requests if auth is enabled
      if (apiAuthEnabled() && apiPassword().length > 0) {
        setAuthCredentials(apiUsername(), apiPassword())
      } else if (apiAuthEnabled()) {
        // Use existing username with empty password if no new password provided
        setAuthCredentials(apiUsername(), '')
      } else {
        clearAuthCredentials()
      }

      const response = await authenticatedFetch('/update_settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settingsPayload)
      })

      if (!response.ok) {
        throw new Error(`Failed to save settings: ${response.status} ${response.statusText}`)
      }

      setSaveSuccess(true)
      setHasUnsavedChanges(false)
      setTimeout(() => setSaveSuccess(false), 3000)

      // Fetch sunrise/sunset data after saving location settings
      fetchSunriseSunsetData()
    } catch (err: any) {
      setError(`Error saving settings: ${err.message || 'Unknown error'}`)
      console.error('Failed to save settings:', err)
    }
  }

  const handleFactoryReset = async () => {
    try {
      const response = await authenticatedFetch('/factory_reset', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ confirm: 'RESET' })
      })
      
      if (response.ok) {
        alert('Factory reset complete! Device is restarting...')
        setTimeout(() => window.location.reload(), 5000)
      } else {
        const error = await response.text()
        alert(`Factory reset failed: ${error}`)
      }
    } catch {
      // Network error is expected - device is rebooting after factory reset
      alert('Factory reset complete! Device is restarting...')
      setTimeout(() => window.location.reload(), 10000)
    }
  }

  const handleReboot = async () => {
    try {
      const response = await authenticatedFetch('/reboot', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ confirm: 'REBOOT' })
      })
      
      if (response.ok) {
        alert('Device is rebooting...')
        setTimeout(() => window.location.reload(), 5000)
      } else {
        const error = await response.text()
        alert(`Reboot failed: ${error}`)
      }
    } catch {
      // Network error is expected - device is rebooting
      alert('Device is rebooting...')
      setTimeout(() => window.location.reload(), 10000)
    }
  }

  const handleBackup = async () => {
    try {
      setBackupLoading(true)
      setError('')

      const response = await fetch('/settings/backup')
      if (!response.ok) {
        throw new Error(`Failed to backup settings: ${response.status} ${response.statusText}`)
      }

      const blob = await response.blob()
      const url = window.URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = 'coop_controller_settings.json'
      document.body.appendChild(a)
      a.click()
      window.URL.revokeObjectURL(url)
      document.body.removeChild(a)
    } catch (err: any) {
      setError(`Error backing up settings: ${err.message || 'Unknown error'}`)
      console.error('Failed to backup settings:', err)
    } finally {
      setBackupLoading(false)
    }
  }

  const handleRestore = async () => {
    if (!restoreFile()) {
      setError('Please select a settings file to restore')
      return
    }

    try {
      setRestoreLoading(true)
      setError('')

      const fileContent = await restoreFile()!.text()
      const settings = JSON.parse(fileContent)

      const response = await authenticatedFetch('/settings/restore', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
      })

      if (!response.ok) {
        const errorData = await response.json()
        throw new Error(errorData.error || `Failed to restore settings: ${response.status} ${response.statusText}`)
      }

      alert('Settings restored successfully! The page will reload.')
      setTimeout(() => window.location.reload(), 1000)
    } catch (err: any) {
      setError(`Error restoring settings: ${err.message || 'Unknown error'}`)
      console.error('Failed to restore settings:', err)
    } finally {
      setRestoreLoading(false)
      setShowRestoreDialog(false)
      setRestoreFile(null)
    }
  }

  const formatTime = (seconds: number) => {
    const minutes = Math.floor(seconds / 60)
    const remainingSeconds = seconds % 60
    return `${minutes}m ${remainingSeconds}s`
  }

  const fetchSunriseSunsetData = async () => {
    try {
      const response = await fetch('/sun/times')
      if (response.ok) {
        const data = await response.json()
        setSunriseData(data)
        setSunsetData(data)
      }
    } catch (error) {
      console.error('Failed to fetch sunrise/sunset data:', error)
    }
  }

  const handleTestWeather = async () => {
    setWeatherTestLoading(true)
    setWeatherTestResult(null)
    try {
      // Send current form value so testing works before saving
      const body: Record<string, string> = {}
      if (weatherApiKey()) body.weather_api_key = weatherApiKey()
      const response = await authenticatedFetch('/weather/test', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
      })
      const data = await response.json()
      setWeatherTestResult({
        success: data.success,
        message: data.success
          ? `OK — ${data.status?.current?.description ?? 'weather fetched'}`
          : (data.error || 'Weather fetch failed')
      })
    } catch (err: any) {
      setWeatherTestResult({ success: false, message: err.message || 'Request failed' })
    } finally {
      setWeatherTestLoading(false)
      setTimeout(() => setWeatherTestResult(null), 6000)
    }
  }

  const handleTestLlmConnection = async () => {
    setLlmTestLoading(true)
    setLlmTestResult(null)
    try {
      // Send current form values so testing works before saving
      const body: Record<string, any> = {}
      if (llmBaseUrl()) body.llm_base_url = llmBaseUrl()
      if (llmApiKey()) body.llm_api_key = llmApiKey()
      if (llmModel()) body.llm_model = llmModel()
      if (llmProviderType()) body.llm_provider_type = llmProviderType()
      if (llmTimeoutSeconds()) body.llm_timeout_seconds = llmTimeoutSeconds()
      const response = await authenticatedFetch('/weather/llm/test_connection', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
      })
      const data = await response.json()
      setLlmTestResult({
        success: data.success,
        message: data.success ? 'Connected! Provider responded.' : (data.error || 'Connection failed')
      })
    } catch (err: any) {
      setLlmTestResult({ success: false, message: err.message || 'Request failed' })
    } finally {
      setLlmTestLoading(false)
      setTimeout(() => setLlmTestResult(null), 8000)
    }
  }

  const handleTestTelegram = async () => {
    setTelegramTestLoading(true)
    setTelegramTestResult(null)
    try {
      // Send current form values so testing works before saving
      const body: Record<string, string> = {}
      if (telegramBotToken()) body.telegram_bot_token = telegramBotToken()
      if (telegramChatId()) body.telegram_chat_id = telegramChatId()
      const response = await authenticatedFetch('/notifications/test/telegram', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
      })
      const data = await response.json()
      setTelegramTestResult({
        success: data.success,
        message: data.success ? 'Test message sent!' : (data.error || 'Failed to send')
      })
    } catch (err: any) {
      setTelegramTestResult({ success: false, message: err.message || 'Request failed' })
    } finally {
      setTelegramTestLoading(false)
      setTimeout(() => setTelegramTestResult(null), 5000)
    }
  }

  const handleTestEmail = async () => {
    setEmailTestLoading(true)
    setEmailTestResult(null)
    try {
      // Send current form values so testing works before saving
      const body: Record<string, any> = {}
      if (emailSmtpServer()) body.email_smtp_server = emailSmtpServer()
      if (emailSmtpPort()) body.email_smtp_port = emailSmtpPort()
      if (emailSmtpUsername()) body.email_smtp_username = emailSmtpUsername()
      if (emailSmtpPassword()) body.email_smtp_password = emailSmtpPassword()
      if (emailFrom()) body.email_from = emailFrom()
      if (emailTo()) body.email_to = emailTo()
      const response = await authenticatedFetch('/notifications/test/email', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
      })
      const data = await response.json()
      setEmailTestResult({
        success: data.success,
        message: data.success ? 'Test email sent!' : (data.error || 'Failed to send')
      })
    } catch (err: any) {
      setEmailTestResult({ success: false, message: err.message || 'Request failed' })
    } finally {
      setEmailTestLoading(false)
      setTimeout(() => setEmailTestResult(null), 5000)
    }
  }

  const markChanged = () => setHasUnsavedChanges(true)

  return (
    <div class="card w-full max-w-full min-w-0">
      {/* Floating notifications */}
      <Show when={error()}>
        <div class="fixed top-4 left-1/2 -translate-x-1/2 z-50 w-auto max-w-lg">
          <div role="alert" class="alert alert-error shadow-lg">{error()}</div>
        </div>
      </Show>
      <Show when={saveSuccess()}>
        <div class="fixed top-4 left-1/2 -translate-x-1/2 z-50 w-auto max-w-lg">
          <div role="alert" class="alert alert-success shadow-lg">Settings saved successfully!</div>
        </div>
      </Show>

      {/* Floating unsaved changes indicator */}
      <Show when={hasUnsavedChanges() && loaded()}>
        <div class="fixed bottom-6 left-6 z-50">
          <div class="badge badge-warning gap-1 shadow-lg p-3">Unsaved changes</div>
        </div>
      </Show>

      {/* Floating save button */}
      <Show when={loaded() && !loading()}>
        <button type="button" class="btn btn-accent shadow-lg fixed bottom-6 right-6 z-50" onClick={handleSave} disabled={!loaded()}>Save Settings</button>
      </Show>

      {loading() ? (
        <p>Loading settings... <span class="loading loading-spinner loading-xl"></span></p>
      ) : (
        <div onInput={markChanged} onChange={markChanged}>

          <h2 class="text-lg font-bold mb-4">Device Settings</h2>
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
            <fieldset class="fieldset">
              <legend class="fieldset-legend">Hostname</legend>
              <Show when={loaded()}>
                <input type="text" id="hostname" value={hostname()} onInput={(e) => setHostname(e.target.value)} placeholder="CoopController" class="input" maxLength={32} pattern="[a-zA-Z0-9\-]+" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" placeholder="--" value="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">Device hostname for mDNS and AP mode (e.g. {hostname()}.local). Changing requires restart.</div>
            </fieldset>
          </div>

          <h2 class="text-lg font-bold mb-4 mt-10">Wifi Settings</h2>
          {apMode() ? (
            <div>
              <fieldset class="fieldset">
                <legend class="fieldset-legend">SSID</legend>
                <input type="text" id="ssid" value={ssid()} onInput={(e) => setSsid(e.target.value)} placeholder="Enter WiFi network name..." class="input" />
              </fieldset>

              <fieldset class="fieldset">
                <legend class="fieldset-legend">Password</legend>
                <div class="input-group">
                  <input 
                    type={showPassword() ? "text" : "password"} 
                    id="password" 
                    value={clearPassword() ? '' : password()} 
                    onInput={(e) => {
                      const value = e.target.value
                      setPassword(value)
                      validatePassword(value)
                    }} 
                    placeholder="Enter WiFi password..." 
                    class={`input ${passwordError() ? 'input-error' : ''}`}
                    disabled={clearPassword()}
                  />
                  <button 
                    type="button" 
                    class="btn btn-ghost" 
                    onClick={() => setShowPassword(!showPassword())}
                    title={showPassword() ? "Hide password" : "Show password"}
                    disabled={clearPassword()}
                  >
                    <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class={showPassword() ? "lucide lucide-eye-off" : "lucide lucide-eye"}>
                      {showPassword() ? (
                        <>
                          <path d="M9.88 9.88a3 3 0 1 0 4.24 4.24"></path>
                          <path d="M10.73 5.08A10.43 10.43 0 0 1 12 5c7 0 10 7 10 7a13.16 13.16 0 0 1-1.67 2.68"></path>
                          <path d="M6.61 6.61A13.526 13.526 0 0 0 2 12s3 7 10 7a9.74 9.74 0 0 0 5.39-1.61"></path>
                          <line x1="2" x2="22" y1="2" y2="22"></line>
                        </>
                      ) : (
                        <>
                          <path d="M2 12s3-7 10-7 10 7 10 7-3 7-10 7-10-7-10-7Z"></path>
                          <circle cx="12" cy="12" r="3"></circle>
                        </>
                      )}
                    </svg>
                  </button>
                </div>
                <Show when={passwordError()}>
                  <div class="fieldset-label text-error">{passwordError()}</div>
                </Show>
              </fieldset>

              <fieldset class="fieldset">
                <legend class="fieldset-legend">Clear Password</legend>
                <div class="form-control">
                  <label class="label cursor-pointer">
                    <span class="label-text">Clear WiFi password (connect to open network)</span>
                    <input
                      type="checkbox"
                      class="toggle toggle-error"
                      checked={clearPassword()}
                      onChange={(e) => {
                        setClearPassword(e.currentTarget.checked)
                        if (e.currentTarget.checked) {
                          setPasswordError('')
                        }
                      }}
                    />
                  </label>
                  <label class="label">
                    <span class="label-text-alt">Check to remove WiFi password and connect to an open network</span>
                  </label>
                </div>
              </fieldset>

              <div role="alert" class="mt-4 alert alert-info alert-soft">
                <span>
                  Note: after changing wifi network you may need to enter a new IP address to get to this device. If wifi connection fails, device will revert to AP mode and you can reconnect by connecting to Wifi network named {hostname()}. If your network supports MDNS discovery you can also find this device at <a class="link link-accent" href={`http://${hostname()}.local`}>{hostname()}.local</a>
                </span>
              </div>
            </div>
          ) : (
            <button class="btn" onClick={() => setApMode(true)}>Change Wifi network</button>
          )}

          <h2 class="text-lg font-bold mb-4 mt-10">Coop Controller Settings</h2>

          <h2 class="text-lg font-bold mb-4 mt-10">Temperature Control Settings</h2>
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
            <fieldset class="fieldset">
              <legend class="fieldset-legend">Temperature ON Threshold (°F)</legend>
              <Show when={loaded()}>
                <input type="number" id="temp_threshold_on_f" value={tempThresholdOnF()!} onInput={(e) => { setTempThresholdOnF(parseFloat(e.target.value)); validateThresholds() }} placeholder="34" step="0.1" min="0" max="100" class="input" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" placeholder="--" value="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">When temperature falls below this value, pump cycling will activate</div>
            </fieldset>

            <fieldset class="fieldset">
              <legend class="fieldset-legend">Temperature OFF Threshold (°F)</legend>
              <Show when={loaded()}>
                <input type="number" id="temp_threshold_off_f" value={tempThresholdOffF()!} onInput={(e) => { setTempThresholdOffF(parseFloat(e.target.value)); validateThresholds() }} placeholder="36" step="0.1" min="0" max="100" class="input" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">When temperature rises above this value, pump cycling will deactivate (hysteresis)</div>
            </fieldset>
          </div>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Water Flow Error Timeout (seconds)</legend>
            <Show when={loaded()}>
              <input type="number" id="water_flow_error_timeout_seconds" value={waterFlowErrorTimeoutSeconds()!} onInput={(e) => setWaterFlowErrorTimeoutSeconds(parseInt(e.target.value))} placeholder="120" step="1" min="10" max="600" class="input" />
            </Show>
            <Show when={!loaded()}>
              <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
            </Show>
            <div class="fieldset-label">Time without water flow before declaring error (default: 120 seconds)</div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Water Meter Calibration (Pulses per Gallon)</legend>
            <Show when={loaded()}>
              <input type="number" id="pulses_per_gallon" value={pulsesPerGallon()!} onInput={(e) => setPulsesPerGallon(parseFloat(e.target.value))} placeholder="450" step="1" min="100" max="2000" class="input" />
            </Show>
            <Show when={!loaded()}>
              <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
            </Show>
            <div class="fieldset-label">Typical range: 200-1000 pulses per gallon. Consult your water meter specifications.</div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Water Meter Connection Timeout (seconds)</legend>
            <Show when={loaded()}>
              <input type="number" id="water_meter_timeout_seconds" value={waterMeterTimeoutSeconds()!} onInput={(e) => setWaterMeterTimeoutSeconds(parseInt(e.target.value))} placeholder="300" step="1" min="60" max="3600" class="input" />
            </Show>
            <Show when={!loaded()}>
              <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
            </Show>
            <div class="fieldset-label">Time since last pulse before water meter considered disconnected (default: 300 seconds)</div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Flow Calculation Interval (seconds)</legend>
            <Show when={loaded()}>
              <input type="number" id="flow_calculation_interval_seconds" value={flowCalculationIntervalSeconds()!} onInput={(e) => setFlowCalculationIntervalSeconds(parseInt(e.target.value))} placeholder="60" step="1" min="5" max="300" class="input" />
            </Show>
            <Show when={!loaded()}>
              <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
            </Show>
            <div class="fieldset-label">How often to calculate interval-based flow rate in seconds (default: 60). Lower values give more frequent updates. Only used when per-pulse calculation is disabled.</div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Per-Pulse Flow Calculation</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable Per-Pulse Flow Calculation</span>
                <input
                  type="checkbox"
                  class="toggle toggle-primary"
                  checked={waterMeterPerPulseCalculationEnabled() ?? false}
                  onChange={(e) => setWaterMeterPerPulseCalculationEnabled(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Calculate flow rate instantly after each pulse instead of 60-second interval. More responsive to flow changes.
                </span>
              </label>
            </div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Pump OFF Flow Monitoring</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable Pump OFF Flow Monitoring</span>
                <input
                  type="checkbox"
                  class="toggle toggle-warning"
                  checked={pumpOffFlowMonitoringEnabled() ?? false}
                  onChange={(e) => setPumpOffFlowMonitoringEnabled(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Monitor for water flow when pump is OFF to detect hardware faults (stuck relay, valve leak).
                </span>
              </label>
            </div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Pump OFF Grace Period (seconds)</legend>
            <Show when={loaded()}>
              <input type="number" id="pump_off_flow_grace_period_seconds" value={pumpOffFlowGracePeriodSeconds()!} onInput={(e) => setPumpOffFlowGracePeriodSeconds(parseInt(e.target.value))} placeholder="30" step="1" min="5" max="300" class="input" />
            </Show>
            <Show when={!loaded()}>
              <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
            </Show>
            <div class="fieldset-label">Grace period after pump turns off before monitoring starts (default: 30 seconds)</div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Leak Detection Pulse Threshold</legend>
            <Show when={loaded()}>
              <input type="number" id="pump_off_flow_pulse_threshold" value={pumpOffFlowPulseThreshold()!} onInput={(e) => setPumpOffFlowPulseThreshold(parseInt(e.target.value))} placeholder="5" step="1" min="1" max="100" class="input" />
            </Show>
            <Show when={!loaded()}>
              <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
            </Show>
            <label class="label">
              <span class="label-text-alt">
                Minimum water meter pulses to trigger a leak alert. Higher values avoid false alarms from normal dripping or settling.
              </span>
            </label>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Pump Maintenance Cycles</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable Minimum Daily Pump Cycles</span>
                <input
                  type="checkbox"
                  class="toggle toggle-info"
                  checked={pumpMinDailyCyclesEnabled() ?? false}
                  onChange={(e) => setPumpMinDailyCyclesEnabled(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Run the pump at regular intervals to prevent water stagnation and maintain pump seal lubrication. Temperature-triggered cycles count toward the minimum.
                </span>
              </label>
            </div>
          </fieldset>

          <Show when={pumpMinDailyCyclesEnabled()}>
            <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Cycles per Day</legend>
                <Show when={loaded()}>
                  <input type="number" id="pump_min_daily_cycles" value={pumpMinDailyCycles()!} onInput={(e) => setPumpMinDailyCycles(parseInt(e.target.value))} placeholder="3" step="1" min="1" max="12" class="input" />
                </Show>
                <Show when={!loaded()}>
                  <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
                </Show>
                <div class="fieldset-label">Minimum pump cycles per 24 hours (1-12, default: 3)</div>
              </fieldset>

              <fieldset class="fieldset">
                <legend class="fieldset-legend">Cycle Duration (seconds)</legend>
                <Show when={loaded()}>
                  <input type="number" id="pump_min_cycle_run_seconds" value={pumpMinCycleRunSeconds()!} onInput={(e) => setPumpMinCycleRunSeconds(parseInt(e.target.value))} placeholder="120" step="10" min="30" max="600" class="input" />
                </Show>
                <Show when={!loaded()}>
                  <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
                </Show>
                <div class="fieldset-label">How long each scheduled cycle runs (30-600s, default: 120s)</div>
              </fieldset>
            </div>
          </Show>

          <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
            <fieldset class="fieldset">
              <legend class="fieldset-legend">Pump ON Time</legend>
              <Show when={loaded()}>
                <input type="number" id="pump_on_time_seconds" value={pumpOnTimeSeconds()!} onInput={(e) => setPumpOnTimeSeconds(parseInt(e.target.value))} placeholder="300" min="10" max="3600" class="input" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">Seconds ({loaded() ? formatTime(pumpOnTimeSeconds()!) : '--'})</div>
            </fieldset>

            <fieldset class="fieldset">
              <legend class="fieldset-legend">Pump OFF Time</legend>
              <Show when={loaded()}>
                <input type="number" id="pump_off_time_seconds" value={pumpOffTimeSeconds()!} onInput={(e) => setPumpOffTimeSeconds(parseInt(e.target.value))} placeholder="600" min="10" max="3600" class="input" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">Seconds ({loaded() ? formatTime(pumpOffTimeSeconds()!) : '--'})</div>
            </fieldset>
          </div>

          <h2 class="text-lg font-bold mb-4 mt-10">Light Settings</h2>

          <h2 class="text-lg font-bold mb-4 mt-10">Light Control Settings</h2>
          
           <fieldset class="fieldset mt-4">
             <legend class="fieldset-legend">Light Auto Mode</legend>
             <div class="form-control">
               <label class="label cursor-pointer">
                 <span class="label-text">Enable Automatic Light Control</span>
                 <input
                   type="checkbox"
                   class="toggle toggle-primary"
                   checked={lightAutoMode() ?? false}
                   onChange={(e) => setLightAutoMode(e.currentTarget.checked)}
                 />
               </label>
               <label class="label">
                 <span class="label-text-alt">
                   Enable automatic light scheduling based on configured ON/OFF hours
                 </span>
               </label>
             </div>
           </fieldset>

          <Show when={lightAutoMode()}>
            <div class="mt-4">
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Light ON Mode</legend>
                <div class="form-control">
                  <label class="label cursor-pointer">
                    <span class="label-text">Light ON Mode</span>
                    <select 
                      id="light_on_mode" 
                      title="Light ON Mode" 
                      class="select" 
                      value={lightOnMode() ?? 'fixed'} 
                      onInput={(e) => {setLightOnMode((e.target as HTMLSelectElement).value); fetchSunriseSunsetData()}}
                    >
                      <option value="fixed">Fixed Time</option>
                      <option value="sunset_offset">Sunset Offset</option>
                    </select>
                  </label>
                  <label class="label">
                    <span class="label-text-alt">
                      Choose how light ON time is determined
                    </span>
                  </label>
                </div>
              </fieldset>

              <Show when={lightOnMode() === 'fixed'}>
                <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
                  <fieldset class="fieldset">
                    <legend class="fieldset-legend">Light ON Hour</legend>
                    <Show when={loaded()}>
                      <input type="number" id="light_on_hour" value={lightOnHour()!} onInput={(e) => setLightOnHour(parseInt(e.target.value))} placeholder="6" min="0" max="23" class="input" />
                    </Show>
                    <Show when={!loaded()}>
                      <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
                    </Show>
                    <div class="fieldset-label">24-hour format (0-23)</div>
                  </fieldset>

                  <fieldset class="fieldset">
                    <legend class="fieldset-legend">Light ON Minute</legend>
                    <Show when={loaded()}>
                      <input type="number" id="light_on_minute" value={lightOnMinute()!} onInput={(e) => setLightOnMinute(parseInt(e.target.value))} placeholder="0" min="0" max="59" class="input" />
                    </Show>
                    <Show when={!loaded()}>
                      <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
                    </Show>
                    <div class="fieldset-label">Minutes (0-59)</div>
                  </fieldset>
                </div>
              </Show>

              <Show when={lightOnMode() === 'sunset_offset'}>
                <fieldset class="fieldset mt-4">
                  <legend class="fieldset-legend">Light ON Sunset Offset</legend>
                  <Show when={loaded()}>
                    <input type="number" id="light_on_sunset_offset_minutes" value={lightOnSunsetOffsetMinutes()!} onInput={(e) => setLightOnSunsetOffsetMinutes(parseInt(e.target.value))} placeholder="0" step="1" min="-120" max="120" class="input" />
                  </Show>
                  <Show when={!loaded()}>
                    <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
                  </Show>
                  <div class="fieldset-label">Minutes before/after sunset to turn on light (negative = before, positive = after)</div>
                </fieldset>
              </Show>

              <fieldset class="fieldset">
                <legend class="fieldset-legend">Light OFF Hour</legend>
                <Show when={loaded()}>
                  <input type="number" id="light_off_hour" value={lightOffHour()!} onInput={(e) => setLightOffHour(parseInt(e.target.value))} placeholder="20" min="0" max="23" class="input" />
                </Show>
                <Show when={!loaded()}>
                  <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
                </Show>
                <div class="fieldset-label">24-hour format (0-23)</div>
              </fieldset>
            </div>
          </Show>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Max Brightness</legend>
            <Show when={loaded()}>
              <input type="number" id="light_brightness_percent" value={lightBrightnessPercent()!} onInput={(e) => setLightBrightnessPercent(parseInt(e.target.value))} placeholder="100" min="0" max="100" class="input" />
            </Show>
            <Show when={!loaded()}>
              <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
            </Show>
            <div class="fieldset-label">Maximum brightness level (0-100%)</div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Transition Duration</legend>
            <Show when={loaded()}>
              <input type="number" id="light_transition_duration_minutes" value={lightTransitionDurationMinutes()!} onInput={(e) => setLightTransitionDurationMinutes(parseInt(e.target.value))} placeholder="5" min="1" max="60" class="input" />
            </Show>
            <Show when={!loaded()}>
              <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
            </Show>
            <div class="fieldset-label">Fade in/out duration in minutes (1-60)</div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Log Level</legend>
            <Show when={loaded()} fallback={
              <select id="log_level-fallback" title="Log Level" class="select input-disabled" disabled>
                <option>--</option>
              </select>
            }>
              <select id="log_level" title="Log Level" class="select" value={logLevel() ?? 'INFO'} onInput={(e) => setLogLevel((e.target as HTMLSelectElement).value)}>
                <option value="VERBOSE">Verbose</option>
                <option value="DEBUG">Debug</option>
                <option value="INFO">Info</option>
                <option value="WARNING">Warning</option>
                <option value="ERROR">Error</option>
              </select>
            </Show>
            <div class="fieldset-label">Select logging verbosity for diagnostics. Higher verbosity may produce more logs.</div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">WiFi Status LED</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable WiFi Status LED</span>
                <input
                  type="checkbox"
                  class="toggle toggle-primary"
                  checked={wifiLedEnabled() ?? true}
                  onChange={(e) => setWifiLedEnabled(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Enable visual WiFi status indicator (heartbeat when connected, fast blink when disconnected)
                </span>
              </label>
            </div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">WiFi BSSID Preference</legend>
            <Show when={loaded()}>
              <input type="text" value={wifiBssidPreference()} onInput={(e) => setWifiBssidPreference(e.target.value)} placeholder="AA:BB:CC:DD:EE:FF" class="input" />
            </Show>
            <Show when={!loaded()}>
              <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
            </Show>
            <div class="fieldset-label">Preferred access point MAC address for mesh networks (leave empty for auto-select)</div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Syslog Server</legend>
            <div class="grid grid-cols-1 sm:grid-cols-2 gap-4">
              <div>
                <label class="label"><span class="label-text">Server Address</span></label>
                <Show when={loaded()}>
                  <input type="text" value={syslogServer()} onInput={(e) => setSyslogServer(e.target.value)} placeholder="192.168.1.100" class="input w-full" />
                </Show>
                <Show when={!loaded()}>
                  <input type="text" value="--" placeholder="--" disabled class="input input-disabled w-full" />
                </Show>
              </div>
              <div>
                <label class="label"><span class="label-text">Port</span></label>
                <Show when={loaded()}>
                  <input type="number" value={syslogPort()!} onInput={(e) => setSyslogPort(parseInt(e.target.value))} placeholder="514" min="1" max="65535" class="input w-full" />
                </Show>
                <Show when={!loaded()}>
                  <input type="text" value="--" placeholder="--" disabled class="input input-disabled w-full" />
                </Show>
              </div>
            </div>
            <div class="fieldset-label">Remote syslog server for log forwarding. Leave server empty to disable. Default port: 514</div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Buzzer Alerts</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable Buzzer Alerts</span>
                <input
                  type="checkbox"
                  class="toggle toggle-primary"
                  checked={buzzerEnabled() ?? true}
                  onChange={(e) => setBuzzerEnabled(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Enable audible alerts for system errors and warnings
                </span>
              </label>
            </div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Buzzer Type</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Buzzer Type</span>
                <select 
                  id="buzzer_type" 
                  title="Buzzer Type" 
                  class="select" 
                  value={buzzerType() ?? 'ACTIVE'} 
                  onInput={(e) => setBuzzerType((e.target as HTMLSelectElement).value)}
                >
                  <option value="ACTIVE">Active Buzzer</option>
                  <option value="PASSIVE">Passive Buzzer</option>
                </select>
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Active buzzers are simple on/off. Passive buzzers support tone generation for different sounds.
                </span>
              </label>
            </div>
          </fieldset>

          <h2 class="text-lg font-bold mb-4 mt-10">Door Automatic Open &amp; Close</h2>
          <p class="text-sm opacity-70 mb-2">
            Automatic open and close are independent. Each can be enabled separately, has its own
            sunrise/sunset offset (negative = before, positive = after), and its own days-of-week.
          </p>

          {/* Auto-Open */}
          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Automatic Open (Sunrise)</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable Automatic Open</span>
                <input
                  type="checkbox"
                  class="toggle toggle-primary"
                  checked={doorAutoOpenEnabled() ?? false}
                  onChange={(e) => setDoorAutoOpenEnabled(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Open the door automatically at sunrise + offset on the selected days
                </span>
              </label>
            </div>
          </fieldset>

          <Show when={doorAutoOpenEnabled()}>
            <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Sunrise Offset (minutes)</legend>
                <Show when={loaded()}>
                  <input type="number" value={doorAutoOpenOffsetMinutes()!} onInput={(e) => setDoorAutoOpenOffsetMinutes(clampOffset(parseInt(e.target.value)))} placeholder="0" step="1" min="-240" max="780" class="input" />
                </Show>
                <Show when={!loaded()}>
                  <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
                </Show>
                <div class="fieldset-label">Minutes after (+) / before (-) sunrise to open door (range -240 to 780, default: 0). The door never opens at or after sunset.</div>
              </fieldset>

              <fieldset class="fieldset">
                <legend class="fieldset-legend">Days of Week</legend>
                <div class="flex flex-wrap gap-2">
                  {['Sun','Mon','Tue','Wed','Thu','Fri','Sat'].map((d, i) => (
                    <button
                      type="button"
                      class={`btn btn-sm ${doorAutoOpenDays()[i] ? 'btn-primary' : 'btn-outline'}`}
                      onClick={() => {
                        const arr = doorAutoOpenDays().slice()
                        arr[i] = !arr[i]
                        setDoorAutoOpenDays(arr)
                      }}
                    >
                      {d}
                    </button>
                  ))}
                </div>
                <div class="fieldset-label">Days the door auto-opens (default: every day)</div>
              </fieldset>
            </div>
          </Show>

          {/* Auto-Close */}
          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Automatic Close (Sunset)</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable Automatic Close</span>
                <input
                  type="checkbox"
                  class="toggle toggle-primary"
                  checked={doorAutoCloseEnabled() ?? false}
                  onChange={(e) => setDoorAutoCloseEnabled(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Close the door automatically at sunset + offset on the selected days
                </span>
              </label>
            </div>
          </fieldset>

          <Show when={doorAutoCloseEnabled()}>
            <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Sunset Offset (minutes)</legend>
                <Show when={loaded()}>
                  <input type="number" value={doorAutoCloseOffsetMinutes()!} onInput={(e) => setDoorAutoCloseOffsetMinutes(clampOffset(parseInt(e.target.value)))} placeholder="0" step="1" min="-240" max="780" class="input" />
                </Show>
                <Show when={!loaded()}>
                  <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
                </Show>
                <div class="fieldset-label">Minutes after (+) / before (-) sunset to close door (range -240 to 780, default: 0)</div>
              </fieldset>

              <fieldset class="fieldset">
                <legend class="fieldset-legend">Days of Week</legend>
                <div class="flex flex-wrap gap-2">
                  {['Sun','Mon','Tue','Wed','Thu','Fri','Sat'].map((d, i) => (
                    <button
                      type="button"
                      class={`btn btn-sm ${doorAutoCloseDays()[i] ? 'btn-primary' : 'btn-outline'}`}
                      onClick={() => {
                        const arr = doorAutoCloseDays().slice()
                        arr[i] = !arr[i]
                        setDoorAutoCloseDays(arr)
                      }}
                    >
                      {d}
                    </button>
                  ))}
                </div>
                <div class="fieldset-label">Days the door auto-closes (default: every day)</div>
              </fieldset>
            </div>
          </Show>

          {/* Timeouts */}
          <h2 class="text-lg font-bold mb-4 mt-10">Door Timeouts</h2>
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
            <fieldset class="fieldset">
              <legend class="fieldset-legend">Door Open Timeout (seconds)</legend>
              <Show when={loaded()}>
                <input type="number" value={doorOpenTimeoutSeconds()!} onInput={(e) => setDoorOpenTimeoutSeconds(parseInt(e.target.value))} placeholder="30" step="1" min="5" max="120" class="input" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">Time to wait before declaring an open timeout (default: 30 seconds)</div>
            </fieldset>

            <fieldset class="fieldset">
              <legend class="fieldset-legend">Door Close Timeout (seconds)</legend>
              <Show when={loaded()}>
                <input type="number" value={doorCloseTimeoutSeconds()!} onInput={(e) => setDoorCloseTimeoutSeconds(parseInt(e.target.value))} placeholder="30" step="1" min="5" max="120" class="input" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">Time to wait before declaring a close timeout (default: 30 seconds)</div>
            </fieldset>
          </div>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Door Lockout</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable Door Lockout</span>
                <input
                  type="checkbox"
                  class="toggle toggle-warning"
                  checked={doorLockoutEnabled() ?? false}
                  onChange={(e) => setDoorLockoutEnabled(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Prevents all door operations (open/close/manual switch/schedule) when enabled
                </span>
              </label>
            </div>
          </fieldset>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Timeout Auto-Calculation</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable Timeout Auto-Calculation</span>
                <input
                  type="checkbox"
                  class="toggle toggle-info"
                  checked={doorTimeoutAutoCalcEnabled() ?? false}
                  onChange={(e) => setDoorTimeoutAutoCalcEnabled(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Automatically adjust door open/close timeouts based on historical operation durations
                </span>
              </label>
            </div>
          </fieldset>

      <h2 class="text-lg font-bold mb-4 mt-10">Location Settings</h2>
      
      <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
        <fieldset class="fieldset">
          <legend class="fieldset-legend">Latitude</legend>
          <Show when={loaded()}>
            <input type="number" value={latitude()!} onInput={(e) => { setLatitude(parseFloat(e.target.value)); fetchSunriseSunsetData() }} placeholder="40.7128" step="0.0001" min="-90" max="90" class="input" />
          </Show>
          <Show when={!loaded()}>
            <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
          </Show>
          <div class="fieldset-label">Latitude for sunrise/sunset calculations (-90 to 90)</div>
        </fieldset>

        <fieldset class="fieldset">
          <legend class="fieldset-legend">Longitude</legend>
          <Show when={loaded()}>
            <input type="number" value={longitude()!} onInput={(e) => { setLongitude(parseFloat(e.target.value)); fetchSunriseSunsetData() }} placeholder="-74.0060" step="0.0001" min="-180" max="180" class="input" />
          </Show>
          <Show when={!loaded()}>
            <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
          </Show>
          <div class="fieldset-label">Longitude for sunrise/sunset calculations (-180 to 180)</div>
        </fieldset>
      </div>

      <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
        <fieldset class="fieldset">
          <legend class="fieldset-legend">Timezone</legend>
          <Show when={loaded()}>
            <select id="timezone_posix" title="Timezone" class="select" value={timezonePosix()} onInput={(e) => { setTimezonePosix((e.target as HTMLSelectElement).value); fetchSunriseSunsetData() }}>
              <option value="HST10">Hawaii (HST, no DST)</option>
              <option value="AKST9AKDT,M3.2.0,M11.1.0">Alaska (AKST/AKDT)</option>
              <option value="PST8PDT,M3.2.0,M11.1.0">Pacific (PST/PDT)</option>
              <option value="MST7">Arizona (MST, no DST)</option>
              <option value="MST7MDT,M3.2.0,M11.1.0">Mountain (MST/MDT)</option>
              <option value="CST6CDT,M3.2.0,M11.1.0">Central (CST/CDT)</option>
              <option value="EST5EDT,M3.2.0,M11.1.0">Eastern (EST/EDT)</option>
              <option value="AST4ADT,M3.2.0,M11.1.0">Atlantic (AST/ADT)</option>
              <option value="GMT0BST,M3.5.0/1,M10.5.0">UK (GMT/BST)</option>
              <option value="CET-1CEST,M3.5.0,M10.5.0/3">Central Europe (CET/CEST)</option>
              <option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Eastern Europe (EET/EEST)</option>
              <option value="IST-5:30">India (IST, no DST)</option>
              <option value="CST-8">China (CST, no DST)</option>
              <option value="JST-9">Japan (JST, no DST)</option>
              <option value="AEST-10AEDT,M10.1.0,M4.1.0/3">Australia Eastern (AEST/AEDT)</option>
              <option value="NZST-12NZDT,M9.5.0,M4.1.0/3">New Zealand (NZST/NZDT)</option>
            </select>
          </Show>
          <Show when={!loaded()}>
            <select class="select input-disabled" disabled><option>--</option></select>
          </Show>
          <div class="fieldset-label">Timezone for sunrise/sunset times (includes automatic DST adjustment)</div>
        </fieldset>

        <fieldset class="fieldset">
          <legend class="fieldset-legend">Get Browser Location</legend>
          <button
            class="btn btn-accent btn-soft w-full"
            onClick={() => {
              if (navigator.geolocation) {
                navigator.geolocation.getCurrentPosition(
                  (position) => {
                    setLatitude(position.coords.latitude);
                    setLongitude(position.coords.longitude);
                    // Auto-detect timezone from browser IANA timezone
                    const ianaToposix: Record<string, string> = {
                      'Pacific/Honolulu': 'HST10',
                      'America/Anchorage': 'AKST9AKDT,M3.2.0,M11.1.0',
                      'America/Los_Angeles': 'PST8PDT,M3.2.0,M11.1.0',
                      'America/Phoenix': 'MST7',
                      'America/Denver': 'MST7MDT,M3.2.0,M11.1.0',
                      'America/Boise': 'MST7MDT,M3.2.0,M11.1.0',
                      'America/Chicago': 'CST6CDT,M3.2.0,M11.1.0',
                      'America/New_York': 'EST5EDT,M3.2.0,M11.1.0',
                      'America/Indiana/Indianapolis': 'EST5EDT,M3.2.0,M11.1.0',
                      'America/Detroit': 'EST5EDT,M3.2.0,M11.1.0',
                      'America/Halifax': 'AST4ADT,M3.2.0,M11.1.0',
                      'Europe/London': 'GMT0BST,M3.5.0/1,M10.5.0',
                      'Europe/Berlin': 'CET-1CEST,M3.5.0,M10.5.0/3',
                      'Europe/Paris': 'CET-1CEST,M3.5.0,M10.5.0/3',
                      'Europe/Helsinki': 'EET-2EEST,M3.5.0/3,M10.5.0/4',
                      'Asia/Kolkata': 'IST-5:30',
                      'Asia/Shanghai': 'CST-8',
                      'Asia/Tokyo': 'JST-9',
                      'Australia/Sydney': 'AEST-10AEDT,M10.1.0,M4.1.0/3',
                      'Pacific/Auckland': 'NZST-12NZDT,M9.5.0,M4.1.0/3',
                    };
                    try {
                      const ianaTz = Intl.DateTimeFormat().resolvedOptions().timeZone;
                      if (ianaTz && ianaToposix[ianaTz]) {
                        setTimezonePosix(ianaToposix[ianaTz]);
                      }
                    } catch (_) { /* ignore */ }
                    fetchSunriseSunsetData();
                  },
                  (error) => {
                    alert(`Location error: ${error.message}`);
                  },
                  { enableHighAccuracy: true, timeout: 10000, maximumAge: 300000 }
                );
              } else {
                alert('Geolocation is not supported by your browser');
              }
            }}
          >
            📍 Get Current Location
          </button>
          <div class="fieldset-label">Use browser GPS to auto-fill location coordinates</div>
        </fieldset>
      </div>

      <div class="card bg-base-200 card-sm shadow-sm mt-4">
        <div class="card-body">
          <h3 class="card-title">Sunrise/Sunset Preview</h3>
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div class="stat">
              <div class="stat-title">Sunrise</div>
              <div class="stat-value text-lg">
                {sunriseData() ? sunriseData().sunrise : '--:-- --'}
              </div>
              <div class="stat-desc">
                {sunriseData() ? `Using coordinates: ${latitude()?.toFixed(4)}°, ${longitude()?.toFixed(4)}°` : 'Preview will show after saving location'}
              </div>
            </div>
            <div class="stat">
              <div class="stat-title">Sunset</div>
              <div class="stat-value text-lg">
                {sunsetData() ? sunsetData().sunset : '--:-- --'}
              </div>
              <div class="stat-desc">
                {sunsetData() ? `Coordinates: ${latitude()?.toFixed(4)}°, ${longitude()?.toFixed(4)}°` : 'Preview will show after saving location'}
              </div>
              <div class="stat-desc">
                {sunsetData() ? `Timezone: ${getTimezoneDisplay()}` : ''}
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Weather-Based Door Opening */}
      <h2 class="text-lg font-bold mb-4 mt-10">Weather</h2>
      <div class="card bg-base-200 card-sm shadow-sm mt-4">
        <div class="card-body">
          <h2 class="card-title">Weather-Based Door Opening</h2>
          <div class="fieldset-label">
            When enabled, the automatic door open (sunrise) checks current conditions and the
            short-term forecast from OpenWeatherMap first. If the weather is inclement
            (rain, snow, storms, high wind, or extreme cold), opening is postponed and
            rechecked about once an hour until the weather clears or the auto-close/sunset
            time arrives. Auto-close is never blocked. Uses your Location coordinates above.
          </div>
          <fieldset class="fieldset">
            <legend class="fieldset-legend">Enable Weather Check</legend>
            <label class="label cursor-pointer justify-start gap-2">
              <span class="label-text">Gate automatic opening on weather</span>
              <Show when={loaded()}>
                <input type="checkbox" class="toggle toggle-accent"
                  checked={weatherEnabled() ?? false}
                  onChange={(e) => setWeatherEnabled(e.currentTarget.checked)} />
              </Show>
            </label>
          </fieldset>
          <Show when={weatherEnabled()}>
            <fieldset class="fieldset">
              <legend class="fieldset-legend">OpenWeatherMap API Key</legend>
              <div class="join w-full">
                <input type={showWeatherApiKey() ? 'text' : 'password'}
                  value={weatherApiKey()}
                  onInput={(e) => setWeatherApiKey(e.target.value)}
                  placeholder="Enter API key (from openweathermap.org)"
                  class="input join-item w-full" />
                <button type="button" class="btn join-item"
                  onClick={() => setShowWeatherApiKey(!showWeatherApiKey())}>
                  {showWeatherApiKey() ? 'Hide' : 'Show'}
                </button>
              </div>
              <div class="fieldset-label">Free key from openweathermap.org (Current Weather + 5-day/3-hour forecast). Leave blank to keep existing key.</div>
            </fieldset>
            <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Units</legend>
                <select class="select w-full" value={weatherUnits()}
                  onInput={(e) => setWeatherUnits((e.target as HTMLSelectElement).value)}>
                  <option value="imperial">Imperial (°F, mph)</option>
                  <option value="metric">Metric (°C, m/s)</option>
                  <option value="standard">Standard (K, m/s)</option>
                </select>
              </fieldset>
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Update Interval (minutes)</legend>
                <input type="number" min="5" max="360" step="5"
                  value={weatherUpdateIntervalMinutes() ?? 10}
                  onInput={(e) => setWeatherUpdateIntervalMinutes(parseInt(e.target.value) || 10)}
                  class="input w-full" />
                <div class="fieldset-label">How often to refresh weather (5-360 min, default 10). Kept well within the free tier.</div>
              </fieldset>
            </div>
            <div class="mt-2">
              <button type="button" class="btn btn-accent btn-soft btn-sm"
                onClick={handleTestWeather}
                disabled={weatherTestLoading()}>
                {weatherTestLoading() ? (
                  <><span class="loading loading-spinner loading-xs"></span> Testing...</>
                ) : 'Test Connection'}
              </button>
              <Show when={weatherTestResult()}>
                <span class={`ml-2 text-sm ${weatherTestResult()!.success ? 'text-success' : 'text-error'}`}>
                  {weatherTestResult()!.message}
                </span>
              </Show>
            </div>
          </Show>
        </div>
      </div>

      {/* LLM Weather Decision (issue #6) */}
      <Show when={weatherEnabled()}>
        <div class="card bg-base-200 card-sm shadow-sm mt-4">
          <div class="card-body">
            <h2 class="card-title">LLM Weather Decision</h2>
            <div class="fieldset-label">
              Optionally replace the rule-based weather check above with an LLM. The model is
              asked to judge the forecast for the door's actual open period today — not just
              whether any bad weather appears somewhere in the forecast. Falls back automatically
              to the rule-based check if the LLM is unreachable, disabled, or returns something
              unusable. Works with Ollama Cloud, a local/LAN Ollama or Rapid-MLX instance, or any
              other OpenAI-compatible provider.
            </div>
            <fieldset class="fieldset">
              <legend class="fieldset-legend">Enable LLM Decision</legend>
              <label class="label cursor-pointer justify-start gap-2">
                <span class="label-text">Use an LLM instead of the rule-based check</span>
                <Show when={loaded()}>
                  <input type="checkbox" class="toggle toggle-accent"
                    checked={llmEnabled() ?? false}
                    onChange={(e) => setLlmEnabled(e.currentTarget.checked)} />
                </Show>
              </label>
            </fieldset>
            <Show when={llmEnabled()}>
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Provider</legend>
                <select class="select w-full" value={llmProviderType()}
                  onInput={(e) => setLlmProviderType((e.target as HTMLSelectElement).value)}>
                  <option value="ollama_cloud">Ollama Cloud</option>
                  <option value="openai_compatible">Ollama (local/LAN) or other OpenAI-compatible</option>
                  <option value="ollama_native">Ollama native API (local/LAN, non-OpenAI-compatible)</option>
                </select>
                <div class="fieldset-label">
                  Ollama Cloud and "OpenAI-compatible" both use the standard /v1/chat/completions
                  API (this covers Rapid-MLX and most local servers too). Pick "Ollama native" only
                  if your local Ollama install's OpenAI-compatible endpoint isn't available.
                </div>
              </fieldset>
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Provider URL</legend>
                <input type="text" value={llmBaseUrl()}
                  onInput={(e) => setLlmBaseUrl(e.target.value)}
                  placeholder="http://192.168.1.5:11434 or https://ollama.com"
                  class="input w-full" />
                <div class="fieldset-label">
                  Base URL including port, no trailing slash or path (e.g. http://localhost:8000
                  for a LAN Ollama/Rapid-MLX server, or https://ollama.com for Ollama Cloud).
                </div>
              </fieldset>
              <fieldset class="fieldset">
                <legend class="fieldset-legend">API Key</legend>
                <div class="join w-full">
                  <input type={showLlmApiKey() ? 'text' : 'password'}
                    value={llmApiKey()}
                    onInput={(e) => setLlmApiKey(e.target.value)}
                    placeholder="Leave blank for LAN providers with no auth"
                    class="input join-item w-full" />
                  <button type="button" class="btn join-item"
                    onClick={() => setShowLlmApiKey(!showLlmApiKey())}>
                    {showLlmApiKey() ? 'Hide' : 'Show'}
                  </button>
                </div>
                <div class="fieldset-label">Bearer token for the provider. Optional for local/LAN Ollama. Leave blank to keep existing key.</div>
              </fieldset>
              <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                <fieldset class="fieldset">
                  <legend class="fieldset-legend">Model</legend>
                  <input type="text" value={llmModel()}
                    onInput={(e) => setLlmModel(e.target.value)}
                    placeholder="e.g. llama3.1, gpt-oss:20b"
                    class="input w-full" />
                </fieldset>
                <fieldset class="fieldset">
                  <legend class="fieldset-legend">Timeout (seconds)</legend>
                  <input type="number" min="5" max="60" step="1"
                    value={llmTimeoutSeconds() ?? 15}
                    onInput={(e) => setLlmTimeoutSeconds(parseInt(e.target.value) || 15)}
                    class="input w-full" />
                  <div class="fieldset-label">Per-request timeout (5-60s, default 15).</div>
                </fieldset>
              </div>
              <div class="mt-2">
                <button type="button" class="btn btn-accent btn-soft btn-sm"
                  onClick={handleTestLlmConnection}
                  disabled={llmTestLoading()}>
                  {llmTestLoading() ? (
                    <><span class="loading loading-spinner loading-xs"></span> Testing...</>
                  ) : 'Test Connection'}
                </button>
                <Show when={llmTestResult()}>
                  <span class={`ml-2 text-sm ${llmTestResult()!.success ? 'text-success' : 'text-error'}`}>
                    {llmTestResult()!.message}
                  </span>
                </Show>
              </div>
            </Show>
          </div>
        </div>
      </Show>

      <h2 class="text-lg font-bold mb-4 mt-10">OTA Updates</h2>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Auto-Update Check</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable Auto-Update Check</span>
                <input
                  type="checkbox"
                  class="toggle toggle-primary"
                  checked={autoUpdateEnabled() ?? false}
                  onChange={(e) => setAutoUpdateEnabled(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Automatically check for firmware updates at the configured interval
                </span>
              </label>
            </div>
          </fieldset>

          <Show when={autoUpdateEnabled()}>
            <fieldset class="fieldset mt-4">
              <legend class="fieldset-legend">Check Interval (hours)</legend>
              <Show when={loaded()}>
                <input type="number" value={updateCheckIntervalHours()!} onInput={(e) => setUpdateCheckIntervalHours(parseInt(e.target.value))} placeholder="24" step="1" min="1" max="168" class="input" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">How often to check for updates in hours (1-168, default: 24)</div>
            </fieldset>
          </Show>

          <h2 class="text-lg font-bold mb-4 mt-10">Historical Data Settings</h2>

          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">History Data Collection</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable Historical Data Collection</span>
                <input
                  type="checkbox"
                  class="toggle toggle-primary"
                  checked={historyEnabled() ?? true}
                  onChange={(e) => setHistoryEnabled(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Collect sensor and controller state changes for historical charts. Data is stored in RAM.
                </span>
              </label>
            </div>
          </fieldset>

          <Show when={historyEnabled()}>
            <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Temperature Min Interval (seconds)</legend>
                <Show when={loaded()}>
                  <input type="number" value={historyTempMinIntervalSeconds()!} onInput={(e) => setHistoryTempMinIntervalSeconds(parseInt(e.target.value))} placeholder="60" step="1" min="10" max="3600" class="input" />
                </Show>
                <Show when={!loaded()}>
                  <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
                </Show>
                <div class="fieldset-label">Minimum seconds between temperature recordings (10-3600, default: 60). Only records when change exceeds 0.5°F.</div>
              </fieldset>

              <fieldset class="fieldset">
                <legend class="fieldset-legend">Flow Rate Min Interval (seconds)</legend>
                <Show when={loaded()}>
                  <input type="number" value={historyFlowMinIntervalSeconds()!} onInput={(e) => setHistoryFlowMinIntervalSeconds(parseInt(e.target.value))} placeholder="10" step="1" min="5" max="300" class="input" />
                </Show>
                <Show when={!loaded()}>
                  <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
                </Show>
                <div class="fieldset-label">Minimum seconds between flow rate recordings (5-300, default: 10). Only records on change.</div>
              </fieldset>
            </div>

            <fieldset class="fieldset mt-4">
              <legend class="fieldset-legend">Buffer Size (data points)</legend>
              <Show when={loaded()}>
                <input type="number" value={historyBufferSize()!} onInput={(e) => setHistoryBufferSize(Math.min(500, Math.max(50, parseInt(e.target.value) || 50)))} placeholder="500" step="50" min="50" max="500" class="input" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">Maximum data points to store in RAM (50-500, default: 500). Oldest data is overwritten when full. Pump, light, and door events are captured immediately on any change.</div>
            </fieldset>
          </Show>

          <h2 class="text-lg font-bold mb-4 mt-10">Advanced Settings</h2>

          {/* API Authentication Section */}
          <div class="card bg-base-200 card-sm shadow-sm mt-4">
            <div class="card-body">
              <h2 class="card-title">API Authentication</h2>
              <p class="text-sm opacity-70 mb-4">
                Secure your device's API endpoints with HTTP Basic Authentication.
                When enabled, all control operations will require username and password.
              </p>

              <fieldset class="fieldset">
                <legend class="fieldset-legend">Enable API Authentication</legend>
                <div class="form-control">
                  <label class="label cursor-pointer">
                    <span class="label-text">Require authentication for API access</span>
                    <input
                      type="checkbox"
                      class="toggle toggle-warning"
                      checked={apiAuthEnabled() ?? false}
                      onChange={(e) => setApiAuthEnabled(e.currentTarget.checked)}
                    />
                  </label>
                  <label class="label">
                    <span class="label-text-alt">
                      Protects state-modifying endpoints. Read-only endpoints remain public for monitoring.
                    </span>
                  </label>
                </div>
              </fieldset>

              <Show when={apiAuthEnabled()}>
                <div class="mt-4 space-y-4">
                  <fieldset class="fieldset">
                    <legend class="fieldset-legend">API Username</legend>
                    <input
                      type="text"
                      value={apiUsername()}
                      onInput={(e) => setApiUsername(e.target.value)}
                      placeholder="admin"
                      class="input"
                    />
                    <div class="fieldset-label">Username for API authentication</div>
                  </fieldset>

                  <fieldset class="fieldset">
                    <legend class="fieldset-legend">API Password</legend>
                    <div class="input-group">
                      <input
                        type={showApiPassword() ? "text" : "password"}
                        value={apiPassword()}
                        onInput={(e) => setApiPassword(e.target.value)}
                        placeholder="Enter new password (leave blank to keep current)"
                        class="input"
                      />
                      <button
                        type="button"
                        class="btn btn-ghost"
                        onClick={() => setShowApiPassword(!showApiPassword())}
                        title={showApiPassword() ? "Hide password" : "Show password"}
                      >
                        <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class={showApiPassword() ? "lucide lucide-eye-off" : "lucide lucide-eye"}>
                          {showApiPassword() ? (
                            <>
                              <path d="M9.88 9.88a3 3 0 1 0 4.24 4.24"></path>
                              <path d="M10.73 5.08A10.43 10.43 0 0 1 12 5c7 0 10 7 10 7a13.16 13.16 0 0 1-1.67 2.68"></path>
                              <path d="M6.61 6.61A13.526 13.526 0 0 0 2 12s3 7 10 7a9.74 9.74 0 0 0 5.39-1.61"></path>
                              <line x1="2" x2="22" y1="2" y2="22"></line>
                            </>
                          ) : (
                            <>
                              <path d="M2 12s3-7 10-7 10 7 10 7-3 7-10 7-10-7-10-7Z"></path>
                              <circle cx="12" cy="12" r="3"></circle>
                            </>
                          )}
                        </svg>
                      </button>
                    </div>
                    <div class="fieldset-label">
                      Leave empty to keep existing password. Password is never displayed after saving.
                    </div>
                  </fieldset>

                  <div role="alert" class="alert alert-warning alert-soft">
                    <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                      <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"></path>
                      <line x1="12" y1="9" x2="12" y2="13"></line>
                      <line x1="12" y1="17" x2="12.01" y2="17"></line>
                    </svg>
                    <span>
                      Warning: Save your credentials securely. You will need them for all future API operations.
                      If you forget your password, you'll need physical access to factory reset the device.
                    </span>
                  </div>
                </div>
              </Show>
            </div>
          </div>

          {/* Backup/Restore Section */}
          <div class="card bg-base-200 card-sm shadow-sm mt-4">
            <div class="card-body">
              <h2 class="card-title">Backup & Restore Settings</h2>
              <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div>
                  <button 
                    class="btn btn-accent btn-soft w-full"
                    onClick={handleBackup}
                    disabled={backupLoading()}
                  >
                    {backupLoading() ? (
                      <>
                        <span class="loading loading-spinner loading-xs"></span>
                        Backing Up...
                      </>
                    ) : (
                      <>
                        <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-download">
                          <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
                          <polyline points="7,10 12,15 17,10"></polyline>
                          <line x1="12" y1="15" x2="12" y2="3"></line>
                        </svg>
                        Backup Settings
                      </>
                    )}
                  </button>
                  <div class="fieldset-label mt-2">
                    Download current settings as JSON file for backup
                  </div>
                </div>
                
                <div>
                  <button 
                    class="btn btn-warning btn-soft w-full"
                    onClick={() => setShowRestoreDialog(true)}
                    disabled={restoreLoading()}
                  >
                    {restoreLoading() ? (
                      <>
                        <span class="loading loading-spinner loading-xs"></span>
                        Restoring...
                      </>
                    ) : (
                      <>
                        <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-upload">
                          <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
                          <polyline points="17,8 12,3 7,8"></polyline>
                          <line x1="12" y1="3" x2="12" y2="15"></line>
                        </svg>
                        Restore Settings
                      </>
                    )}
                  </button>
                  <div class="fieldset-label mt-2">
                    Upload settings file to restore configuration
                  </div>
                </div>
              </div>
            </div>
          </div>

          {/* Restore Confirmation Dialog */}
          <Show when={showRestoreDialog()}>
            <div class="modal modal-open">
              <div class="modal-box">
                <h3 class="font-bold text-lg">Restore Settings</h3>
                <p class="py-4">
                  Select a settings file to restore. This will overwrite all current settings.
                </p>
                <div class="form-control">
                  <input 
                    title="Select settings file"
                    type="file" 
                    accept=".json"
                    class="file-input file-input-bordered w-full"
                    onChange={(e) => {
                      const file = (e.target as HTMLInputElement).files?.[0]
                      if (file) {
                        setRestoreFile(file)
                      }
                    }}
                  />
                </div>
                <div class="modal-action">
                  <button class="btn" onClick={() => setShowRestoreDialog(false)}>Cancel</button>
                  <button 
                    class="btn btn-warning" 
                    onClick={handleRestore}
                    disabled={!restoreFile() || restoreLoading()}
                  >
                    {restoreLoading() ? (
                      <>
                        <span class="loading loading-spinner loading-xs"></span>
                        Restoring...
                      </>
                    ) : (
                      'Restore Settings'
                    )}
                  </button>
                </div>
              </div>
            </div>
          </Show>

          {/* Notifications Section */}
          <h2 class="text-lg font-bold mb-4 mt-10">Notifications</h2>

          {/* MQTT */}
          <div class="card bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title">MQTT (Home Assistant)</h2>
              <div class="fieldset-label">Connect to Home Assistant via MQTT for real-time monitoring and control.</div>
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Enable MQTT</legend>
                <label class="label cursor-pointer justify-start gap-2">
                  <span class="label-text">Enable MQTT</span>
                  <Show when={loaded()}>
                    <input type="checkbox" class="toggle toggle-accent"
                      checked={mqttEnabled() ?? false}
                      onChange={(e) => setMqttEnabled(e.currentTarget.checked)} />
                  </Show>
                </label>
              </fieldset>
              <Show when={mqttEnabled()}>
                <fieldset class="fieldset">
                  <legend class="fieldset-legend">Server</legend>
                  <input type="text" value={mqttServer()}
                    onInput={(e) => setMqttServer(e.target.value)}
                    placeholder="e.g., 192.168.1.100"
                    class="input w-full" />
                </fieldset>
                <fieldset class="fieldset">
                  <legend class="fieldset-legend">Port</legend>
                  <input type="number" min="1" max="65535"
                    value={mqttPort() ?? 1883}
                    onInput={(e) => setMqttPort(parseInt(e.target.value) || 1883)}
                    class="input w-full" />
                </fieldset>
                <fieldset class="fieldset">
                  <legend class="fieldset-legend">Username</legend>
                  <input type="text" value={mqttUsername()}
                    onInput={(e) => setMqttUsername(e.target.value)}
                    placeholder="optional"
                    class="input w-full" />
                </fieldset>
                <fieldset class="fieldset">
                  <legend class="fieldset-legend">Password</legend>
                  <input type="password" value={mqttPassword()}
                    onInput={(e) => setMqttPassword(e.target.value)}
                    placeholder="Leave blank to keep current"
                    class="input w-full" />
                </fieldset>
              </Show>
            </div>
          </div>

          {/* Telegram */}
          <div class="card bg-base-200 card-sm shadow-sm mt-4">
            <div class="card-body">
              <h2 class="card-title">Telegram Notifications</h2>
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Enable Telegram</legend>
                <label class="label cursor-pointer justify-start gap-2">
                  <span class="label-text">Enable Telegram Notifications</span>
                  <Show when={loaded()}>
                    <input type="checkbox" class="toggle toggle-accent"
                      checked={telegramEnabled() ?? false}
                      onChange={(e) => setTelegramEnabled(e.currentTarget.checked)} />
                  </Show>
                </label>
              </fieldset>
              <Show when={telegramEnabled()}>
                <fieldset class="fieldset">
                  <legend class="fieldset-legend">Bot Token</legend>
                  <div class="join w-full">
                    <input type={showTelegramToken() ? 'text' : 'password'}
                      value={telegramBotToken()}
                      onInput={(e) => setTelegramBotToken(e.target.value)}
                      placeholder="Enter bot token (from @BotFather)"
                      class="input join-item w-full" />
                    <button type="button" class="btn join-item"
                      onClick={() => setShowTelegramToken(!showTelegramToken())}>
                      {showTelegramToken() ? 'Hide' : 'Show'}
                    </button>
                  </div>
                  <div class="fieldset-label">Get a bot token from @BotFather on Telegram. Leave blank to keep existing token.</div>
                </fieldset>
                <fieldset class="fieldset">
                  <legend class="fieldset-legend">Chat ID</legend>
                  <input type="text" value={telegramChatId()}
                    onInput={(e) => setTelegramChatId(e.target.value)}
                    placeholder="Enter chat ID"
                    class="input w-full" />
                  <div class="fieldset-label">Your Telegram chat ID. Send /start to your bot, then forward the message to @userinfobot to find your ID.</div>
                </fieldset>
                <fieldset class="fieldset">
                  <legend class="fieldset-legend">Command Polling Interval (seconds)</legend>
                  <input type="number" min="10" max="300" step="5"
                    value={telegramPollingInterval() ?? 20}
                    onInput={(e) => setTelegramPollingInterval(parseInt(e.target.value) || 20)}
                    class="input w-full" />
                  <div class="fieldset-label">How often to check for bot commands (10-300 seconds). Lower = faster response, higher = less network usage.</div>
                </fieldset>
                <div class="mt-2">
                  <button type="button" class="btn btn-accent btn-soft btn-sm"
                    onClick={handleTestTelegram}
                    disabled={telegramTestLoading()}>
                    {telegramTestLoading() ? (
                      <><span class="loading loading-spinner loading-xs"></span> Testing...</>
                    ) : 'Send Test Message'}
                  </button>
                  <Show when={telegramTestResult()}>
                    <span class={`ml-2 text-sm ${telegramTestResult()!.success ? 'text-success' : 'text-error'}`}>
                      {telegramTestResult()!.message}
                    </span>
                  </Show>
                </div>
              </Show>
            </div>
          </div>

          {/* Email */}
          <div class="card bg-base-200 card-sm shadow-sm mt-4">
            <div class="card-body">
              <h2 class="card-title">Email Notifications</h2>
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Enable Email</legend>
                <label class="label cursor-pointer justify-start gap-2">
                  <span class="label-text">Enable Email Notifications</span>
                  <Show when={loaded()}>
                    <input type="checkbox" class="toggle toggle-accent"
                      checked={emailEnabled() ?? false}
                      onChange={(e) => setEmailEnabled(e.currentTarget.checked)} />
                  </Show>
                </label>
              </fieldset>
              <Show when={emailEnabled()}>
                <fieldset class="fieldset">
                  <legend class="fieldset-legend">SMTP Server</legend>
                  <input type="text" value={emailSmtpServer()}
                    onInput={(e) => setEmailSmtpServer(e.target.value)}
                    placeholder="in-v3.mailjet.com"
                    class="input w-full" />
                  <div class="fieldset-label">SMTP server hostname (e.g., in-v3.mailjet.com, smtp.gmail.com). Port 587 uses STARTTLS, port 465 uses direct TLS.</div>
                </fieldset>
                <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                  <fieldset class="fieldset">
                    <legend class="fieldset-legend">SMTP Username</legend>
                    <input type="text" value={emailSmtpUsername()}
                      onInput={(e) => setEmailSmtpUsername(e.target.value)}
                      placeholder="your-smtp-username"
                      class="input w-full" />
                  </fieldset>
                  <fieldset class="fieldset">
                    <legend class="fieldset-legend">SMTP Password</legend>
                    <div class="join w-full">
                      <input type={showEmailPassword() ? 'text' : 'password'}
                        value={emailSmtpPassword()}
                        onInput={(e) => setEmailSmtpPassword(e.target.value)}
                        placeholder="Leave blank to keep existing"
                        class="input join-item w-full" />
                      <button type="button" class="btn join-item"
                        onClick={() => setShowEmailPassword(!showEmailPassword())}>
                        {showEmailPassword() ? 'Hide' : 'Show'}
                      </button>
                    </div>
                  </fieldset>
                </div>
                <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                  <fieldset class="fieldset">
                    <legend class="fieldset-legend">From Email</legend>
                    <input type="email" value={emailFrom()}
                      onInput={(e) => setEmailFrom(e.target.value)}
                      placeholder="coop@yourdomain.com"
                      class="input w-full" />
                  </fieldset>
                  <fieldset class="fieldset">
                    <legend class="fieldset-legend">To Email</legend>
                    <input type="email" value={emailTo()}
                      onInput={(e) => setEmailTo(e.target.value)}
                      placeholder="you@email.com"
                      class="input w-full" />
                  </fieldset>
                </div>
                <div class="mt-2">
                  <button type="button" class="btn btn-accent btn-soft btn-sm"
                    onClick={handleTestEmail}
                    disabled={emailTestLoading()}>
                    {emailTestLoading() ? (
                      <><span class="loading loading-spinner loading-xs"></span> Testing...</>
                    ) : 'Send Test Email'}
                  </button>
                  <Show when={emailTestResult()}>
                    <span class={`ml-2 text-sm ${emailTestResult()!.success ? 'text-success' : 'text-error'}`}>
                      {emailTestResult()!.message}
                    </span>
                  </Show>
                </div>
              </Show>
            </div>
          </div>

          {/* Notification Preferences */}
          <Show when={telegramEnabled() || emailEnabled()}>
            <div class="card bg-base-200 card-sm shadow-sm mt-4">
              <div class="card-body">
                <h2 class="card-title">Alert Preferences</h2>
                <div class="fieldset-label mb-2">Choose which alerts trigger notifications</div>
                <div class="grid grid-cols-1 md:grid-cols-2 gap-2">
                  <label class="label cursor-pointer justify-start gap-2">
                    <input type="checkbox" class="checkbox checkbox-accent checkbox-sm"
                      checked={notifyPumpError() ?? true}
                      onChange={(e) => setNotifyPumpError(e.currentTarget.checked)} />
                    <span class="label-text">Pump / Flow Errors</span>
                  </label>
                  <label class="label cursor-pointer justify-start gap-2">
                    <input type="checkbox" class="checkbox checkbox-accent checkbox-sm"
                      checked={notifySensorError() ?? true}
                      onChange={(e) => setNotifySensorError(e.currentTarget.checked)} />
                    <span class="label-text">Sensor Failures</span>
                  </label>
                  <label class="label cursor-pointer justify-start gap-2">
                    <input type="checkbox" class="checkbox checkbox-accent checkbox-sm"
                      checked={notifyDoorFault() ?? true}
                      onChange={(e) => setNotifyDoorFault(e.currentTarget.checked)} />
                    <span class="label-text">Door Faults</span>
                  </label>
                  <label class="label cursor-pointer justify-start gap-2">
                    <input type="checkbox" class="checkbox checkbox-accent checkbox-sm"
                      checked={notifyWifiDisconnect() ?? false}
                      onChange={(e) => setNotifyWifiDisconnect(e.currentTarget.checked)} />
                    <span class="label-text">WiFi Disconnections</span>
                  </label>
                  <label class="label cursor-pointer justify-start gap-2">
                    <input type="checkbox" class="checkbox checkbox-accent checkbox-sm"
                      checked={notifySystemError() ?? true}
                      onChange={(e) => setNotifySystemError(e.currentTarget.checked)} />
                    <span class="label-text">System Errors (low memory, etc.)</span>
                  </label>
                </div>
              </div>
            </div>
          </Show>

          <h2 class="text-lg font-bold mb-4 mt-10">Danger Zone</h2>
          <div class="card bg-error text-error-content">
            <div class="card-body py-4 px-6">
              <h2 class="card-title">Factory Reset</h2>
              <span class="card-actions mt-2">Factory reset will erase all settings and return to defaults. This action cannot be undone.</span>
              <span class="card-actions mt-4">
                <button 
                  class="btn btn-warning btn-sm"
                  onClick={() => setShowResetDialog(true)}
                >
                  Factory Reset
                </button>
              </span>
            </div>
          </div>
          
          <div class="card bg-warning text-warning-content mt-4">
            <div class="card-body py-4 px-6">
              <h2 class="card-title">System Reboot</h2>
              <span class="card-actions mt-2">Reboot ESP32 device. This will restart system and reconnect to WiFi.</span>
              <span class="card-actions mt-4">
                <button 
                  class="btn btn-error btn-sm"
                  onClick={() => setShowRebootDialog(true)}
                >
                  Reboot Device
                </button>
              </span>
            </div>
          </div>

          {/* Confirmation Dialog */}
          <Show when={showResetDialog()}>
            <div class="modal modal-open">
              <div class="modal-box">
                <h3 class="font-bold text-lg">Confirm Factory Reset</h3>
                <p class="py-4">
                  This will permanently delete all settings, WiFi credentials, and configurations.
                  The device will restart in AP mode with default settings.
                </p>
                <div class="modal-action">
                  <button class="btn" onClick={() => setShowResetDialog(false)}>Cancel</button>
                  <button 
                    class="btn btn-error" 
                    onClick={() => {
                      setShowResetDialog(false)
                      handleFactoryReset()
                    }}
                  >
                    Yes, Reset Everything
                  </button>
                </div>
              </div>
            </div>
          </Show>

          {/* Reboot Confirmation Dialog */}
          <Show when={showRebootDialog()}>
            <div class="modal modal-open">
              <div class="modal-box">
                <h3 class="font-bold text-lg">Confirm System Reboot</h3>
                <p class="py-4">
                  The ESP32 device will restart and reconnect to WiFi.
                  Current settings will be preserved.
                </p>
                <div class="modal-action">
                  <button class="btn" onClick={() => setShowRebootDialog(false)}>Cancel</button>
                  <button 
                    class="btn btn-error" 
                    onClick={() => {
                      setShowRebootDialog(false)
                      handleReboot()
                    }}
                  >
                    Yes, Reboot Device
                  </button>
                </div>
              </div>
            </div>
          </Show>

          {/* Save button is now floating - see fixed button above */}
        </div>
      )}
    </div>
  )
}

export default Settings