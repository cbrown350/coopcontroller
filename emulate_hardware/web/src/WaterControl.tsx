import { createSignal, onMount, onCleanup, Show } from 'solid-js'
import { EmulatorStatus } from './types'

function WaterControl() {
  const [status, setStatus] = createSignal<EmulatorStatus | null>(null)
  const [loading, setLoading] = createSignal(true)
  const [flowRate, setFlowRateLocal] = createSignal(2.5)

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

  const triggerPulse = async (channel: number) => {
    const formData = new FormData()
    formData.append('channel', channel.toString())
    await fetch('/emulator/water/pulse', { method: 'POST', body: formData })
  }

  const resetCounters = async () => {
    await fetch('/emulator/water/reset', { method: 'POST' })
  }

  const updateWaterConfig = async (key: string, value: string) => {
    const formData = new FormData()
    formData.append(key, value)
    await fetch('/emulator/water/config', { method: 'POST', body: formData })
  }

  const toggleFrozenLine = async (frozen: boolean) => {
    const formData = new FormData()
    formData.append('frozen', frozen.toString())
    await fetch('/emulator/water/frozen', { method: 'POST', body: formData })
  }

  return (
    <div class="space-y-4">
      <h2 class="text-lg font-bold">Water Meter Emulation</h2>

      <Show when={loading()}>
        <p class="flex items-center gap-2">
          Loading...
          <span class="loading loading-spinner loading-md"></span>
        </p>
      </Show>

      <Show when={status()}>
        {/* Current Status */}
        <div class="card bg-base-200 card-sm shadow-sm">
          <div class="card-body">
            <h3 class="card-title">Water Flow Status</h3>

            <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
              <div class="stat p-3 bg-base-100 rounded-lg">
                <div class="stat-title">Pump State (From Main)</div>
                <div class={`stat-value text-xl ${status()!.monitored.pump_active ? 'text-success' : 'text-base-content/30'}`}>
                  {status()!.monitored.pump_active ? 'RUNNING' : 'OFF'}
                </div>
                <div class="stat-desc">
                  <span class={`inline-block w-2 h-2 rounded-full mr-1 ${status()!.monitored.pump_active ? 'bg-success animate-pulse' : 'bg-base-300'}`}></span>
                  Reading from GPIO34
                </div>
              </div>

              <div class="stat p-3 bg-base-100 rounded-lg">
                <div class="stat-title">Flow Rate</div>
                <div class="stat-value text-xl text-info">
                  {status()!.emulated.flow_rate_gpm.toFixed(2)} GPM
                </div>
                <div class="stat-desc">Configured rate when pump active</div>
              </div>

              <div class="stat p-3 bg-base-100 rounded-lg">
                <div class="stat-title">Auto Pulses</div>
                <div class={`stat-value text-lg ${status()!.config.auto_generate_pulses ? 'text-success' : 'text-warning'}`}>
                  {status()!.config.auto_generate_pulses ? 'ENABLED' : 'DISABLED'}
                </div>
              </div>
            </div>
          </div>
        </div>

        {/* Pulse Counters */}
        <div class="card bg-base-200 card-sm shadow-sm">
          <div class="card-body">
            <h3 class="card-title">Pulse Counters</h3>

            <div class="grid grid-cols-2 gap-4">
              <div class="stat p-3 bg-base-100 rounded-lg">
                <div class="stat-title">Channel 1 (Primary)</div>
                <div class="stat-value text-2xl text-info">
                  {status()!.emulated.channel1_pulses.toLocaleString()}
                </div>
                <div class="stat-desc">Pulses sent to TEMP_METER_PIN (32)</div>
                <button
                  class="btn btn-accent btn-sm mt-2"
                  onClick={() => triggerPulse(1)}
                >
                  Send Pulse
                </button>
              </div>

              <div class="stat p-3 bg-base-100 rounded-lg">
                <div class="stat-title">Channel 2 (Secondary)</div>
                <div class="stat-value text-2xl text-base-content/50">
                  {status()!.emulated.channel2_pulses.toLocaleString()}
                </div>
                <div class="stat-desc">Pulses sent to TEMP_METER_2_PIN (33)</div>
                <button
                  class="btn btn-accent btn-sm mt-2"
                  onClick={() => triggerPulse(2)}
                >
                  Send Pulse
                </button>
              </div>
            </div>

            <div class="mt-4">
              <button
                class="btn btn-outline btn-sm"
                onClick={resetCounters}
              >
                Reset All Counters
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
                <span class="label-text">Auto-generate pulses when pump is running</span>
                <input
                  type="checkbox"
                  class="toggle toggle-info"
                  checked={status()!.config.auto_generate_pulses}
                  onChange={(e) => updateWaterConfig('auto_generate', e.currentTarget.checked.toString())}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">When enabled, water pulses are generated based on flow rate when pump signal is HIGH</span>
              </label>
            </div>

            <div class="form-control mt-4">
              <label class="label">
                <span class="label-text">Flow Rate: {flowRate().toFixed(1)} GPM</span>
              </label>
              <input
                type="range"
                min="0.5"
                max="10"
                step="0.5"
                value={status()!.config.flow_rate_gpm}
                class="range range-info range-sm"
                onInput={(e) => setFlowRateLocal(parseFloat((e.target as HTMLInputElement).value))}
                onChange={(e) => updateWaterConfig('flow_rate_gpm', (e.target as HTMLInputElement).value)}
              />
              <div class="w-full flex justify-between text-xs px-2 mt-1">
                <span>0.5</span>
                <span>5</span>
                <span>10 GPM</span>
              </div>
            </div>

            <div class="form-control mt-4">
              <label class="label">
                <span class="label-text">Pulses Per Gallon: {status()!.config.pulses_per_gallon.toFixed(0)}</span>
              </label>
              <input
                type="range"
                min="100"
                max="1000"
                step="50"
                value={status()!.config.pulses_per_gallon}
                class="range range-sm"
                onChange={(e) => updateWaterConfig('pulses_per_gallon', (e.target as HTMLInputElement).value)}
              />
              <div class="w-full flex justify-between text-xs px-2 mt-1">
                <span>100</span>
                <span>450</span>
                <span>1000</span>
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
                <span class="label-text">Simulate frozen/blocked water line (no pulses despite pump running)</span>
                <input
                  type="checkbox"
                  class="toggle toggle-error"
                  checked={status()!.config.simulate_frozen_line}
                  onChange={(e) => toggleFrozenLine(e.currentTarget.checked)}
                />
              </label>
              <label class="label">
                <span class="label-text-alt">When enabled, no water pulses will be generated even when pump is ON. This will trigger a flow error on the main controller.</span>
              </label>
            </div>
          </div>
        </div>

        {/* Manual Pulse Generation */}
        <div class="card bg-base-200 card-sm shadow-sm">
          <div class="card-body">
            <h3 class="card-title">Manual Pulse Generation</h3>

            <div class="flex flex-wrap gap-2">
              <button
                class="btn btn-info btn-sm"
                onClick={() => triggerPulse(1)}
              >
                1 Pulse (Ch1)
              </button>
              <button
                class="btn btn-info btn-sm"
                onClick={() => {
                  for (let i = 0; i < 10; i++) {
                    setTimeout(() => triggerPulse(1), i * 100)
                  }
                }}
              >
                10 Pulses (Ch1)
              </button>
              <button
                class="btn btn-info btn-sm"
                onClick={() => {
                  for (let i = 0; i < 100; i++) {
                    setTimeout(() => triggerPulse(1), i * 50)
                  }
                }}
              >
                100 Pulses (Ch1)
              </button>
            </div>
          </div>
        </div>
      </Show>
    </div>
  )
}

export default WaterControl
