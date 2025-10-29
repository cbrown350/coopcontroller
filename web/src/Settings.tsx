import { createSignal, onMount, Show } from 'solid-js'

function Settings() {
  const [ssid, setSsid] = createSignal('')
  const [password, setPassword] = createSignal('')
  const [loading, setLoading] = createSignal(true)
  const [loaded, setLoaded] = createSignal(false)
  const [error, setError] = createSignal('')
  const [saveSuccess, setSaveSuccess] = createSignal(false)
  const [apMode, setApMode] = createSignal<boolean | null>(null);
  
  // Coop controller settings
  const [tempThresholdOnF, setTempThresholdOnF] = createSignal<number | null>(null);
  const [tempThresholdOffF, setTempThresholdOffF] = createSignal<number | null>(null);
  const [pumpOnTimeSeconds, setPumpOnTimeSeconds] = createSignal<number | null>(null);
  const [pumpOffTimeSeconds, setPumpOffTimeSeconds] = createSignal<number | null>(null);
  const [pumpAutoMode, setPumpAutoMode] = createSignal<boolean | null>(null);
  const [lightAutoMode, setLightAutoMode] = createSignal<boolean | null>(null);
  const [lightOnHour, setLightOnHour] = createSignal<number | null>(null);
  const [lightOffHour, setLightOffHour] = createSignal<number | null>(null);
  const [debugEnabled, setDebugEnabled] = createSignal<boolean | null>(null);
  const [waterFlowErrorTimeoutSeconds, setWaterFlowErrorTimeoutSeconds] = createSignal<number | null>(null);
  const [pumpErrorRetrySeconds, setPumpErrorRetrySeconds] = createSignal<number | null>(null);

  // Load settings from the server and scan for WiFi networks
  onMount(async () => {
    try {
      setLoading(true)

      // Load settings
      const response = await fetch('/get_settings')
      if (!response.ok) {
        throw new Error(`Failed to load settings: ${response.status} ${response.statusText}`)
      }
      const settings = await response.json()

      setSsid(settings.ssid ?? '')
      // Password won't be loaded from server for security
      setPassword('')
      setApMode(settings.ap_mode ?? null)
      
      // Load coop controller settings
      setTempThresholdOnF(settings.temp_threshold_on_f ?? null)
      setTempThresholdOffF(settings.temp_threshold_off_f ?? null)
      setPumpOnTimeSeconds(settings.pump_on_time_seconds ?? null)
      setPumpOffTimeSeconds(settings.pump_off_time_seconds ?? null)
      setPumpAutoMode(settings.pump_auto_mode ?? null)
      setLightAutoMode(settings.light_auto_mode ?? null)
      setLightOnHour(settings.light_on_hour ?? null)
      setLightOffHour(settings.light_off_hour ?? null)
      setDebugEnabled(settings.debug_enabled ?? null)
      setWaterFlowErrorTimeoutSeconds(settings.water_flow_error_timeout_seconds ?? null)
      setPumpErrorRetrySeconds(settings.pump_error_retry_seconds ?? null)

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


  const handleSave = async () => {
    if (!loaded()) {
      setError('Settings not loaded. Please refresh the page.')
      return
    }

    // Validate temperature thresholds
    if (!validateThresholds()) {
      return
    }

    try {
      setSaveSuccess(false)
      setError('')

      const settings = {
        ap_mode: false,
        // Conditionally include WiFi settings only when changing WiFi
        ...(apMode() && {
          ssid: ssid(),
          passwd: password()
        }),
        // Coop controller settings
        temp_threshold_on_f: tempThresholdOnF() ?? 34.0,
        temp_threshold_off_f: tempThresholdOffF() ?? 36.0,
        water_flow_error_timeout_seconds: waterFlowErrorTimeoutSeconds() ?? 120,
        pump_error_retry_seconds: pumpErrorRetrySeconds() ?? 120,
        pump_on_time_seconds: pumpOnTimeSeconds() ?? 150,
        pump_off_time_seconds: pumpOffTimeSeconds() ?? 300,
        pump_auto_mode: pumpAutoMode() ?? true,
        light_auto_mode: lightAutoMode() ?? false,
        light_on_hour: lightOnHour() ?? 6,
        light_off_hour: lightOffHour() ?? 20,
        debug_enabled: debugEnabled() ?? false,
      }

      const response = await fetch('/update_settings', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(settings)
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

  const formatTime = (seconds: number) => {
    const minutes = Math.floor(seconds / 60)
    const remainingSeconds = seconds % 60
    return `${minutes}m ${remainingSeconds}s`
  }

  const validateThresholds = () => {
    if (tempThresholdOnF() !== null && tempThresholdOffF() !== null) {
      if (tempThresholdOnF()! > tempThresholdOffF()!) {
        setError('ON threshold must be less than or equal to OFF threshold')
        return false
      } else {
        // Clear error if thresholds are now valid
        setError('')
        return true
      }
    }
    return true
  }

  return (
    <div class="card" >


      {loading() ? (
        <p>Loading settings... <span class="loading loading-spinner loading-xl"></span></p>
      ) : (
        <div>
          {error() && (
            <div role="alert" class="mb-4 alert alert-error">
              {error()}
            </div>
          )}

          {saveSuccess() && (
            <div role="alert" class="mb-4 alert alert-success">
              Settings saved successfully!
            </div>
          )}

          <h2 class="text-lg font-bold mb-4">Wifi Settings</h2>
          {apMode() && (
            <div>
              <fieldset class="fieldset ">
                <legend class="fieldset-legend">SSID</legend>
                <input
                  type="text"
                  id="ssid"
                  value={ssid()}
                  onInput={(e) => setSsid(e.target.value)}
                  placeholder="Enter WiFi network name..."
                  class="input"
                />
              </fieldset>


              <fieldset class="fieldset">
                <legend class="fieldset-legend">Password</legend>
                <input
                  type="password"
                  id="password"
                  value={password()}
                  onInput={(e) => setPassword(e.target.value)}
                  placeholder="Enter WiFi password..."
                  class="input"
                />
              </fieldset>


              <div role="alert" class="mt-4 alert alert-info alert-soft">
                <span>Note: after changing the wifi network you may need to enter a new IP address to get to this device. If the wifi connection fails, the device will revert to AP mode and you can reconnect by connecting to the Wifi network named coopcontroller. If your network supports MDNS discovery you can also find this device at <a class="link link-accent" href="http://coopcontroller.local">
                  coopcontroller.local</a></span>
              </div>
            </div>
          )
          }
          {
            !apMode() && (
              <button class="btn" onClick={() => setApMode(true)}>Change Wifi network</button>
            )
          }

          <h2 class="text-lg font-bold mb-4 mt-10">Coop Controller Settings</h2>

          {/* Temperature Hysteresis */}
          <h2 class="text-lg font-bold mb-4 mt-10">Temperature Control Settings</h2>

          {/* Temperature Hysteresis */}
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
            <fieldset class="fieldset">
              <legend class="fieldset-legend">Temperature ON Threshold (°F)</legend>
              <Show when={loaded()}>
                <input
                  type="number"
                  id="temp_threshold_on_f"
                  value={tempThresholdOnF()!}
                  onInput={(e) => {
                    setTempThresholdOnF(parseFloat(e.target.value))
                    validateThresholds()
                  }}
                  placeholder="34"
                  step="0.1"
                  min="0"
                  max="100"
                  class="input"
                />
              </Show>
              <Show when={!loaded()}>
                <input
                  type="text"
                  placeholder="--"
                  value="--"
                  disabled
                  class="input input-disabled"
                />
              </Show>
              <div class="fieldset-label">When temperature falls below this value, pump cycling will activate</div>
            </fieldset>

            <fieldset class="fieldset">
              <legend class="fieldset-legend">Temperature OFF Threshold (°F)</legend>
              <Show when={loaded()}>
                <input
                  type="number"
                  id="temp_threshold_off_f"
                  value={tempThresholdOffF()!}
                  onInput={(e) => {
                    setTempThresholdOffF(parseFloat(e.target.value))
                    validateThresholds()
                  }}
                  placeholder="36"
                  step="0.1"
                  min="0"
                  max="100"
                  class="input"
                />
              </Show>
              <Show when={!loaded()}>
                <input
                  type="text"
                  value="--"
                  placeholder="--"
                  disabled
                  class="input input-disabled"
                />
              </Show>
              <div class="fieldset-label">When temperature rises above this value, pump cycling will deactivate (hysteresis)</div>
            </fieldset>
          </div>

          {/* Water Flow Error Timeout */}
          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Water Flow Error Timeout (seconds)</legend>
            <Show when={loaded()}>
              <input
                type="number"
                id="water_flow_error_timeout_seconds"
                value={waterFlowErrorTimeoutSeconds()!}
                onInput={(e) => setWaterFlowErrorTimeoutSeconds(parseInt(e.target.value))}
                placeholder="120"
                step="1"
                min="10"
                max="600"
                class="input"
              />
            </Show>
            <Show when={!loaded()}>
              <input
                type="text"
                value="--"
                  placeholder="--"
                disabled
                class="input input-disabled"
              />
            </Show>
            <div class="fieldset-label">Time without water flow before declaring error (default: 120 seconds)</div>
          </fieldset>

          {/* Pump Error Retry Time */}
          <fieldset class="fieldset mt-4">
            <legend class="fieldset-legend">Pump Error Retry Time (seconds)</legend>
            <Show when={loaded()}>
              <input
                type="number"
                id="pump_error_retry_seconds"
                value={pumpErrorRetrySeconds()!}
                onInput={(e) => setPumpErrorRetrySeconds(parseInt(e.target.value))}
                placeholder="120"
                step="1"
                min="10"
                max="600"
                class="input"
              />
            </Show>
            <Show when={!loaded()}>
              <input
                type="text"
                value="--"
                  placeholder="--"
                disabled
                class="input input-disabled"
              />
            </Show>
            <div class="fieldset-label">Time to wait before retrying pump after flow error (default: 120 seconds)</div>
          </fieldset>

          {/* Pump Settings */}
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
            <fieldset class="fieldset">
              <legend class="fieldset-legend">Pump ON Time</legend>
              <Show when={loaded()}>
                <input
                  type="number"
                  id="pump_on_time_seconds"
                  value={pumpOnTimeSeconds()!}
                  onInput={(e) => setPumpOnTimeSeconds(parseInt(e.target.value))}
                  placeholder="300"
                  min="10"
                  max="3600"
                  class="input"
                />
              </Show>
              <Show when={!loaded()}>
                <input
                  type="text"
                  value="--"
                  placeholder="--"
                  disabled
                  class="input input-disabled"
                />
              </Show>
              <div class="fieldset-label">Seconds ({loaded() ? formatTime(pumpOnTimeSeconds()!) : '--'})</div>
            </fieldset>

            <fieldset class="fieldset">
              <legend class="fieldset-legend">Pump OFF Time</legend>
              <Show when={loaded()}>
                <input
                  type="number"
                  id="pump_off_time_seconds"
                  value={pumpOffTimeSeconds()!}
                  onInput={(e) => setPumpOffTimeSeconds(parseInt(e.target.value))}
                  placeholder="600"
                  min="10"
                  max="3600"
                  class="input"
                />
              </Show>
              <Show when={!loaded()}>
                <input
                  type="text"
                  value="--"
                  placeholder="--"
                  disabled
                  class="input input-disabled"
                />
              </Show>
              <div class="fieldset-label">Seconds ({loaded() ? formatTime(pumpOffTimeSeconds()!) : '--'})</div>
            </fieldset>
          </div>

          {/* Auto Mode Settings */}
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
            <fieldset class="fieldset">
              <legend class="fieldset-legend">Pump Auto Mode</legend>
              <label class="label cursor-pointer">
                <Show when={loaded()} fallback={<input type="checkbox" disabled class="checkbox" />}>
                  <input
                    type="checkbox"
                    id="pump_auto_mode"
                    checked={pumpAutoMode()!}
                    onChange={(e) => setPumpAutoMode(e.target.checked)}
                    class="checkbox checkbox-accent"
                  />
                </Show>
                <span class="label-text">Enable automatic pump control based on temperature threshold</span>
              </label>
            </fieldset>

            <fieldset class="fieldset">
              <legend class="fieldset-legend">Light Auto Mode</legend>
              <label class="label cursor-pointer">
                <Show when={loaded()} fallback={<input type="checkbox" disabled class="checkbox" />}>
                  <input
                    type="checkbox"
                    id="light_auto_mode"
                    checked={lightAutoMode()!}
                    onChange={(e) => setLightAutoMode(e.target.checked)}
                    class="checkbox checkbox-accent"
                  />
                </Show>
                <span class="label-text">Enable automatic light control (future feature)</span>
              </label>
            </fieldset>
          </div>

          {/* Light Schedule */}
          <Show when={lightAutoMode()}>
            <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Light ON Hour</legend>
                <Show when={loaded()}>
                  <input
                    type="number"
                    id="light_on_hour"
                    value={lightOnHour()!}
                    onInput={(e) => setLightOnHour(parseInt(e.target.value))}
                    placeholder="6"
                    min="0"
                    max="23"
                    class="input"
                  />
                </Show>
                <Show when={!loaded()}>
                  <input
                    type="text"
                    value="--"
                  placeholder="--"
                    disabled
                    class="input input-disabled"
                  />
                </Show>
                <div class="fieldset-label">24-hour format (0-23)</div>
              </fieldset>

              <fieldset class="fieldset">
                <legend class="fieldset-legend">Light OFF Hour</legend>
                <Show when={loaded()}>
                  <input
                    type="number"
                    id="light_off_hour"
                    value={lightOffHour()!}
                    onInput={(e) => setLightOffHour(parseInt(e.target.value))}
                    placeholder="20"
                    min="0"
                    max="23"
                    class="input"
                  />
                </Show>
                <Show when={!loaded()}>
                  <input
                    type="text"
                    value="--"
                    placeholder="--"
                    disabled
                    class="input input-disabled"
                  />
                </Show>
                <div class="fieldset-label">24-hour format (0-23)</div>
              </fieldset>
            </div>
          </Show>

          {/* Debug Settings */}
          <h2 class="text-lg font-bold mb-4 mt-10">Debug Settings</h2>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Debug Mode</legend>
            <label class="label cursor-pointer">
              <Show when={loaded()} fallback={<input type="checkbox" disabled class="checkbox" />}>
                <input
                  type="checkbox"
                  id="debug_enabled"
                  checked={debugEnabled()!}
                  onChange={(e) => setDebugEnabled(e.target.checked)}
                  class="checkbox checkbox-accent"
                />
              </Show>
              <span class="label-text">Enable debug logging for troubleshooting (shows pin states and sensor activity)</span>
            </label>
          </fieldset>

          <button
            class="btn btn-accent btn-soft mt-10"
            onClick={handleSave}
            disabled={!loaded()}
          >
            Save Settings
          </button>
        </div >
      )
      }
    </div >
  )
}

export default Settings