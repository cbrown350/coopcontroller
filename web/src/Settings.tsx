import { createSignal, onMount, Show } from 'solid-js'

function Settings() {
  const [ssid, setSsid] = createSignal('')
  const [password, setPassword] = createSignal('')
  const [loading, setLoading] = createSignal(true)
  const [loaded, setLoaded] = createSignal(false)
  const [error, setError] = createSignal('')
  const [saveSuccess, setSaveSuccess] = createSignal(false)
  const [apMode, setApMode] = createSignal<boolean | null>(null)

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
  const [logLevel, setLogLevel] = createSignal<string | null>(null)

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
           pulses_per_gallon: pulsesPerGallon() ?? 450.0
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
      const formData = new FormData();
      formData.append('confirm', 'RESET');
      
      const response = await fetch('/factory_reset', {
        method: 'POST',
        body: formData
      });
      
      if (response.ok) {
        alert('Factory reset complete! Device is restarting...');
        // Reload page after a delay
        setTimeout(() => window.location.reload(), 5000);
      } else {
        const error = await response.text();
        alert(`Factory reset failed: ${error}`);
      }
    } catch (error) {
      alert(`Factory reset error: ${error}`);
    }
  };

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
                <input type="password" id="password" value={password()} onInput={(e) => setPassword(e.target.value)} placeholder="Enter WiFi password..." class="input" />
              </fieldset>

              <div role="alert" class="mt-4 alert alert-info alert-soft">
                <span>
                  Note: after changing the wifi network you may need to enter a new IP address to get to this device. If the wifi connection fails, the device will revert to AP mode and you can reconnect by connecting to the Wifi network named coopcontroller. If your network supports MDNS discovery you can also find this device at <a class="link link-accent" href="http://coopcontroller.local">coopcontroller.local</a>
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
              <select class="select input-disabled" disabled>
                <option>--</option>
              </select>
            }>
              <select id="log_level" class="select" value={logLevel() ?? 'INFO'} onInput={(e) => setLogLevel((e.target as HTMLSelectElement).value)}>
                <option value="VERBOSE">Verbose</option>
                <option value="DEBUG">Debug</option>
                <option value="INFO">Info</option>
                <option value="WARNING">Warning</option>
                <option value="ERROR">Error</option>
              </select>
            </Show>
            <div class="fieldset-label">Select logging verbosity for diagnostics. Higher verbosity may produce more logs.</div>
          </fieldset>

          <h2 class="text-lg font-bold mb-4 mt-10">Danger Zone</h2>
          <div class="card bg-error text-error-content">
            <div class="card-body">
              <h2 class="card-title">Factory Reset</h2>
              <p>Factory reset will erase all settings and return to defaults. This action cannot be undone.</p>
              <div class="card-actions justify-end">
                <button 
                  class="btn btn-warning"
                  onClick={() => setShowResetDialog(true)}
                >
                  Factory Reset
                </button>
              </div>
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
                      setShowResetDialog(false);
                      handleFactoryReset();
                    }}
                  >
                    Yes, Reset Everything
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