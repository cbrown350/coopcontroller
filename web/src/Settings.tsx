import { createSignal, onMount, Show } from 'solid-js'

function Settings() {
  const [ssid, setSsid] = createSignal('')
  const [password, setPassword] = createSignal('')
  const [showPassword, setShowPassword] = createSignal(false)
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
  const [lightOffHour, setLightOffHour] = createSignal<number | null>(null)
  const [waterFlowErrorTimeoutSeconds, setWaterFlowErrorTimeoutSeconds] = createSignal<number | null>(null)

  // Water meter calibration
  const [pulsesPerGallon, setPulsesPerGallon] = createSignal<number | null>(null)
  
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
      setLightOffHour(settings.light_off_hour ?? null)
      setWaterFlowErrorTimeoutSeconds(settings.water_flow_error_timeout_seconds ?? null)
      setWaterMeterTimeoutSeconds(settings.water_meter_timeout_seconds ?? null)
      setLogLevel(settings.log_level ?? 'INFO')
      setPulsesPerGallon(settings.pulses_per_gallon ?? null)
      setWifiLedEnabled(settings.wifi_led_enabled ?? true)
      setBuzzerEnabled(settings.buzzer_enabled ?? true)
      setBuzzerType(settings.buzzer_type ?? 'ACTIVE')

      // Set hostname if available
      if (settings.hostname) {
        setHostname(settings.hostname)
      }

      setLoaded(true)
      setError('')
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

  const handleSave = async () => {
    if (!loaded()) {
      setError('Settings not loaded. Please refresh the page.')
      return
    }

    if (!validateThresholds()) return

    try {
      setSaveSuccess(false)
      setError('')

      const settingsPayload = {
        ap_mode: false,
        water_meter_timeout_seconds: waterMeterTimeoutSeconds() ?? 300,
        ...(apMode() && { ssid: ssid(), passwd: password() }),
        temp_threshold_on_f: tempThresholdOnF() ?? 34.0,
        temp_threshold_off_f: tempThresholdOffF() ?? 36.0,
        water_flow_error_timeout_seconds: waterFlowErrorTimeoutSeconds() ?? 120,
        pump_on_time_seconds: pumpOnTimeSeconds() ?? 150,
        pump_off_time_seconds: pumpOffTimeSeconds() ?? 300,
        // include log level string
        log_level: logLevel() ?? 'INFO',
        light_auto_mode: lightAutoMode() ?? false,
        light_on_hour: lightOnHour() ?? 6,
        light_off_hour: lightOffHour() ?? 20,
        pulses_per_gallon: pulsesPerGallon() ?? 450.0,
        wifi_led_enabled: wifiLedEnabled() ?? true,
        buzzer_enabled: buzzerEnabled() ?? true,
        buzzer_type: buzzerType() ?? 'ACTIVE'
      }

      const response = await fetch('/update_settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settingsPayload)
      })

      if (!response.ok) {
        throw new Error(`Failed to save settings: ${response.status} ${response.statusText}`)
      }

      setSaveSuccess(true)
      setTimeout(() => setSaveSuccess(false), 3000)
    } catch (err: any) {
      setError(`Error saving settings: ${err.message || 'Unknown error'}`)
      console.error('Failed to save settings:', err)
    }
  }

  const handleFactoryReset = async () => {
    try {
      const formData = new FormData()
      formData.append('confirm', 'RESET')
      
      const response = await fetch('/factory_reset', {
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
      
      const response = await fetch('/reboot', {
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

      const response = await fetch('/settings/restore', {
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

  return (
    <div class="card">
      {loading() ? (
        <p>Loading settings... <span class="loading loading-spinner loading-xl"></span></p>
      ) : (
        <div>
          {error() && (
            <div role="alert" class="mb-4 alert alert-error">{error()}</div>
          )}

          {saveSuccess() && (
            <div role="alert" class="mb-4 alert alert-success">Settings saved successfully!</div>
          )}

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
                  <input type={showPassword() ? "text" : "password"} id="password" value={password()} onInput={(e) => setPassword(e.target.value)} placeholder="Enter WiFi password..." class="input" />
                  <button 
                    type="button" 
                    class="btn btn-ghost" 
                    onClick={() => setShowPassword(!showPassword())}
                    title={showPassword() ? "Hide password" : "Show password"}
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
              </fieldset>

              <div role="alert" class="mt-4 alert alert-info alert-soft">
                <span>
                  Note: after changing the wifi network you may need to enter a new IP address to get to this device. If the wifi connection fails, the device will revert to AP mode and you can reconnect by connecting to the Wifi network named {hostname()}. If your network supports MDNS discovery you can also find this device at <a class="link link-accent" href={`http://${hostname()}.local`}>{hostname()}.local</a>
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

          <Show when={lightAutoMode()}>
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

          {/* Debug settings removed; logging is controlled via Log Level selector below */}

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

          <h2 class="text-lg font-bold mb-4 mt-10">Advanced Settings</h2>

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
              <span class="card-actions mt-2">Reboot the ESP32 device. This will restart the system and reconnect to WiFi.</span>
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

          <button class="btn btn-accent btn-soft mt-10" onClick={handleSave} disabled={!loaded()}>Save Settings</button>
        </div>
      )}
    </div>
  )
}

export default Settings