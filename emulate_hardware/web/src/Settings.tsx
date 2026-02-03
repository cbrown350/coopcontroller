import { createSignal, onMount, Show } from 'solid-js'
import { EmulatorSettings } from './types'

function Settings() {
  const [loading, setLoading] = createSignal(true)
  const [saving, setSaving] = createSignal(false)
  const [error, setError] = createSignal('')
  const [success, setSuccess] = createSignal('')

  // WiFi settings
  const [ssid, setSsid] = createSignal('')
  const [password, setPassword] = createSignal('')
  const [showPassword, setShowPassword] = createSignal(false)
  const [apMode, setApMode] = createSignal(false)

  // Emulator settings
  const [doorTravelTime, setDoorTravelTime] = createSignal(10000)
  const [pulsesPerGallon, setPulsesPerGallon] = createSignal(450)
  const [flowRate, setFlowRate] = createSignal(2.5)
  const [autoSimulateDoor, setAutoSimulateDoor] = createSignal(true)
  const [autoGeneratePulses, setAutoGeneratePulses] = createSignal(true)
  const [logLevel, setLogLevel] = createSignal('INFO')

  // Device info
  const [hostname, setHostname] = createSignal('')
  const [firmwareVersion, setFirmwareVersion] = createSignal('')

  const loadSettings = async () => {
    try {
      setLoading(true)
      const response = await fetch('/get_settings')
      if (!response.ok) {
        throw new Error(`Failed to load settings: ${response.status}`)
      }

      const settings: EmulatorSettings = await response.json()

      setSsid(settings.ssid || '')
      setApMode(settings.ap_mode)
      setDoorTravelTime(settings.door_travel_time_ms)
      setPulsesPerGallon(settings.pulses_per_gallon)
      setFlowRate(settings.flow_rate_gpm)
      setAutoSimulateDoor(settings.auto_simulate_door)
      setAutoGeneratePulses(settings.auto_generate_pulses)
      setLogLevel(settings.log_level)
      setHostname(settings.hostname)
      setFirmwareVersion(settings.firmware_version)

      setError('')
    } catch (err: any) {
      setError(`Error loading settings: ${err.message}`)
    } finally {
      setLoading(false)
    }
  }

  const saveSettings = async () => {
    try {
      setSaving(true)
      setError('')
      setSuccess('')

      const settings: Partial<EmulatorSettings> = {
        ssid: ssid(),
        ap_mode: apMode(),
        door_travel_time_ms: doorTravelTime(),
        pulses_per_gallon: pulsesPerGallon(),
        flow_rate_gpm: flowRate(),
        auto_simulate_door: autoSimulateDoor(),
        auto_generate_pulses: autoGeneratePulses(),
        log_level: logLevel()
      }

      // Only include password if it was changed
      if (password().length > 0) {
        (settings as any).password = password()
      }

      const response = await fetch('/update_settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
      })

      if (!response.ok) {
        const data = await response.json()
        throw new Error(data.error || 'Failed to save settings')
      }

      setSuccess('Settings saved successfully!')
      setPassword('')  // Clear password field after save
      setTimeout(() => setSuccess(''), 3000)
    } catch (err: any) {
      setError(`Error saving settings: ${err.message}`)
    } finally {
      setSaving(false)
    }
  }

  const handleReboot = async () => {
    if (confirm('Are you sure you want to reboot the emulator?')) {
      await fetch('/reboot', { method: 'POST' })
      alert('Emulator is rebooting...')
    }
  }

  const handleFactoryReset = async () => {
    if (confirm('Are you sure you want to factory reset? All settings will be lost.')) {
      await fetch('/factory_reset', { method: 'POST' })
      alert('Factory reset complete. Emulator is rebooting...')
    }
  }

  onMount(() => {
    loadSettings()
  })

  return (
    <div class="space-y-4">
      <h2 class="text-lg font-bold">Emulator Settings</h2>

      <Show when={loading()}>
        <p class="flex items-center gap-2">
          Loading settings...
          <span class="loading loading-spinner loading-md"></span>
        </p>
      </Show>

      <Show when={error()}>
        <div role="alert" class="alert alert-error">{error()}</div>
      </Show>

      <Show when={success()}>
        <div role="alert" class="alert alert-success">{success()}</div>
      </Show>

      <Show when={!loading()}>
        {/* WiFi Settings */}
        <div class="card bg-base-200 card-sm shadow-sm">
          <div class="card-body">
            <h3 class="card-title">WiFi Configuration</h3>

            <Show when={apMode()}>
              <div role="alert" class="alert alert-info mb-4">
                Currently in AP Mode. Connect to "{hostname()}" WiFi network.
              </div>
            </Show>

            <div class="form-control">
              <label class="label">
                <span class="label-text">WiFi SSID</span>
              </label>
              <input
                type="text"
                class="input input-bordered"
                value={ssid()}
                onInput={(e) => setSsid(e.currentTarget.value)}
                placeholder="Enter WiFi network name"
              />
            </div>

            <div class="form-control mt-4">
              <label class="label">
                <span class="label-text">WiFi Password</span>
              </label>
              <div class="join">
                <input
                  type={showPassword() ? 'text' : 'password'}
                  class="input input-bordered join-item flex-1"
                  value={password()}
                  onInput={(e) => setPassword(e.currentTarget.value)}
                  placeholder="Enter new password (leave empty to keep current)"
                />
                <button
                  class="btn join-item"
                  onClick={() => setShowPassword(!showPassword())}
                >
                  {showPassword() ? 'Hide' : 'Show'}
                </button>
              </div>
            </div>

            <div class="form-control mt-4">
              <label class="label cursor-pointer">
                <span class="label-text">Start in AP Mode</span>
                <input
                  type="checkbox"
                  class="toggle"
                  checked={apMode()}
                  onChange={(e) => setApMode(e.currentTarget.checked)}
                />
              </label>
            </div>
          </div>
        </div>

        {/* Emulation Settings */}
        <div class="card bg-base-200 card-sm shadow-sm">
          <div class="card-body">
            <h3 class="card-title">Emulation Defaults</h3>

            <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div class="form-control">
                <label class="label">
                  <span class="label-text">Door Travel Time (seconds)</span>
                </label>
                <input
                  type="number"
                  class="input input-bordered"
                  min="1"
                  max="60"
                  value={doorTravelTime() / 1000}
                  onInput={(e) => setDoorTravelTime(parseInt(e.currentTarget.value) * 1000)}
                />
              </div>

              <div class="form-control">
                <label class="label">
                  <span class="label-text">Pulses Per Gallon</span>
                </label>
                <input
                  type="number"
                  class="input input-bordered"
                  min="100"
                  max="2000"
                  value={pulsesPerGallon()}
                  onInput={(e) => setPulsesPerGallon(parseInt(e.currentTarget.value))}
                />
              </div>

              <div class="form-control">
                <label class="label">
                  <span class="label-text">Flow Rate (GPM)</span>
                </label>
                <input
                  type="number"
                  class="input input-bordered"
                  min="0.1"
                  max="20"
                  step="0.1"
                  value={flowRate()}
                  onInput={(e) => setFlowRate(parseFloat(e.currentTarget.value))}
                />
              </div>

              <div class="form-control">
                <label class="label">
                  <span class="label-text">Log Level</span>
                </label>
                <select
                  class="select select-bordered"
                  value={logLevel()}
                  onChange={(e) => setLogLevel(e.currentTarget.value)}
                >
                  <option value="VERBOSE">Verbose</option>
                  <option value="DEBUG">Debug</option>
                  <option value="INFO">Info</option>
                  <option value="WARNING">Warning</option>
                  <option value="ERROR">Error</option>
                </select>
              </div>
            </div>

            <div class="form-control mt-4">
              <label class="label cursor-pointer">
                <span class="label-text">Auto-simulate door movement</span>
                <input
                  type="checkbox"
                  class="toggle toggle-info"
                  checked={autoSimulateDoor()}
                  onChange={(e) => setAutoSimulateDoor(e.currentTarget.checked)}
                />
              </label>
            </div>

            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Auto-generate water pulses</span>
                <input
                  type="checkbox"
                  class="toggle toggle-info"
                  checked={autoGeneratePulses()}
                  onChange={(e) => setAutoGeneratePulses(e.currentTarget.checked)}
                />
              </label>
            </div>
          </div>
        </div>

        {/* Device Info */}
        <div class="card bg-base-200 card-sm shadow-sm">
          <div class="card-body">
            <h3 class="card-title">Device Information</h3>

            <div class="grid grid-cols-2 gap-4">
              <div>
                <div class="text-sm text-base-content/60">Hostname</div>
                <div class="font-mono">{hostname()}</div>
              </div>
              <div>
                <div class="text-sm text-base-content/60">Firmware Version</div>
                <div class="font-mono">{firmwareVersion()}</div>
              </div>
            </div>
          </div>
        </div>

        {/* Save Button */}
        <div class="flex gap-2">
          <button
            class="btn btn-primary"
            onClick={saveSettings}
            disabled={saving()}
          >
            {saving() ? (
              <>
                <span class="loading loading-spinner loading-sm"></span>
                Saving...
              </>
            ) : (
              'Save Settings'
            )}
          </button>
          <button
            class="btn btn-outline"
            onClick={loadSettings}
          >
            Reload
          </button>
        </div>

        {/* Danger Zone */}
        <div class="card bg-error/10 card-sm shadow-sm mt-8">
          <div class="card-body">
            <h3 class="card-title text-error">Danger Zone</h3>

            <div class="flex gap-2">
              <button
                class="btn btn-warning btn-sm"
                onClick={handleReboot}
              >
                Reboot Emulator
              </button>
              <button
                class="btn btn-error btn-sm"
                onClick={handleFactoryReset}
              >
                Factory Reset
              </button>
            </div>
          </div>
        </div>
      </Show>
    </div>
  )
}

export default Settings
