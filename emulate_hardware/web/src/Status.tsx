import { createSignal, onMount, onCleanup, Show } from 'solid-js'
import { EmulatorStatus, SystemStatus } from './types'

function Status() {
  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal('')
  const [status, setStatus] = createSignal<EmulatorStatus | null>(null)
  const [systemStatus, setSystemStatus] = createSignal<SystemStatus | null>(null)

  const refreshStatus = async () => {
    try {
      const [statusRes, systemRes] = await Promise.all([
        fetch('/emulator/status'),
        fetch('/system_status')
      ])

      if (!statusRes.ok) {
        throw new Error(`Status error: ${statusRes.status}`)
      }

      const statusData = await statusRes.json()
      setStatus(statusData)

      if (systemRes.ok) {
        const systemData = await systemRes.json()
        setSystemStatus(systemData)
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
    const intervalId = setInterval(refreshStatus, 500)  // Fast refresh for real-time feel
    onCleanup(() => clearInterval(intervalId))
  })

  const motorDirectionColor = (dir: string) => {
    switch (dir) {
      case 'OPENING': return 'text-success'
      case 'CLOSING': return 'text-warning'
      case 'BRAKE': return 'text-error'
      default: return 'text-base-content/50'
    }
  }

  const doorStateColor = (state: string) => {
    switch (state) {
      case 'OPEN': return 'text-success'
      case 'CLOSED': return 'text-info'
      case 'OPENING':
      case 'CLOSING': return 'text-warning'
      case 'STOPPED': return 'text-error'
      default: return 'text-base-content/50'
    }
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
          {/* Monitored Signals Section */}
          <div class="card bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title text-warning">Monitored Signals</h2>
              <p class="text-sm text-base-content/60">Reading outputs from main Coop Controller</p>

              <div class="grid grid-cols-2 md:grid-cols-3 gap-4 mt-4">
                {/* Pump */}
                <div class="stat p-3 bg-base-100 rounded-lg">
                  <div class="stat-title text-xs">Pump Relay</div>
                  <div class={`stat-value text-lg ${status()!.monitored.pump_active ? 'text-success' : 'text-base-content/30'}`}>
                    {status()!.monitored.pump_active ? 'ON' : 'OFF'}
                  </div>
                  <div class="stat-desc text-xs">
                    <span class={`inline-block w-2 h-2 rounded-full mr-1 ${status()!.monitored.pump_active ? 'bg-success animate-pulse' : 'bg-base-300'}`}></span>
                    GPIO34 reading
                  </div>
                </div>

                {/* Light */}
                <div class="stat p-3 bg-base-100 rounded-lg">
                  <div class="stat-title text-xs">Light PWM</div>
                  <div class={`stat-value text-lg ${status()!.monitored.light_active ? 'text-warning' : 'text-base-content/30'}`}>
                    {status()!.monitored.light_brightness}%
                  </div>
                  <div class="stat-desc">
                    <progress
                      class="progress progress-warning w-full h-1"
                      value={status()!.monitored.light_brightness}
                      max="100"
                    />
                  </div>
                </div>

                {/* Motor Direction */}
                <div class="stat p-3 bg-base-100 rounded-lg">
                  <div class="stat-title text-xs">Door Motor</div>
                  <div class={`stat-value text-lg ${motorDirectionColor(status()!.monitored.motor_direction)}`}>
                    {status()!.monitored.motor_direction}
                  </div>
                  <div class="stat-desc text-xs">
                    +:{status()!.monitored.motor_pos_active ? 'H' : 'L'} | -{status()!.monitored.motor_neg_active ? 'H' : 'L'}
                  </div>
                </div>

                {/* Buzzer */}
                <div class="stat p-3 bg-base-100 rounded-lg">
                  <div class="stat-title text-xs">Buzzer</div>
                  <div class={`stat-value text-lg ${status()!.monitored.buzzer_active ? 'text-error animate-pulse' : 'text-base-content/30'}`}>
                    {status()!.monitored.buzzer_active ? 'SOUNDING' : 'SILENT'}
                  </div>
                  <Show when={status()!.monitored.buzzer_active}>
                    <div class="stat-desc text-xs">
                      {(status()!.monitored.buzzer_duration_ms / 1000).toFixed(1)}s
                    </div>
                  </Show>
                </div>

                {/* WiFi LED */}
                <div class="stat p-3 bg-base-100 rounded-lg">
                  <div class="stat-title text-xs">WiFi LED</div>
                  <div class={`stat-value text-lg ${status()!.monitored.wifi_led_active ? 'text-info' : 'text-base-content/30'}`}>
                    {status()!.monitored.wifi_led_active ? 'ON' : 'OFF'}
                  </div>
                </div>
              </div>
            </div>
          </div>

          {/* Emulated Outputs Section */}
          <div class="card bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title text-info">Emulated Outputs</h2>
              <p class="text-sm text-base-content/60">Signals being sent to main Coop Controller</p>

              <div class="grid grid-cols-2 md:grid-cols-3 gap-4 mt-4">
                {/* Door Position */}
                <div class="stat p-3 bg-base-100 rounded-lg col-span-2 md:col-span-1">
                  <div class="stat-title text-xs">Door Position</div>
                  <div class={`stat-value text-lg ${doorStateColor(status()!.emulated.door_state)}`}>
                    {status()!.emulated.door_position}%
                  </div>
                  <div class="stat-desc">
                    <progress
                      class="progress progress-info w-full h-2"
                      value={status()!.emulated.door_position}
                      max="100"
                    />
                  </div>
                  <div class="stat-desc text-xs mt-1">
                    State: {status()!.emulated.door_state}
                  </div>
                </div>

                {/* Hall Sensors */}
                <div class="stat p-3 bg-base-100 rounded-lg">
                  <div class="stat-title text-xs">Hall Sensors</div>
                  <div class="stat-value text-sm space-y-1">
                    <div class={status()!.emulated.hall_open_active ? 'text-success' : 'text-base-content/30'}>
                      Open: {status()!.emulated.hall_open_active ? 'ACTIVE' : 'OFF'}
                    </div>
                    <div class={status()!.emulated.hall_close_active ? 'text-info' : 'text-base-content/30'}>
                      Close: {status()!.emulated.hall_close_active ? 'ACTIVE' : 'OFF'}
                    </div>
                  </div>
                </div>

                {/* Water Pulses */}
                <div class="stat p-3 bg-base-100 rounded-lg">
                  <div class="stat-title text-xs">Water Pulses</div>
                  <div class="stat-value text-sm">
                    <div>Ch1: {status()!.emulated.channel1_pulses.toLocaleString()}</div>
                    <div class="text-base-content/50">Ch2: {status()!.emulated.channel2_pulses.toLocaleString()}</div>
                  </div>
                  <div class="stat-desc text-xs">
                    {status()!.emulated.flow_rate_gpm.toFixed(1)} GPM
                  </div>
                </div>

                {/* Manual Switch */}
                <div class="stat p-3 bg-base-100 rounded-lg">
                  <div class="stat-title text-xs">Manual Switch</div>
                  <div class={`stat-value text-lg ${status()!.emulated.manual_switch_pressed ? 'text-warning' : 'text-base-content/30'}`}>
                    {status()!.emulated.manual_switch_pressed ? 'PRESSED' : 'RELEASED'}
                  </div>
                </div>

                {/* Door Fault */}
                <div class="stat p-3 bg-base-100 rounded-lg">
                  <div class="stat-title text-xs">Door Fault</div>
                  <div class={`stat-value text-lg ${status()!.emulated.door_fault_active ? 'text-error' : 'text-success'}`}>
                    {status()!.emulated.door_fault_active ? 'FAULT' : 'OK'}
                  </div>
                </div>
              </div>
            </div>
          </div>

          {/* Quick Actions */}
          <div class="card bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title">Quick Actions</h2>

              <div class="flex flex-wrap gap-2">
                <button
                  class="btn btn-sm btn-success"
                  onClick={() => fetch('/emulator/door/open', { method: 'POST' })}
                >
                  Door Open
                </button>
                <button
                  class="btn btn-sm btn-info"
                  onClick={() => fetch('/emulator/door/close', { method: 'POST' })}
                >
                  Door Close
                </button>
                <button
                  class="btn btn-sm btn-warning"
                  onClick={() => fetch('/emulator/manual_switch/pulse', { method: 'POST' })}
                >
                  Pulse Switch
                </button>
                <button
                  class="btn btn-sm btn-accent"
                  onClick={() => {
                    const formData = new FormData()
                    formData.append('channel', '1')
                    fetch('/emulator/water/pulse', { method: 'POST', body: formData })
                  }}
                >
                  Water Pulse
                </button>
                <button
                  class="btn btn-sm btn-error"
                  onClick={() => fetch('/emulator/door/fault', { method: 'POST' })}
                >
                  Inject Fault
                </button>
                <button
                  class="btn btn-sm btn-outline"
                  onClick={() => fetch('/emulator/fault/clear_all', { method: 'POST' })}
                >
                  Clear Faults
                </button>
              </div>
            </div>
          </div>

          {/* System Status */}
          <Show when={systemStatus()}>
            <div class="card bg-base-200 card-sm shadow-sm">
              <div class="card-body">
                <h2 class="card-title">System Status</h2>

                <div class="grid grid-cols-2 md:grid-cols-4 gap-4">
                  <div class="stat p-3 bg-base-100 rounded-lg">
                    <div class="stat-title text-xs">Uptime</div>
                    <div class="stat-value text-sm">{systemStatus()!.uptime_formatted}</div>
                  </div>

                  <div class="stat p-3 bg-base-100 rounded-lg">
                    <div class="stat-title text-xs">Memory</div>
                    <div class="stat-value text-sm">{systemStatus()!.heap_used_percent.toFixed(1)}%</div>
                    <div class="stat-desc text-xs">
                      {(systemStatus()!.heap_free / 1024).toFixed(0)}KB free
                    </div>
                  </div>

                  <div class="stat p-3 bg-base-100 rounded-lg">
                    <div class="stat-title text-xs">WiFi</div>
                    <div class="stat-value text-sm">
                      {systemStatus()!.wifi_rssi ? `${systemStatus()!.wifi_rssi} dBm` : 'AP Mode'}
                    </div>
                    <div class="stat-desc text-xs">{systemStatus()!.wifi_ip}</div>
                  </div>

                  <div class="stat p-3 bg-base-100 rounded-lg">
                    <div class="stat-title text-xs">Firmware</div>
                    <div class="stat-value text-sm">{systemStatus()!.firmware_version}</div>
                    <div class="stat-desc text-xs">{systemStatus()!.chip_model}</div>
                  </div>
                </div>
              </div>
            </div>
          </Show>
        </div>
      </Show>
    </div>
  )
}

export default Status
