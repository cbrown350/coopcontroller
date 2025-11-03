import { createSignal, onMount, onCleanup, Show } from 'solid-js'

function Status() {

  const [loading, setLoading] = createSignal(true)
  const [sensorStatus, setSensorStatus] = createSignal({
    sensor1: {
      type: "UNKNOWN",
      connected: false,
      temperature_f: null as number | null,
      flow_rate: 0,
      pulse_count: 0,
      last_pulse_time: 0,
      actively_connected: false,
      status: 'Not Connected'
    },
    sensor2: {
      type: "UNKNOWN",
      connected: false,
      temperature_f: null as number | null,
      flow_rate: 0,
      pulse_count: 0,
      last_pulse_time: 0,
      actively_connected: false,
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
      light_auto_mode: false,
      light_on_hour: 6,
      light_off_hour: 20
    }
  })
  const [error, setError] = createSignal('')

  // Helpers accept both new string enums and legacy numeric values for backward compatibility.
  const isDallasType = (t: any) => t === 1 || t === 'DALLAS_TEMP'
  const isWaterType = (t: any) => t === 2 || t === 'WATER_METER'
  const sensorTypeLabel = (t: any) => {
    if (isDallasType(t)) return 'Dallas Temperature'
    if (isWaterType(t)) return 'Water Meter'
    if (t === 'UNKNOWN' || t === 0) return 'None'
    return String(t)
  }

  const displayTemp = (temp: number | null | undefined) => {
    if (temp === null || temp === undefined || isNaN(temp)) {
      return "---°F"
    }
    return `${temp.toFixed(1)}°F`
  }

  const refreshSensorStatus = async () => {
    try {
      const response = await fetch('/sensor_status')
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status} ${response.statusText}`)
      }
      const data = await response.json()
      
      // Add debug logging to see what's happening
      console.log('Pump status data received:', data)
      console.log('Pump flow_error:', data.pump.flow_error)
      console.log('Pump state:', data.pump.state)
      
      setSensorStatus(data)
      setLoading(false)
      // Clear any existing error when data is successfully fetched
      setError('')
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
                    {sensorTypeLabel(sensorStatus().sensor1.type)}
                  </div>
                  <div class="stat-desc text-xs">
                    Status: {sensorStatus().sensor1.status}
                  </div>
                  <Show when={isWaterType(sensorStatus().sensor1.type)}>
                    <div class="stat-desc text-xs">
                      Time since last pulse: {sensorStatus().sensor1.last_pulse_time}s
                    </div>
                  </Show>
                  <div class="stat-desc text-xs">
                      Actively Connected: {sensorStatus().sensor1.actively_connected ? "Yes" : "No"}
                    </div>
                  <Show when={isDallasType(sensorStatus().sensor1.type)}>
                     <div class="stat-desc text-xs">
                       Temperature: {displayTemp(sensorStatus().sensor1.temperature_f)}
                     </div>
                   </Show>
                  <Show when={isWaterType(sensorStatus().sensor1.type)}>
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
                    {sensorTypeLabel(sensorStatus().sensor2.type)}
                  </div>
                  <div class="stat-desc text-xs">
                    Status: {sensorStatus().sensor2.status}
                  </div>
                  <Show when={isWaterType(sensorStatus().sensor2.type)}>
                    <div class="stat-desc text-xs">
                      Time since last pulse: {sensorStatus().sensor2.last_pulse_time}s
                    </div>
                  </Show>
                  <div class="stat-desc text-xs">
                      Actively Connected: {sensorStatus().sensor2.actively_connected ? "Yes" : "No"}
                    </div>
                  <Show when={isDallasType(sensorStatus().sensor2.type)}>
                     <div class="stat-desc text-xs">
                       Temperature: {displayTemp(sensorStatus().sensor2.temperature_f)}
                     </div>
                   </Show>
                  <Show when={isWaterType(sensorStatus().sensor2.type)}>
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
                    {displayTemp(sensorStatus().pump.temperature_f)}
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
                  disabled={sensorStatus().pump.state !== 'AUTO' || !sensorStatus().pump.temperature_below_threshold}
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