import { createSignal, onMount, onCleanup, Show } from 'solid-js'
import { EmulatorStatus } from './types'

function DoorControl() {
  const [status, setStatus] = createSignal<EmulatorStatus | null>(null)
  const [loading, setLoading] = createSignal(true)
  const [positionOverride, setPositionOverride] = createSignal(50)

  const refreshStatus = async () => {
    try {
      const response = await fetch('/emulator/status')
      if (response.ok) {
        const data = await response.json()
        setStatus(data)
        setLoading(false)
      }
    } catch (err) {
      console.error('Failed to fetch status:', err)
    }
  }

  onMount(() => {
    refreshStatus()
    const intervalId = setInterval(refreshStatus, 500)
    onCleanup(() => clearInterval(intervalId))
  })

  const setPosition = async (position: number) => {
    const formData = new FormData()
    formData.append('position', position.toString())
    await fetch('/emulator/door/position', { method: 'POST', body: formData })
  }

  const setTravelTime = async (timeMs: number) => {
    const formData = new FormData()
    formData.append('travel_time_ms', timeMs.toString())
    await fetch('/emulator/door/config', { method: 'POST', body: formData })
  }

  const toggleAutoSimulate = async (enabled: boolean) => {
    const formData = new FormData()
    formData.append('auto_simulate', enabled.toString())
    await fetch('/emulator/door/config', { method: 'POST', body: formData })
  }

  const toggleDoorStuck = async (stuck: boolean) => {
    const formData = new FormData()
    formData.append('stuck', stuck.toString())
    await fetch('/emulator/fault/door_stuck', { method: 'POST', body: formData })
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
    <div class="space-y-4">
      <h2 class="text-lg font-bold">Door Emulation Controls</h2>

      <Show when={loading()}>
        <p class="flex items-center gap-2">
          Loading...
          <span class="loading loading-spinner loading-md"></span>
        </p>
      </Show>

      <Show when={status()}>
        {/* Current State */}
        <div class="card bg-base-200 card-sm shadow-sm">
          <div class="card-body">
            <h3 class="card-title">Current Door State</h3>

            <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
              <div class="stat p-3 bg-base-100 rounded-lg">
                <div class="stat-title">State</div>
                <div class={`stat-value text-xl ${doorStateColor(status()!.emulated.door_state)}`}>
                  {status()!.emulated.door_state}
                </div>
              </div>

              <div class="stat p-3 bg-base-100 rounded-lg col-span-2">
                <div class="stat-title">Position</div>
                <div class="stat-value text-xl">{status()!.emulated.door_position}%</div>
                <progress
                  class="progress progress-info w-full h-3 mt-2"
                  value={status()!.emulated.door_position}
                  max="100"
                />
              </div>
            </div>

            <div class="grid grid-cols-2 gap-4 mt-4">
              <div class="stat p-3 bg-base-100 rounded-lg">
                <div class="stat-title">Hall Sensor - OPEN</div>
                <div class={`stat-value text-lg ${status()!.emulated.hall_open_active ? 'text-success' : 'text-base-content/30'}`}>
                  {status()!.emulated.hall_open_active ? 'TRIGGERED' : 'Inactive'}
                </div>
              </div>

              <div class="stat p-3 bg-base-100 rounded-lg">
                <div class="stat-title">Hall Sensor - CLOSED</div>
                <div class={`stat-value text-lg ${status()!.emulated.hall_close_active ? 'text-info' : 'text-base-content/30'}`}>
                  {status()!.emulated.hall_close_active ? 'TRIGGERED' : 'Inactive'}
                </div>
              </div>
            </div>

            <div class="grid grid-cols-2 gap-4 mt-4">
              <div class="stat p-3 bg-base-100 rounded-lg">
                <div class="stat-title">Motor Signals (From Main)</div>
                <div class="stat-value text-lg">
                  {status()!.monitored.motor_direction}
                </div>
                <div class="stat-desc">
                  POS: {status()!.monitored.motor_pos_active ? 'HIGH' : 'LOW'} |
                  NEG: {status()!.monitored.motor_neg_active ? 'HIGH' : 'LOW'}
                </div>
              </div>

              <div class="stat p-3 bg-base-100 rounded-lg">
                <div class="stat-title">Door Fault Signal</div>
                <div class={`stat-value text-lg ${status()!.emulated.door_fault_active ? 'text-error' : 'text-success'}`}>
                  {status()!.emulated.door_fault_active ? 'FAULT ACTIVE' : 'OK'}
                </div>
              </div>
            </div>
          </div>
        </div>

        {/* Manual Position Override */}
        <div class="card bg-base-200 card-sm shadow-sm">
          <div class="card-body">
            <h3 class="card-title">Manual Position Override</h3>

            <div class="form-control">
              <label class="label">
                <span class="label-text">Set Door Position: {positionOverride()}%</span>
              </label>
              <input
                type="range"
                min="0"
                max="100"
                value={positionOverride()}
                class="range range-info"
                onInput={(e) => setPositionOverride(parseInt((e.target as HTMLInputElement).value))}
              />
              <div class="w-full flex justify-between text-xs px-2 mt-1">
                <span>CLOSED (0%)</span>
                <span>OPEN (100%)</span>
              </div>
            </div>

            <div class="flex gap-2 mt-4">
              <button
                class="btn btn-info btn-sm"
                onClick={() => setPosition(positionOverride())}
              >
                Set Position
              </button>
              <button
                class="btn btn-success btn-sm"
                onClick={() => { setPositionOverride(100); setPosition(100) }}
              >
                Set to OPEN
              </button>
              <button
                class="btn btn-warning btn-sm"
                onClick={() => { setPositionOverride(0); setPosition(0) }}
              >
                Set to CLOSED
              </button>
              <button
                class="btn btn-accent btn-sm"
                onClick={() => { setPositionOverride(50); setPosition(50) }}
              >
                Set to 50%
              </button>
            </div>
          </div>
        </div>

        {/* Configuration */}
        <div class="card bg-base-200 card-sm shadow-sm">
          <div class="card-body">
            <h3 class="card-title">Simulation Settings</h3>

            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Auto-simulate door movement</span>
                <input
                  type="checkbox"
                  class="toggle toggle-info"
                  checked={status()!.config.auto_simulate_door}
                  onChange={(e) => toggleAutoSimulate(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">When enabled, door position changes based on motor signals from main controller</span>
              </label>
            </div>

            <div class="form-control mt-4">
              <label class="label">
                <span class="label-text">Door Travel Time: {status()!.config.door_travel_time_ms / 1000}s</span>
              </label>
              <input
                type="range"
                min="1000"
                max="30000"
                step="1000"
                value={status()!.config.door_travel_time_ms}
                class="range range-sm"
                onChange={(e) => setTravelTime(parseInt((e.target as HTMLInputElement).value))}
              />
              <div class="w-full flex justify-between text-xs px-2 mt-1">
                <span>1s</span>
                <span>15s</span>
                <span>30s</span>
              </div>
            </div>
          </div>
        </div>

        {/* Fault Injection */}
        <div class="card bg-error/10 card-sm shadow-sm">
          <div class="card-body">
            <h3 class="card-title text-error">Fault Injection</h3>

            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Simulate door stuck (hall sensors never trigger)</span>
                <input
                  type="checkbox"
                  class="toggle toggle-error"
                  checked={status()!.config.simulate_door_stuck}
                  onChange={(e) => toggleDoorStuck(e.currentTarget.checked)}
                />
              </label>
            </div>

            <div class="flex gap-2 mt-4">
              <button
                class="btn btn-error btn-sm"
                onClick={() => fetch('/emulator/door/fault', { method: 'POST' })}
              >
                Inject Motor Fault
              </button>
              <button
                class="btn btn-outline btn-sm"
                onClick={() => fetch('/emulator/door/clear_fault', { method: 'POST' })}
              >
                Clear Motor Fault
              </button>
            </div>
          </div>
        </div>
      </Show>
    </div>
  )
}

export default DoorControl
