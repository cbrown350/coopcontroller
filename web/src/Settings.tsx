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
  const [doorAutoMode, setDoorAutoMode] = createSignal<boolean | null>(null)
  const [doorOpenTimeoutSeconds, setDoorOpenTimeoutSeconds] = createSignal<number | null>(null)
  const [doorCloseTimeoutSeconds, setDoorCloseTimeoutSeconds] = createSignal<number | null>(null)
  const [sunriseOffsetMinutes, setSunriseOffsetMinutes] = createSignal<number | null>(null)
  const [sunsetOffsetMinutes, setSunsetOffsetMinutes] = createSignal<number | null>(null)
  
  // Location settings
  const [latitude, setLatitude] = createSignal<number | null>(null)
  const [longitude, setLongitude] = createSignal<number | null>(null)
  const [timezoneOffsetHours, setTimezoneOffsetHours] = createSignal<number | null>(null)
  
  // Sunrise/sunset data for preview
  // Helper function to get timezone display
  const getTimezoneDisplay = () => {
    const offset = timezoneOffsetHours() ?? 0;
    return `UTC${offset >= 0 ? '+' : ''}${offset}`;
  };
  const [sunriseData, setSunriseData] = createSignal<any>(null)
  const [sunsetData, setSunsetData] = createSignal<any>(null)
  
  // Door lockout and auto-calc
  const [doorLockoutEnabled, setDoorLockoutEnabled] = createSignal<boolean | null>(null)
  const [doorTimeoutAutoCalcEnabled, setDoorTimeoutAutoCalcEnabled] = createSignal<boolean | null>(null)

  // Task 3.5k preparation settings
  const [doorAutoCloseAfterSunsetEnabled, setDoorAutoCloseAfterSunsetEnabled] = createSignal<boolean | null>(null)
  const [doorAutoCloseAfterSunsetMinutes, setDoorAutoCloseAfterSunsetMinutes] = createSignal<number | null>(null)

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
      setPumpMinDailyCyclesEnabled(settings.pump_min_daily_cycles_enabled ?? false)
      setPumpMinDailyCycles(settings.pump_min_daily_cycles ?? 3)
      setPumpMinCycleRunSeconds(settings.pump_min_cycle_run_seconds ?? 120)
      setLogLevel(settings.log_level ?? 'INFO')
      setPulsesPerGallon(settings.pulses_per_gallon ?? null)
      setWifiLedEnabled(settings.wifi_led_enabled ?? true)
      setBuzzerEnabled(settings.buzzer_enabled ?? true)
      setBuzzerType(settings.buzzer_type ?? 'ACTIVE')
      
      // Load door control settings
      setDoorAutoMode(settings.door_auto_mode ?? false)
      setDoorOpenTimeoutSeconds(settings.door_open_timeout_seconds ?? 30)
      setDoorCloseTimeoutSeconds(settings.door_close_timeout_seconds ?? 30)
      setSunriseOffsetMinutes(settings.sunrise_offset_minutes ?? 0)
      setSunsetOffsetMinutes(settings.sunset_offset_minutes ?? 0)
      
      // Load location settings
      setLatitude(settings.latitude ?? 40.7128)
      setLongitude(settings.longitude ?? -74.0060)
      setTimezoneOffsetHours(settings.timezone_offset_hours ?? -5)
      
      // Load door lockout and auto-calc settings
      setDoorLockoutEnabled(settings.door_lockout_enabled ?? false)
      setDoorTimeoutAutoCalcEnabled(settings.door_timeout_auto_calc_enabled ?? false)

      // Load Task 3.5k preparation settings
      setDoorAutoCloseAfterSunsetEnabled(settings.door_auto_close_after_sunset_enabled ?? false)
      setDoorAutoCloseAfterSunsetMinutes(settings.door_auto_close_after_sunset_minutes ?? 0)

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
        pump_min_daily_cycles_enabled: pumpMinDailyCyclesEnabled() ?? false,
        pump_min_daily_cycles: pumpMinDailyCycles() ?? 3,
        pump_min_cycle_run_seconds: pumpMinCycleRunSeconds() ?? 120,
        wifi_led_enabled: wifiLedEnabled() ?? true,
        buzzer_enabled: buzzerEnabled() ?? true,
        buzzer_type: buzzerType() ?? 'ACTIVE',
        door_auto_mode: doorAutoMode() ?? false,
        door_open_timeout_seconds: doorOpenTimeoutSeconds() ?? 30,
        door_close_timeout_seconds: doorCloseTimeoutSeconds() ?? 30,
        sunrise_offset_minutes: sunriseOffsetMinutes() ?? 0,
        sunset_offset_minutes: sunsetOffsetMinutes() ?? 0,
        latitude: latitude() ?? 40.7128,
        longitude: longitude() ?? -74.0060,
        timezone_offset_hours: timezoneOffsetHours() ?? -5,
        door_lockout_enabled: doorLockoutEnabled() ?? false,
        door_timeout_auto_calc_enabled: doorTimeoutAutoCalcEnabled() ?? false,
        door_auto_close_after_sunset_enabled: doorAutoCloseAfterSunsetEnabled() ?? false,
        door_auto_close_after_sunset_minutes: isNaN(doorAutoCloseAfterSunsetMinutes()!) ? 0 : doorAutoCloseAfterSunsetMinutes()! ?? 0,
        log_level: logLevel() ?? 'INFO',
        api_auth_enabled: apiAuthEnabled() ?? false,
        api_username: apiUsername() ?? 'admin',
        wifi_bssid_preference: wifiBssidPreference() ?? '',
        syslog_server: syslogServer() ?? '',
        syslog_port: syslogPort() ?? 514,
        flow_calculation_interval_seconds: flowCalculationIntervalSeconds() ?? 60
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
      const formData = new FormData()
      formData.append('confirm', 'RESET')

      const response = await authenticatedFetch('/factory_reset', {
        method: 'POST',
        body: formData
      })
      
      if (response.ok) {
        alert('Factory reset complete! Device is restarting...')
        // Reload page after a delay
        setTimeout(() => window.location.reload(), 5000)
      } else {
        const error = await response.text()
        alert(`Factory reset failed: ${error}`)
      }
    } catch (error) {
      alert(`Factory reset error: ${error}`)
    }
  }

  const handleReboot = async () => {
    try {
      const formData = new FormData()
      formData.append('confirm', 'REBOOT')

      const response = await authenticatedFetch('/reboot', {
        method: 'POST',
        body: formData
      })
      
      if (response.ok) {
        alert('Device is rebooting...')
        // Reload page after a delay
        setTimeout(() => window.location.reload(), 5000)
      } else {
        const error = await response.text()
        alert(`Reboot failed: ${error}`)
      }
    } catch (error) {
      alert(`Reboot error: ${error}`)
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

          <h2 class="text-lg font-bold mb-4">Wifi Settings</h2>
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

          <h2 class="text-lg font-bold mb-4 mt-10">Door Control Settings</h2>
          
          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Door Auto Mode</legend>
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable Automatic Door Control</span>
                <input
                  type="checkbox"
                  class="toggle toggle-primary"
                  checked={doorAutoMode() ?? false}
                  onChange={(e) => setDoorAutoMode(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">
                  Enable automatic door opening/closing based on sunrise/sunset schedule
                </span>
              </label>
            </div>
          </fieldset>

          <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
            <fieldset class="fieldset">
              <legend class="fieldset-legend">Door Open Timeout (seconds)</legend>
              <Show when={loaded()}>
                <input type="number" value={doorOpenTimeoutSeconds()!} onInput={(e) => setDoorOpenTimeoutSeconds(parseInt(e.target.value))} placeholder="30" step="1" min="5" max="120" class="input" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">Time to wait before declaring open timeout (default: 10 seconds)</div>
            </fieldset>

            <fieldset class="fieldset">
              <legend class="fieldset-legend">Door Close Timeout (seconds)</legend>
              <Show when={loaded()}>
                <input type="number" value={doorCloseTimeoutSeconds()!} onInput={(e) => setDoorCloseTimeoutSeconds(parseInt(e.target.value))} placeholder="30" step="1" min="5" max="120" class="input" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">Time to wait before declaring close timeout (default: 10 seconds)</div>
            </fieldset>
          </div>

          <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
            <fieldset class="fieldset">
              <legend class="fieldset-legend">Sunrise Offset (minutes)</legend>
              <Show when={loaded()}>
                <input type="number" value={sunriseOffsetMinutes()!} onInput={(e) => setSunriseOffsetMinutes(parseInt(e.target.value))} placeholder="0" step="1" min="-60" max="60" class="input" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">Minutes before/after sunrise to open door (default: 0)</div>
            </fieldset>

            <fieldset class="fieldset">
              <legend class="fieldset-legend">Sunset Offset (minutes)</legend>
              <Show when={loaded()}>
                <input type="number" value={sunsetOffsetMinutes()!} onInput={(e) => setSunsetOffsetMinutes(parseInt(e.target.value))} placeholder="0" step="1" min="-60" max="60" class="input" />
              </Show>
              <Show when={!loaded()}>
                <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
              </Show>
              <div class="fieldset-label">Minutes before/after sunset to close door (default: 0)</div>
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
          <legend class="fieldset-legend">Timezone Offset</legend>
          <Show when={loaded()}>
            <select id="timezone_offset_hours" title="Timezone Offset" class="select" value={timezoneOffsetHours() ?? -5} onInput={(e) => { setTimezoneOffsetHours(parseInt((e.target as HTMLSelectElement).value)); fetchSunriseSunsetData() }}>
              <option value="-12">UTC-12 (Baker Island)</option>
              <option value="-11">UTC-11 (American Samoa)</option>
              <option value="-10">UTC-10 (Hawaii)</option>
              <option value="-9">UTC-9 (Alaska)</option>
              <option value="-8">UTC-8 (Pacific Time)</option>
              <option value="-7">UTC-7 (Mountain Time)</option>
              <option value="-6">UTC-6 (Central Time)</option>
              <option value="-5">UTC-5 (Eastern Time)</option>
              <option value="-4">UTC-4 (Atlantic Time)</option>
              <option value="-3">UTC-3 (Brazil, Argentina)</option>
              <option value="-2">UTC-2 (Mid-Atlantic)</option>
              <option value="-1">UTC-1 (Azores)</option>
              <option value="0">UTC+0 (London, Dublin)</option>
              <option value="1">UTC+1 (Paris, Berlin)</option>
              <option value="2">UTC+2 (Cairo, Johannesburg)</option>
              <option value="3">UTC+3 (Moscow, Istanbul)</option>
              <option value="4">UTC+4 (Dubai)</option>
              <option value="5">UTC+5 (Pakistan)</option>
              <option value="6">UTC+6 (Bangladesh)</option>
              <option value="7">UTC+7 (Bangkok, Jakarta)</option>
              <option value="8">UTC+8 (Beijing, Singapore)</option>
              <option value="9">UTC+9 (Tokyo, Seoul)</option>
              <option value="10">UTC+10 (Sydney)</option>
              <option value="11">UTC+11 (Solomon Islands)</option>
              <option value="12">UTC+12 (New Zealand)</option>
              <option value="13">UTC+13 (Samoa)</option>
              <option value="14">UTC+14 (Kiribati)</option>
            </select>
          </Show>
          <Show when={!loaded()}>
            <select id="timezone_offset_hours-fallback" title="Timezone Offset" class="select input-disabled" disabled>
              <option>--</option>
            </select>
          </Show>
          <div class="fieldset-label">UTC timezone offset for sunrise/sunset calculations</div>
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
                    // Try to guess timezone from browser
                    const offset = -new Date().getTimezoneOffset() / 60;
                    setTimezoneOffsetHours(offset);
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

      <h2 class="text-lg font-bold mb-4 mt-10">Automatic Door Close</h2>
      
      <fieldset class="fieldset mt-4">
        <legend class="fieldset-legend">Auto-Close After Sunset</legend>
        <div class="form-control">
          <label class="label cursor-pointer">
            <span class="label-text">Enable Auto-Close After Sunset</span>
            <input
              type="checkbox"
              class="toggle toggle-primary"
              checked={doorAutoCloseAfterSunsetEnabled() ?? false}
              onChange={(e) => setDoorAutoCloseAfterSunsetEnabled(e.currentTarget.checked)}
            />
          </label>
          <label class="label">
            <span class="label-text-alt">
              Enable automatic door closing X minutes after sunset (separate from sunset offset)
            </span>
          </label>
        </div>
      </fieldset>

      <fieldset class="fieldset mt-4">
        <legend class="fieldset-legend">Auto-Close Delay (minutes)</legend>
        <Show when={loaded()}>
          <input type="number" value={doorAutoCloseAfterSunsetMinutes()!} onInput={(e) => setDoorAutoCloseAfterSunsetMinutes(parseInt(e.target.value))} placeholder="0" step="1" min="0" max="120" class="input" />
        </Show>
        <Show when={!loaded()}>
          <input type="text" value="--" placeholder="--" disabled class="input input-disabled" />
        </Show>
        <div class="fieldset-label">Minutes after sunset to auto-close door (default: 0 = immediate)</div>
      </fieldset>

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