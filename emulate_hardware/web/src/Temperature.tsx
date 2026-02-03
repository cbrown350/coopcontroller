import { createSignal, onMount, Show } from 'solid-js'
import type { TempSensorState, TempSensorConfig } from './types'

function SensorCard(props: {
  label: string
  sensor: TempSensorConfig
  onUpdate: (updated: TempSensorConfig) => void
}) {
  const update = <K extends keyof TempSensorConfig>(key: K, value: TempSensorConfig[K]) => {
    props.onUpdate({ ...props.sensor, [key]: value })
  }

  const tempF = () => (props.sensor.temperature_c * 9 / 5 + 32).toFixed(1)

  return (
    <div class={`card shadow-sm ${props.sensor.enabled ? 'bg-base-200' : 'bg-base-200 opacity-60'}`}>
      <div class="card-body">
        <div class="flex items-center justify-between">
          <h3 class="card-title">{props.label}</h3>
          <div class="flex items-center gap-2">
            <Show when={props.sensor.disconnected}>
              <span class="badge badge-error badge-sm">Disconnected</span>
            </Show>
            <Show when={!props.sensor.disconnected && props.sensor.enabled}>
              <span class="badge badge-success badge-sm">Active</span>
            </Show>
            <input
              type="checkbox"
              class="toggle toggle-primary toggle-sm"
              checked={props.sensor.enabled}
              onChange={(e) => update('enabled', e.currentTarget.checked)}
            />
          </div>
        </div>

        <Show when={props.sensor.enabled}>
          {/* Temperature display */}
          <div class="text-center my-3">
            <span class="text-3xl font-bold text-primary">
              {props.sensor.temperature_c.toFixed(1)}°C
            </span>
            <span class="text-base text-base-content/60 ml-2">
              ({tempF()}°F)
            </span>
          </div>

          {/* Temperature slider */}
          <div class="form-control">
            <label class="label">
              <span class="label-text">Temperature (°C)</span>
              <span class="label-text-alt">{props.sensor.temperature_c.toFixed(1)}°C</span>
            </label>
            <input
              type="range"
              class="range range-primary"
              min="-40"
              max="60"
              step="0.5"
              value={props.sensor.temperature_c}
              onInput={(e) => update('temperature_c', parseFloat(e.currentTarget.value))}
            />
            <div class="flex justify-between text-xs text-base-content/50">
              <span>-40°C</span>
              <span>0°C</span>
              <span>60°C</span>
            </div>
          </div>

          {/* Numeric input for precise temperature */}
          <div class="form-control mt-2">
            <label class="label">
              <span class="label-text">Precise Temperature (°C)</span>
            </label>
            <input
              type="number"
              class="input input-bordered input-sm"
              min="-40"
              max="60"
              step="0.1"
              value={props.sensor.temperature_c}
              onInput={(e) => {
                const v = parseFloat(e.currentTarget.value)
                if (!isNaN(v) && v >= -40 && v <= 60) update('temperature_c', v)
              }}
            />
          </div>

          {/* Disconnect simulation */}
          <div class="form-control mt-3">
            <label class="label cursor-pointer">
              <span class="label-text">Simulate Disconnected</span>
              <input
                type="checkbox"
                class="toggle toggle-error toggle-sm"
                checked={props.sensor.disconnected}
                onChange={(e) => update('disconnected', e.currentTarget.checked)}
              />
            </label>
          </div>

          {/* Drift settings */}
          <div class="collapse collapse-arrow bg-base-300 rounded-box mt-3">
            <input type="checkbox" class="peer" />
            <div class="collapse-title font-medium text-sm">
              Drift Simulation
            </div>
            <div class="collapse-content">
              <div class="form-control">
                <label class="label cursor-pointer">
                  <span class="label-text text-sm">Enable Drift</span>
                  <input
                    type="checkbox"
                    class="toggle toggle-accent toggle-sm"
                    checked={props.sensor.drift_enabled}
                    onChange={(e) => update('drift_enabled', e.currentTarget.checked)}
                  />
                </label>
              </div>

              <Show when={props.sensor.drift_enabled}>
                <div class="form-control">
                  <label class="label">
                    <span class="label-text text-sm">Amplitude (°C): {props.sensor.drift_amplitude_c.toFixed(1)}</span>
                  </label>
                  <input
                    type="range"
                    class="range range-accent range-sm"
                    min="0.1"
                    max="10"
                    step="0.1"
                    value={props.sensor.drift_amplitude_c}
                    onInput={(e) => update('drift_amplitude_c', parseFloat(e.currentTarget.value))}
                  />
                  <div class="flex justify-between text-xs text-base-content/50">
                    <span>0.1°C</span>
                    <span>10°C</span>
                  </div>
                </div>

                <div class="form-control mt-2">
                  <label class="label">
                    <span class="label-text text-sm">Period (seconds): {(props.sensor.drift_period_ms / 1000).toFixed(0)}</span>
                  </label>
                  <input
                    type="range"
                    class="range range-accent range-sm"
                    min="5000"
                    max="300000"
                    step="5000"
                    value={props.sensor.drift_period_ms}
                    onInput={(e) => update('drift_period_ms', parseInt(e.currentTarget.value))}
                  />
                  <div class="flex justify-between text-xs text-base-content/50">
                    <span>5s</span>
                    <span>5 min</span>
                  </div>
                </div>
              </Show>
            </div>
          </div>
        </Show>
      </div>
    </div>
  )
}

