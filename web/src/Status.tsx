import { createSignal, onMount, onCleanup, Show } from 'solid-js'

function Status() {

  const [loading, setLoading] = createSignal(true)
  const [sensorStatus, setSensorStatus] = createSignal({
    sensor1: {
      type: 0,
      connected: false,
      temperature_f: 0,
      flow_rate: 0,
      pulse_count: 0,
      status: 'Not Connected'
    },
    sensor2: {
      type: 0,
      connected: false,
      temperature_f: 0,
      flow_rate: 0,
      pulse_count: 0,
      status: 'Not Connected'
    },
    pump: {
      state: 'OFF',
      is_active: false,
      temperature_f: 0,
      temperature_below_threshold: false,
      flow_error: false,
      current_cycle_time: 0,
      time_until_next_switch: 0,
      time_until_retry: 0,
      total_on_time: 0,
      total_off_time: 0,
      total_cycles: 0
    },
    system: {
      temp_threshold_on_f: 34,
      temp_threshold_off_f: 36,
      pump_on_time_seconds: 300,
      pump_off_time_seconds: 600,
      pump_auto_mode: true,
      light_auto_mode: false,
      light_on_hour: 6,
      light_off_hour: 20
    }
  })
  const [error, setError] = createSignal('')

  const refreshSensorStatus = async () => {
    try {
      const response = await fetch('/sensor_status')
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status} ${response.statusText}`)
      }
      const data = await response.json()
      setSensorStatus(data)
      setLoading(false)
    } catch (err: any) {
      console.error('Failed to fetch sensor status:', err)
      setError(`Error loading sensor status: ${err.message || 'Unknown error'}`)
      setLoading(false)
    }
  }

  const handlePumpControl = async (action: string) => {
    try {
      const response = await fetch(`/pump/${action}`, { method: 'GET' })
      if (response.ok) {
        await refreshSensorStatus() // Refresh status after action
      }
    } catch (error) {
      console.error('Pump control error:', error)
    }
  }

  const handleWaterReset = async (sensor: number) => {
    try {
      const response = await fetch(`/water/reset/${sensor}`, { method: 'GET' })
      if (response.ok) {
        await refreshSensorStatus() // Refresh status after action
      }
    } catch (error) {
      console.error('Water reset error:', error)
    }
  }

  const formatTime = (seconds: number) => {
    const hours = Math.floor(seconds / 3600)
    const minutes = Math.floor((seconds % 3600) / 60)
    const secs = seconds % 60
    return `${hours}h ${minutes}m ${secs}s`
  }

  onMount(async () => {
    setLoading(true)
    await refreshSensorStatus()
    const intervalId = setInterval(refreshSensorStatus, 2500)

    onCleanup(() => {
      clearInterval(intervalId)
    })
  })

  return (
    <div>
      {loading() ? (
        <p>Loading status... <span class="loading loading-spinner loading-xl"></span></p>
      ) : (
        <div>
          {/* Temperature Sensors Section */}
          <div class="card w-full mt-4 bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title">Temperature & Water Sensors</h2>
              <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                {/* Sensor 1 */}
                <div class="stat">
                  <div class="stat-title">Sensor 1</div>
                  <div class="stat-value text-sm">
                    {sensorStatus().sensor1.type === 1 ? 'Dallas Temperature' : 
                     sensorStatus().sensor1.type === 2 ? 'Water Meter' : 'None'}
                  </div>
                  <div class="stat-desc text-xs">
                    Status: {sensorStatus().sensor1.status && sensorStatus().sensor1.status !=="Not Connected" ? "Connected" : "Not Connected"}
                  </div>
                  <Show when={sensorStatus().sensor1.type === 1}>
                    <div class="stat-desc text-xs">
                      Temperature: {sensorStatus().sensor1.temperature_f.toFixed(1)}°F
                    </div>
                  </Show>
                  <Show when={sensorStatus().sensor1.type === 2}>
                    <div class="stat-desc text-xs">
                      Flow: {sensorStatus().sensor1.flow_rate.toFixed(2)} GPM
                    </div>
                    <div class="stat-desc text-xs">
                      Pulses: {sensorStatus().sensor1.pulse_count}
                    </div>
                    <button 
                      class="btn btn-xs btn-outline mt-2"
                      onClick={() => handleWaterReset(1)}
                    >
                      Reset Counter
                    </button>
                  </Show>
                </div>

                {/* Sensor 2 */}
                <div class="stat">
                  <div class="stat-title">Sensor 2</div>
                  <div class="stat-value text-sm">
                    {sensorStatus().sensor2.type === 1 ? 'Dallas Temperature' : 
                     sensorStatus().sensor2.type === 2 ? 'Water Meter' : 'None'}
                  </div>
                  <div class="stat-desc text-xs">
                    Status: {sensorStatus().sensor2.status && sensorStatus().sensor2.status !=="Not Connected" ? "Connected" : "Not Connected"}
                  </div>
                  <Show when={sensorStatus().sensor2.type === 1}>
                    <div class="stat-desc text-xs">
                      Temperature: {sensorStatus().sensor2.temperature_f.toFixed(1)}°F
                    </div>
                  </Show>
                  <Show when={sensorStatus().sensor2.type === 2}>
                    <div class="stat-desc text-xs">
                      Flow: {sensorStatus().sensor2.flow_rate.toFixed(2)} GPM
                    </div>
                    <div class="stat-desc text-xs">
                      Pulses: {sensorStatus().sensor2.pulse_count}
                    </div>
                    <button 
                      class="btn btn-xs btn-outline mt-2"
                      onClick={() => handleWaterReset(2)}
                    >
                      Reset Counter
                    </button>
                  </Show>
                </div>
              </div>
            </div>
          </div>

          {/* Pump Control Section */}
          <div class="card w-full mt-4 bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title">Pump Control</h2>
              <div class="stats w-full shadow bg-base-300">
                <div class="stat">
                  <div class="stat-title">Pump State</div>
                  <div class={`stat-value text-lg ${sensorStatus().pump.flow_error ? 'text-error' : sensorStatus().pump.is_active ? 'text-success' : 'text-warning'}`}>
                    {sensorStatus().pump.state === 'AUTO' ? 'AUTO' : sensorStatus().pump.state === 'ON' ? 'ON' : 'OFF'}
                  </div>
                  <div class="stat-desc">
                    {sensorStatus().pump.state === 'AUTO' ? 'Automatic temperature control' : 
                     sensorStatus().pump.state === 'ON' ? 'Manual ON' : 
                     sensorStatus().pump.state === 'ERROR' ? 'Flow Error - Pump Off' : 'Manual OFF'}
                  </div>
                  {error() && (
                    <div class="stat-desc text-error">Error: {error()}</div>
                  )}
                  <Show when={sensorStatus().pump.flow_error}>
                    <div class="stat-desc text-error">Flow Error Detected!</div>
                    <Show when={sensorStatus().pump.time_until_retry > 0}>
                      <div class="stat-desc text-error">Retry in: {formatTime(sensorStatus().pump.time_until_retry)}</div>
                    </Show>
                  </Show>
                </div>
                
                <div class="stat">
                  <div class="stat-title">Current Temperature</div>
                  <div class="stat-value text-lg">
                    {sensorStatus().pump.temperature_f.toFixed(1)}°F
                  </div>
                  <div class="stat-desc">
                    ON Threshold: {sensorStatus().system.temp_threshold_on_f.toFixed(1)}°F
                  </div>
                  <div class="stat-desc">
                    OFF Threshold: {sensorStatus().system.temp_threshold_off_f.toFixed(1)}°F
                  </div>
                </div>

                <div class="stat">
                  <div class="stat-title">Cycle Time</div>
                  <div class="stat-value text-lg">
                    {formatTime(sensorStatus().pump.current_cycle_time)}
                  </div>
                  <Show when={sensorStatus().pump.state === 'AUTO' && sensorStatus().pump.time_until_next_switch > 0}>
                    <div class="stat-desc">
                      Next switch: {formatTime(sensorStatus().pump.time_until_next_switch)}
                    </div>
                  </Show>
                </div>
              </div>

              {/* Pump Control Buttons */}
              <div class="flex gap-2 mt-4">
                <button 
                  class="btn btn-success btn-sm"
                  onClick={() => handlePumpControl('on')}
                  disabled={sensorStatus().pump.state === 'ON'}
                >
                  Turn On
                </button>
                <button 
                  class="btn btn-error btn-sm"
                  onClick={() => handlePumpControl('off')}
                  disabled={sensorStatus().pump.state === 'OFF'}
                >
                  Turn Off
                </button>
                <button 
                  class="btn btn-primary btn-sm"
                  onClick={() => handlePumpControl('auto')}
                  disabled={sensorStatus().pump.state === 'AUTO'}
                >
                  Auto Mode
                </button>
                <button 
                  class="btn btn-outline btn-sm"
                  onClick={() => handlePumpControl('force_cycle')}
                  disabled={sensorStatus().pump.state !== 'AUTO'}
                >
                  Force Cycle
                </button>
                <Show when={sensorStatus().pump.flow_error}>
                  <button 
                    class="btn btn-warning btn-sm"
                    onClick={() => handlePumpControl('clear_error')}
                  >
                    Clear Error
                  </button>
                </Show>
              </div>
            </div>
          </div>

          {/* System Information Section */}
          <div class="card w-full mt-4 bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title">System Information</h2>
              <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div class="stat">
                  <div class="stat-title">Pump Configuration</div>
                  <div class="stat-desc text-sm">
                    Current Mode: <span class={sensorStatus().pump.state === 'AUTO' ? 'text-success' : 
                                                     sensorStatus().pump.state === 'ON' ? 'text-success' : 
                                                     sensorStatus().pump.state === 'ERROR' ? 'text-error' : 'text-warning'}>
                      {sensorStatus().pump.state === 'AUTO' ? 'Auto Mode' : 
                       sensorStatus().pump.state === 'ON' ? 'Manual ON' : 
                       sensorStatus().pump.state === 'ERROR' ? 'Flow Error' : 'Manual OFF'}
                    </span>
                  </div>
                  <div class="stat-desc text-sm">
                    Auto Mode Setting: <span class={sensorStatus().system.pump_auto_mode ? 'text-success' : 'text-error'}>
                      {sensorStatus().system.pump_auto_mode ? 'Enabled' : 'Disabled'}
                    </span>
                  </div>
                  <div class="stat-desc text-sm">
                    On Time: {formatTime(sensorStatus().system.pump_on_time_seconds)}
                  </div>
                  <div class="stat-desc text-sm">
                    Off Time: {formatTime(sensorStatus().system.pump_off_time_seconds)}
                  </div>
                </div>

                <div class="stat">
                  <div class="stat-title">Pump Statistics</div>
                  <div class="stat-desc text-sm">
                    Total Cycles: {sensorStatus().pump.total_cycles}
                  </div>
                  <div class="stat-desc text-sm">
                    Total On Time: {formatTime(sensorStatus().pump.total_on_time)}
                  </div>
                  <div class="stat-desc text-sm">
                    Total Off Time: {formatTime(sensorStatus().pump.total_off_time)}
                  </div>
                  <button 
                    class="btn btn-xs btn-outline mt-2"
                    onClick={() => handlePumpControl('reset_stats')}
                  >
                    Reset Statistics
                  </button>
                </div>
              </div>
            </div>
          </div>

        </div>
      )}
    </div>
  )
}

export default Status