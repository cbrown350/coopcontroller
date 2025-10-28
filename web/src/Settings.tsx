import { createSignal, onMount, Show } from 'solid-js'

function Settings() {
  const [ssid, setSsid] = createSignal('')
  const [password, setPassword] = createSignal('')
  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal('')
  const [saveSuccess, setSaveSuccess] = createSignal(false)
  const [apMode, setApMode] = createSignal<boolean | null>(null);
  
  // Coop controller settings
  const [tempThresholdF, setTempThresholdF] = createSignal(34.0);
  const [pumpOnTimeSeconds, setPumpOnTimeSeconds] = createSignal(300);
  const [pumpOffTimeSeconds, setPumpOffTimeSeconds] = createSignal(600);
  const [pumpAutoMode, setPumpAutoMode] = createSignal(true);
  const [lightAutoMode, setLightAutoMode] = createSignal(false);
  const [lightOnHour, setLightOnHour] = createSignal(6);
  const [lightOffHour, setLightOffHour] = createSignal(20);
  const [debugEnabled, setDebugEnabled] = createSignal(false);

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

      setSsid(settings.ssid || '')
      // Password won't be loaded from server for security
      setPassword('')
      setApMode(settings.ap_mode || null)
      
      // Load coop controller settings
      setTempThresholdF(settings.temp_threshold_f || 34.0)
      setPumpOnTimeSeconds(settings.pump_on_time_seconds || 300)
      setPumpOffTimeSeconds(settings.pump_off_time_seconds || 600)
      setPumpAutoMode(settings.pump_auto_mode !== undefined ? settings.pump_auto_mode : true)
      setLightAutoMode(settings.light_auto_mode !== undefined ? settings.light_auto_mode : false)
      setLightOnHour(settings.light_on_hour || 6)
      setLightOffHour(settings.light_off_hour || 20)
      setDebugEnabled(settings.debug_enabled !== undefined ? settings.debug_enabled : false)

      setError('')
    } catch (err: any) {
      setError(`Error loading settings: ${err.message || 'Unknown error'}`)
      console.error('Failed to load settings:', err)
    } finally {
      setLoading(false)
    }
  })


  const handleSave = async () => {
    try {
      setSaveSuccess(false)
      setError('')

      const settings = {
        ssid: ssid(),
        passwd: password(),
        ap_mode: false,
        // Coop controller settings
        temp_threshold_f: parseFloat(tempThresholdF().toString()),
        pump_on_time_seconds: parseInt(pumpOnTimeSeconds().toString()),
        pump_off_time_seconds: parseInt(pumpOffTimeSeconds().toString()),
        pump_auto_mode: pumpAutoMode(),
        light_auto_mode: lightAutoMode(),
        light_on_hour: parseInt(lightOnHour().toString()),
        light_off_hour: parseInt(lightOffHour().toString()),
        debug_enabled: debugEnabled(),
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

          <h2 class="text-lg font-bold mb-4 mt-10">Device Settings</h2>

          <h2 class="text-lg font-bold mb-4 mt-10">Coop Controller Settings</h2>

          {/* Temperature Threshold */}
          <fieldset class="fieldset">
            <legend class="fieldset-legend">Temperature Threshold (°F)</legend>
            <input
              type="number"
              id="temp_threshold_f"
              value={tempThresholdF()}
              onInput={(e) => setTempThresholdF(parseFloat(e.target.value))}
              placeholder="34"
              step="0.1"
              min="0"
              max="100"
              class="input"
            />
            <div class="fieldset-label">When temperature falls below this value, pump cycling will activate</div>
          </fieldset>

          {/* Pump Settings */}
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
            <fieldset class="fieldset">
              <legend class="fieldset-legend">Pump ON Time</legend>
              <input
                type="number"
                id="pump_on_time_seconds"
                value={pumpOnTimeSeconds()}
                onInput={(e) => setPumpOnTimeSeconds(parseInt(e.target.value))}
                placeholder="300"
                min="10"
                max="3600"
                class="input"
              />
              <div class="fieldset-label">Seconds ({formatTime(pumpOnTimeSeconds())})</div>
            </fieldset>

            <fieldset class="fieldset">
              <legend class="fieldset-legend">Pump OFF Time</legend>
              <input
                type="number"
                id="pump_off_time_seconds"
                value={pumpOffTimeSeconds()}
                onInput={(e) => setPumpOffTimeSeconds(parseInt(e.target.value))}
                placeholder="600"
                min="10"
                max="3600"
                class="input"
              />
              <div class="fieldset-label">Seconds ({formatTime(pumpOffTimeSeconds())})</div>
            </fieldset>
          </div>

          {/* Auto Mode Settings */}
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
            <fieldset class="fieldset">
              <legend class="fieldset-legend">Pump Auto Mode</legend>
              <label class="label cursor-pointer">
                <input
                  type="checkbox"
                  id="pump_auto_mode"
                  checked={pumpAutoMode()}
                  onChange={(e) => setPumpAutoMode(e.target.checked)}
                  class="checkbox checkbox-accent"
                />
                <span class="label-text">Enable automatic pump control based on temperature threshold</span>
              </label>
            </fieldset>

            <fieldset class="fieldset">
              <legend class="fieldset-legend">Light Auto Mode</legend>
              <label class="label cursor-pointer">
                <input
                  type="checkbox"
                  id="light_auto_mode"
                  checked={lightAutoMode()}
                  onChange={(e) => setLightAutoMode(e.target.checked)}
                  class="checkbox checkbox-accent"
                />
                <span class="label-text">Enable automatic light control (future feature)</span>
              </label>
            </fieldset>
          </div>

          {/* Light Schedule */}
          <Show when={lightAutoMode()}>
            <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
              <fieldset class="fieldset">
                <legend class="fieldset-legend">Light ON Hour</legend>
                <input
                  type="number"
                  id="light_on_hour"
                  value={lightOnHour()}
                  onInput={(e) => setLightOnHour(parseInt(e.target.value))}
                  placeholder="6"
                  min="0"
                  max="23"
                  class="input"
                />
                <div class="fieldset-label">24-hour format (0-23)</div>
              </fieldset>

              <fieldset class="fieldset">
                <legend class="fieldset-legend">Light OFF Hour</legend>
                <input
                  type="number"
                  id="light_off_hour"
                  value={lightOffHour()}
                  onInput={(e) => setLightOffHour(parseInt(e.target.value))}
                  placeholder="20"
                  min="0"
                  max="23"
                  class="input"
                />
                <div class="fieldset-label">24-hour format (0-23)</div>
              </fieldset>
            </div>
          </Show>

          {/* Debug Settings */}
          <h2 class="text-lg font-bold mb-4 mt-10">Debug Settings</h2>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Debug Mode</legend>
            <label class="label cursor-pointer">
              <input
                type="checkbox"
                id="debug_enabled"
                checked={debugEnabled()}
                onChange={(e) => setDebugEnabled(e.target.checked)}
                class="checkbox checkbox-accent"
              />
              <span class="label-text">Enable debug logging for troubleshooting (shows pin states and sensor activity)</span>
            </label>
          </fieldset>

          <button
            class="btn btn-accent btn-soft mt-10"
            onClick={handleSave}
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