function Temperature() {
  const defaultSensor = (): TempSensorConfig => ({
    enabled: false,
    temperature_c: 22.0,
    disconnected: false,
    drift_enabled: false,
    drift_amplitude_c: 1.0,
    drift_period_ms: 60000
  })

  const [sensors, setSensors] = createSignal<TempSensorState>({
    sensor1: defaultSensor(),
    sensor2: defaultSensor()
  })
  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal('')
  const [success, setSuccess] = createSignal('')

  const fetchTemperature = async () => {
    try {
      const res = await fetch('/emulator/temperature')
      if (!res.ok) throw new Error(`Failed to load temperature data: ${res.status}`)
      const data: TempSensorState = await res.json()
      setSensors(data)
      setError('')
    } catch (err: any) {
      setError(err.message)
    } finally {
      setLoading(false)
    }
  }

  onMount(() => {
    fetchTemperature()
  })

  const showSuccess = (msg: string) => {
    setSuccess(msg)
    setTimeout(() => setSuccess(''), 3000)
  }

  const saveSensor = async (key: 'sensor1' | 'sensor2', config: TempSensorConfig) => {
    try {
      setError('')
      const res = await fetch(`/emulator/temperature/set`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ [key]: config })
      })
      const data = await res.json()
      if (!res.ok) throw new Error(data.error || 'Failed to update sensor')
      showSuccess(`${key === 'sensor1' ? 'Sensor 1' : 'Sensor 2'} updated`)
      await fetchTemperature()
    } catch (err: any) {
      setError(err.message)
    }
  }

  const handleSensorUpdate = (key: 'sensor1' | 'sensor2', updated: TempSensorConfig) => {
    setSensors(prev => ({ ...prev, [key]: updated }))
    saveSensor(key, updated)
  }

  return (
    <div class="space-y-4">
      <h2 class="text-lg font-bold">Temperature Sensor Emulation</h2>

      <p class="text-sm text-base-content/60">
        Emulate DS18B20 temperature sensors with configurable values and drift simulation.
        Changes are applied immediately via REST API.
      </p>

      <Show when={error()}>
        <div role="alert" class="alert alert-error">{error()}</div>
      </Show>
      <Show when={success()}>
        <div role="alert" class="alert alert-success">{success()}</div>
      </Show>

      <Show when={loading()}>
        <p class="flex items-center gap-2">
          Loading...
          <span class="loading loading-spinner loading-md"></span>
        </p>
      </Show>

      <Show when={!loading()}>
        <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
          <SensorCard
            label="Sensor 1"
            sensor={sensors().sensor1}
            onUpdate={(updated) => handleSensorUpdate('sensor1', updated)}
          />
          <SensorCard
            label="Sensor 2"
            sensor={sensors().sensor2}
            onUpdate={(updated) => handleSensorUpdate('sensor2', updated)}
          />
        </div>

        {/* Refresh */}
        <div class="flex gap-2 mt-2">
          <button class="btn btn-outline btn-sm" onClick={fetchTemperature}>
            Refresh
          </button>
        </div>
      </Show>
    </div>
  )
}

export default Temperature
