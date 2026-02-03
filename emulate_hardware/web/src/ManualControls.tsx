import { createSignal, onMount, onCleanup, Show } from 'solid-js'
import { EmulatorStatus } from './types'

function ManualControls() {
  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal('')
  const [status, setStatus] = createSignal<EmulatorStatus | null>(null)
  const [overrideEnabled, setOverrideEnabled] = createSignal(false)

  // Override toggle states (local UI state)
  const [overrideHallOpen, setOverrideHallOpen] = createSignal(false)
  const [overrideHallClose, setOverrideHallClose] = createSignal(false)
  const [overrideDoorFault, setOverrideDoorFault] = createSignal(false)
  const [overrideManualSwitch, setOverrideManualSwitch] = createSignal(false)
  const [overrideWaterPulse1, setOverrideWaterPulse1] = createSignal(false)
  const [overrideWaterPulse2, setOverrideWaterPulse2] = createSignal(false)

  const refreshStatus = async () => {
    try {
      const res = await fetch('/emulator/status')
      if (!res.ok) throw new Error(`Status error: ${res.status}`)

      const data = await res.json()
      setStatus(data)

      // Sync override state from server
      if (data.override) {
        setOverrideEnabled(data.override.enabled)
        setOverrideHallOpen(data.override.hall_open)
        setOverrideHallClose(data.override.hall_close)
        setOverrideDoorFault(data.override.door_fault)
        setOverrideManualSwitch(data.override.manual_switch)
        setOverrideWaterPulse1(data.override.water_pulse_1)
        setOverrideWaterPulse2(data.override.water_pulse_2)
      }

      setLoading(false)
      setError('')
    } catch (err: any) {
      console.error('Failed to fetch status:', err)
      setError(`Error: ${err.message || 'Unknown error'}`)
      setLoading(false)
    }
  }

  onMount(() => {
    refreshStatus()
    const intervalId = setInterval(refreshStatus, 500)
    onCleanup(() => clearInterval(intervalId))
  })

  // API helpers
  const postForm = async (url: string, data: Record<string, string>) => {
    const formData = new FormData()
    Object.entries(data).forEach(([key, value]) => formData.append(key, value))
    await fetch(url, { method: 'POST', body: formData })
  }

  const toggleOverrideMode = async () => {
    const newState = !overrideEnabled()
    await fetch(newState ? '/emulator/override/enable' : '/emulator/override/disable', { method: 'POST' })
    setOverrideEnabled(newState)
  }

  const updateOverride = async (signal: string, value: boolean) => {
    await postForm('/emulator/override/set', { [signal]: value.toString() })
  }

  const clearAllOverrides = async () => {
    await fetch('/emulator/override/clear_all', { method: 'POST' })
    setOverrideHallOpen(false)
    setOverrideHallClose(false)
    setOverrideDoorFault(false)
    setOverrideManualSwitch(false)
    setOverrideWaterPulse1(false)
    setOverrideWaterPulse2(false)
  }

  // Manual switch controls
  const pressSwitch = () => fetch('/emulator/manual_switch/press', { method: 'POST' })
  const releaseSwitch = () => fetch('/emulator/manual_switch/release', { method: 'POST' })
  const pulseSwitch = (duration: number) => postForm('/emulator/manual_switch/pulse', { duration_ms: duration.toString() })
  const longPressSwitch = (duration: number) => postForm('/emulator/manual_switch/long_press', { duration_ms: duration.toString() })

  const formatFrequency = (hz: number) => {
    if (hz <= 0) return 'N/A'
    if (hz >= 1) return `${hz.toFixed(1)} Hz`
    return `${(hz * 1000).toFixed(0)} mHz`
  }

  return (
    <div>
      <Show when={loading()}>
        <p class="flex items-center gap-2">
          Loading status...
          <span class="loading loading-spinner loading-md"></span>
        </p>
      </Show>

      <Show when={error()}>
        <div role="alert" class="alert alert-error mb-4">{error()}</div>
      </Show>

      <Show when={status()}>
        <div class="space-y-4">
          {/* Manual Switch Controls */}
          <div class="card bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title text-warning">Manual Switch Control</h2>
              <p class="text-sm text-base-content/60">Simulate external door button presses</p>

              <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
                {/* Current State */}
                <div class="stat p-3 bg-base-100 rounded-lg">
                  <div class="stat-title text-xs">Current State</div>
                  <div class={`stat-value text-lg ${status()?.emulated.manual_switch?.is_pressed ? 'text-warning' : 'text-base-content/30'}`}>
                    {status()?.emulated.manual_switch?.is_pressed ? 'PRESSED' : 'RELEASED'}
                  </div>
                  <Show when={status()?.emulated.manual_switch}>
                    <div class="stat-desc text-xs">
                      Type: {status()?.emulated.manual_switch?.press_type || 'NONE'} |
                      Duration: {status()?.emulated.manual_switch?.press_duration_ms || 0}ms
                    </div>
                  </Show>
                </div>

                {/* Thresholds */}
                <div class="stat p-3 bg-base-100 rounded-lg">
                  <div class="stat-title text-xs">Thresholds</div>
                  <div class="stat-value text-sm">
                    <div>Short: {status()?.emulated.manual_switch?.short_threshold_ms || 200}ms</div>
                    <div>Long: {status()?.emulated.manual_switch?.long_threshold_ms || 2000}ms</div>
                  </div>
                </div>
              </div>

              {/* Press/Release Controls */}
              <div class="flex flex-wrap gap-2 mt-4">
                <button
                  class="btn btn-warning"
                  onMouseDown={pressSwitch}
                  onMouseUp={releaseSwitch}
                  onMouseLeave={releaseSwitch}
                >
                  Hold to Press
                </button>
                <button class="btn btn-outline" onClick={() => pulseSwitch(200)}>
                  Short Press (200ms)
                </button>
                <button class="btn btn-outline" onClick={() => pulseSwitch(500)}>
                  Medium Press (500ms)
                </button>
                <button class="btn btn-accent" onClick={() => longPressSwitch(2000)}>
                  Long Press (2s)
                </button>
                <button class="btn btn-accent" onClick={() => longPressSwitch(5000)}>
                  Long Press (5s)
                </button>
              </div>
            </div>
          </div>

          {/* Buzzer & LED Pattern Tracking */}
          <div class="card bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title text-info">Signal Pattern Analysis</h2>
              <p class="text-sm text-base-content/60">Real-time pattern detection for buzzer and LED signals</p>

              <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
                {/* Buzzer Pattern */}
                <div class="p-4 bg-base-100 rounded-lg">
                  <h3 class="font-bold mb-2 flex items-center gap-2">
                    <span class={`w-3 h-3 rounded-full ${status()?.monitored.buzzer_active ? 'bg-error animate-pulse' : 'bg-base-300'}`}></span>
                    Buzzer Pattern
                  </h3>
                  <div class="grid grid-cols-2 gap-2 text-sm">
                    <div>Status:</div>
                    <div class={status()?.monitored.buzzer_active ? 'text-error' : ''}>
                      {status()?.monitored.buzzer_active ? 'SOUNDING' : 'SILENT'}
                    </div>
                    <div>Duration:</div>
                    <div>{(status()?.monitored.buzzer_duration_ms || 0) / 1000}s</div>
                    <div>Blinking:</div>
                    <div>{status()?.monitored.buzzer_pattern?.is_blinking ? 'Yes' : 'No'}</div>
                    <div>Frequency:</div>
                    <div>{formatFrequency(status()?.monitored.buzzer_pattern?.frequency_hz || 0)}</div>
                    <div>Period:</div>
                    <div>{status()?.monitored.buzzer_pattern?.period_ms || 0}ms</div>
                    <div>Duty Cycle:</div>
                    <div>{status()?.monitored.buzzer_pattern?.duty_cycle || 0}%</div>
                    <div>Cycles:</div>
                    <div>{status()?.monitored.buzzer_pattern?.cycle_count || 0}</div>
                  </div>
                </div>

                {/* LED Pattern */}
                <div class="p-4 bg-base-100 rounded-lg">
                  <h3 class="font-bold mb-2 flex items-center gap-2">
                    <span class={`w-3 h-3 rounded-full ${status()?.monitored.wifi_led_active ? 'bg-info animate-pulse' : 'bg-base-300'}`}></span>
                    WiFi LED Pattern
                  </h3>
                  <div class="grid grid-cols-2 gap-2 text-sm">
                    <div>Status:</div>
                    <div class={status()?.monitored.wifi_led_active ? 'text-info' : ''}>
                      {status()?.monitored.wifi_led_active ? 'ON' : 'OFF'}
                    </div>
                    <div>Blinking:</div>
                    <div>{status()?.monitored.led_pattern?.is_blinking ? 'Yes' : 'No'}</div>
                    <div>Frequency:</div>
                    <div>{formatFrequency(status()?.monitored.led_pattern?.frequency_hz || 0)}</div>
                    <div>Period:</div>
                    <div>{status()?.monitored.led_pattern?.period_ms || 0}ms</div>
                    <div>On Time:</div>
                    <div>{status()?.monitored.led_pattern?.on_time_ms || 0}ms</div>
                    <div>Off Time:</div>
                    <div>{status()?.monitored.led_pattern?.off_time_ms || 0}ms</div>
                    <div>Duty Cycle:</div>
                    <div>{status()?.monitored.led_pattern?.duty_cycle || 0}%</div>
                    <div>Cycles:</div>
                    <div>{status()?.monitored.led_pattern?.cycle_count || 0}</div>
                  </div>
                </div>
              </div>
            </div>
          </div>

          {/* Manual Override Mode */}
          <div class="card bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <div class="flex items-center justify-between">
                <div>
                  <h2 class="card-title text-error">Manual Override Mode</h2>
                  <p class="text-sm text-base-content/60">
                    Bypass automatic behavior and control all outputs directly
                  </p>
                </div>
                <input
                  type="checkbox"
                  class="toggle toggle-error toggle-lg"
                  checked={overrideEnabled()}
                  onChange={toggleOverrideMode}
                />
              </div>

              <Show when={overrideEnabled()}>
                <div class="alert alert-warning mt-4">
                  <svg xmlns="http://www.w3.org/2000/svg" class="stroke-current shrink-0 h-6 w-6" fill="none" viewBox="0 0 24 24">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
                  </svg>
                  <span>Override mode is ACTIVE. Automatic behaviors are bypassed.</span>
                </div>

                <div class="grid grid-cols-2 md:grid-cols-3 gap-4 mt-4">
                  {/* Hall Open */}
                  <div class="form-control p-3 bg-base-100 rounded-lg">
                    <label class="label cursor-pointer">
                      <span class="label-text">Hall Open Sensor</span>
                      <input
                        type="checkbox"
                        class="toggle toggle-success"
                        checked={overrideHallOpen()}
                        onChange={(e) => {
                          setOverrideHallOpen(e.target.checked)
                          updateOverride('hall_open', e.target.checked)
                        }}
                      />
                    </label>
                    <span class="text-xs text-base-content/50">
                      {overrideHallOpen() ? 'ACTIVE (magnet detected)' : 'INACTIVE'}
                    </span>
                  </div>

                  {/* Hall Close */}
                  <div class="form-control p-3 bg-base-100 rounded-lg">
                    <label class="label cursor-pointer">
                      <span class="label-text">Hall Close Sensor</span>
                      <input
                        type="checkbox"
                        class="toggle toggle-info"
                        checked={overrideHallClose()}
                        onChange={(e) => {
                          setOverrideHallClose(e.target.checked)
                          updateOverride('hall_close', e.target.checked)
                        }}
                      />
                    </label>
                    <span class="text-xs text-base-content/50">
                      {overrideHallClose() ? 'ACTIVE (magnet detected)' : 'INACTIVE'}
                    </span>
                  </div>

                  {/* Door Fault */}
                  <div class="form-control p-3 bg-base-100 rounded-lg">
                    <label class="label cursor-pointer">
                      <span class="label-text">Door Fault Signal</span>
                      <input
                        type="checkbox"
                        class="toggle toggle-error"
                        checked={overrideDoorFault()}
                        onChange={(e) => {
                          setOverrideDoorFault(e.target.checked)
                          updateOverride('door_fault', e.target.checked)
                        }}
                      />
                    </label>
                    <span class="text-xs text-base-content/50">
                      {overrideDoorFault() ? 'FAULT ACTIVE' : 'NO FAULT'}
                    </span>
                  </div>

                  {/* Manual Switch */}
                  <div class="form-control p-3 bg-base-100 rounded-lg">
                    <label class="label cursor-pointer">
                      <span class="label-text">Manual Switch</span>
                      <input
                        type="checkbox"
                        class="toggle toggle-warning"
                        checked={overrideManualSwitch()}
                        onChange={(e) => {
                          setOverrideManualSwitch(e.target.checked)
                          updateOverride('manual_switch', e.target.checked)
                        }}
                      />
                    </label>
                    <span class="text-xs text-base-content/50">
                      {overrideManualSwitch() ? 'PRESSED' : 'RELEASED'}
                    </span>
                  </div>

                  {/* Water Pulse 1 */}
                  <div class="form-control p-3 bg-base-100 rounded-lg">
                    <label class="label cursor-pointer">
                      <span class="label-text">Water Pulse Ch1</span>
                      <input
                        type="checkbox"
                        class="toggle toggle-accent"
                        checked={overrideWaterPulse1()}
                        onChange={(e) => {
                          setOverrideWaterPulse1(e.target.checked)
                          updateOverride('water_pulse_1', e.target.checked)
                        }}
                      />
                    </label>
                    <span class="text-xs text-base-content/50">
                      {overrideWaterPulse1() ? 'LOW (pulse)' : 'HIGH (idle)'}
                    </span>
                  </div>

                  {/* Water Pulse 2 */}
                  <div class="form-control p-3 bg-base-100 rounded-lg">
                    <label class="label cursor-pointer">
                      <span class="label-text">Water Pulse Ch2</span>
                      <input
                        type="checkbox"
                        class="toggle toggle-accent"
                        checked={overrideWaterPulse2()}
                        onChange={(e) => {
                          setOverrideWaterPulse2(e.target.checked)
                          updateOverride('water_pulse_2', e.target.checked)
                        }}
                      />
                    </label>
                    <span class="text-xs text-base-content/50">
                      {overrideWaterPulse2() ? 'LOW (pulse)' : 'HIGH (idle)'}
                    </span>
                  </div>
                </div>

                <div class="mt-4">
                  <button class="btn btn-outline btn-error" onClick={clearAllOverrides}>
                    Clear All Overrides
                  </button>
                </div>
              </Show>
            </div>
          </div>
        </div>
      </Show>
    </div>
  )
}

export default ManualControls
