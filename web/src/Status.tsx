import { createSignal, onMount, onCleanup, Show } from 'solid-js'
import { SystemStatus } from './types'

function Status() {

  const [loading, setLoading] = createSignal(true)
  const [systemStatus, setSystemStatus] = createSignal<SystemStatus | null>(null)
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
      temperature_f: null as number | null,
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
    },
    buzzer: {
      enabled: true,
      buzzer_type: 'ACTIVE',
      has_active_alert: false,
      current_alert_type: undefined,
      silence_remaining_ms: 0
    },
    door: {
      state: 'IDLE',
      position: 'UNKNOWN',
      progress: 0,
      auto_mode: false,
      test_mode: false,
      hall_open: false,
      hall_closed: false,
      total_open_time: 0,
      total_close_time: 0,
      total_cycles: 0,
      next_scheduled_action: 'No scheduled action'
    },
    light: {
      state: 'OFF',
      current_brightness: 0,
      target_brightness: 0,
      max_brightness: 100,
      fade_progress: 0,
      auto_mode: false,
      test_mode: false,
      total_on_time: 0,
      total_cycles: 0,
      next_scheduled_action: 'No scheduled action'
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
      const [sensorResponse, systemResponse] = await Promise.all([
        fetch('/sensor_status'),
        fetch('/system_status')
      ])
      
      if (!sensorResponse.ok) {
        throw new Error(`HTTP error! status: ${sensorResponse.status} ${sensorResponse.statusText}`)
      }
      
      const sensorData = await sensorResponse.json()
      
      // Add debug logging to see what's happening
      console.log('Pump status data received:', sensorData)
      console.log('Pump flow_error:', sensorData.pump.flow_error)
      console.log('Pump state:', sensorData.pump.state)
      
      setSensorStatus(sensorData)
      
      if (systemResponse.ok) {
        const systemData = await systemResponse.json()
        setSystemStatus(systemData)
      }
      
      setLoading(false)
      // Clear any existing error when data is successfully fetched
      setError('')
    } catch (err: any) {
      console.error('Failed to fetch status:', err)
      setError(`Error loading status: ${err.message || 'Unknown error'}`)
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

  onMount(() => {
    setLoading(true)
    refreshSensorStatus().finally(() => setLoading(false))
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

          {/* Buzzer Control Section */}
          <Show when={sensorStatus().buzzer}>
            <div class="card w-full mt-4 bg-base-200 card-sm shadow-sm">
              <div class="card-body">
                <h2 class="card-title">Buzzer Alerts</h2>
                <div class="stats w-full shadow bg-base-300">
                  <div class="stat">
                    <div class="stat-title">Buzzer Status</div>
                    <div class={`stat-value text-lg ${sensorStatus().buzzer?.has_active_alert ? 'text-error' : 'text-success'}`}>
                      {sensorStatus().buzzer?.has_active_alert ? 'Active Alert' : 'No Alerts'}
                    </div>
                    <div class="stat-desc">
                      {sensorStatus().buzzer?.has_active_alert ? 
                        `Alert: ${sensorStatus().buzzer?.current_alert_type || 'Unknown'}` : 
                        'Buzzer is ready'}
                    </div>
                    <Show when={sensorStatus().buzzer?.silence_remaining_ms && sensorStatus().buzzer?.silence_remaining_ms! > 0}>
                      <div class="stat-desc">
                        Silenced for: {Math.floor((sensorStatus().buzzer?.silence_remaining_ms || 0) / 1000 / 60)}m {(sensorStatus().buzzer?.silence_remaining_ms || 0) % 60000 / 1000}s
                      </div>
                    </Show>
                  </div>
                  
                  <div class="stat">
                    <div class="stat-title">Buzzer Type</div>
                    <div class="stat-value text-lg">
                      {sensorStatus().buzzer?.buzzer_type || 'ACTIVE'}
                    </div>
                    <div class="stat-desc">
                      {sensorStatus().buzzer?.buzzer_type === 'ACTIVE' ? 'Active buzzer (simple on/off)' : 'Passive buzzer (tone generation)'}
                    </div>
                  </div>
                </div>

                {/* Buzzer Control Buttons */}
                <div class="flex gap-2 mt-4">
                  <button 
                    class="btn btn-warning btn-sm"
                    onClick={() => {
                      fetch('/buzzer/silence', { method: 'POST' })
                        .then(response => {
                          if (response.ok) {
                            // Status will be updated on next refresh
                          }
                        })
                        .catch(error => {
                          console.error('Buzzer silence error:', error);
                        });
                    }}
                    disabled={!sensorStatus().buzzer?.has_active_alert}
                  >
                    Silence Alerts
                  </button>
                  <button 
                    class={`btn btn-sm ${sensorStatus().buzzer?.has_active_alert && sensorStatus().buzzer?.current_alert_type === 'TEST_ALERT' ? 'btn-error' : 'btn-outline'}`}
                    onClick={() => {
                      if (sensorStatus().buzzer?.has_active_alert && sensorStatus().buzzer?.current_alert_type === 'TEST_ALERT') {
                        // Clear the test alert
                        fetch('/buzzer/clear', { method: 'POST' })
                          .then(response => {
                            if (response.ok) {
                              // Status will be updated on next refresh
                            }
                          })
                          .catch(error => {
                            console.error('Buzzer clear error:', error);
                          });
                      } else {
                        // Trigger test alert
                        fetch('/buzzer/test', { method: 'GET' })
                          .then(response => {
                            if (response.ok) {
                              // Status will be updated on next refresh
                            }
                          })
                          .catch(error => {
                            console.error('Buzzer test error:', error);
                          });
                      }
                    }}
                  >
                    {sensorStatus().buzzer?.has_active_alert && sensorStatus().buzzer?.current_alert_type === 'TEST_ALERT' ? 'Clear Buzzer' : 'Test Buzzer'}
                  </button>
                </div>
              </div>
            </div>
          </Show>

           {/* Light Control Section */}
           <Show when={sensorStatus().light}>
             <div class="card w-full mt-4 bg-base-200 card-sm shadow-sm">
               <div class="card-body">
                 <h2 class="card-title">Light Control</h2>
                 <div class="stats w-full shadow bg-base-300">
                   <div class="stat">
                     <div class="stat-title">Light State</div>
                     <div class={`stat-value text-lg ${sensorStatus().light?.state === 'FAULT' ? 'text-error' :
                                                   sensorStatus().light?.state === 'FADING_IN' || sensorStatus().light?.state === 'FADING_OUT' ? 'text-warning' :
                                                   sensorStatus().light?.state === 'ON' ? 'text-success' : 'text-info'}`}>
                       {sensorStatus().light?.state || 'UNKNOWN'}
                     </div>
                     <div class="stat-desc">
                       {sensorStatus().light?.state === 'FADING_IN' ? 'Fading in' :
                        sensorStatus().light?.state === 'FADING_OUT' ? 'Fading out' :
                        sensorStatus().light?.state === 'ON' ? 'Light is on' :
                        sensorStatus().light?.state === 'OFF' ? 'Light is off' :
                        sensorStatus().light?.state === 'FAULT' ? 'Light fault detected' :
                        'Light state unknown'}
                     </div>
                     <Show when={sensorStatus().light?.test_mode}>
                       <div class="stat-desc text-warning">
                         Test Mode Active
                       </div>
                     </Show>
                   </div>

                   <div class="stat">
                     <div class="stat-title">Brightness</div>
                     <div class="stat-value text-lg">
                       {sensorStatus().light?.current_brightness || 0}%
                     </div>
                     <div class="stat-desc">
                       Max: {sensorStatus().light?.max_brightness || 100}%
                     </div>
                     <div class="stat-desc">
                       <progress
                         class="progress progress-primary w-full mt-2"
                         value={sensorStatus().light?.current_brightness || 0}
                         max={sensorStatus().light?.max_brightness || 100}
                       />
                     </div>
                   </div>

                   <div class="stat">
                     <div class="stat-title">Auto Mode</div>
                     <div class={`stat-value text-lg ${sensorStatus().light?.auto_mode ? 'text-success' : 'text-warning'}`}>
                       {sensorStatus().light?.auto_mode ? 'Enabled' : 'Disabled'}
                     </div>
                     <div class="stat-desc">
                       {sensorStatus().light?.next_scheduled_action || 'No scheduled action'}
                     </div>
                   </div>
                 </div>

                 {/* Light Control Buttons */}
                 <div class="flex gap-2 mt-4 flex-wrap">
                   <button
                     class="btn btn-success btn-sm"
                     onClick={() => {
                       fetch('/light/on', { method: 'GET' })
                         .then(response => {
                           if (response.ok) {
                             // Status will be updated on next refresh
                           }
                         })
                         .catch(error => {
                           console.error('Light on error:', error);
                         });
                     }}
                     disabled={sensorStatus().light?.state === 'ON'}
                   >
                     Turn On
                   </button>
                   <button
                     class="btn btn-error btn-sm"
                     onClick={() => {
                       fetch('/light/off', { method: 'GET' })
                         .then(response => {
                           if (response.ok) {
                             // Status will be updated on next refresh
                           }
                         })
                         .catch(error => {
                           console.error('Light off error:', error);
                         });
                     }}
                     disabled={sensorStatus().light?.state === 'OFF'}
                   >
                     Turn Off
                   </button>
                   <button
                     class="btn btn-primary btn-sm"
                     onClick={() => {
                       fetch('/light/fade_in', { method: 'GET' })
                         .then(response => {
                           if (response.ok) {
                             // Status will be updated on next refresh
                           }
                         })
                         .catch(error => {
                           console.error('Light fade in error:', error);
                         });
                     }}
                     disabled={sensorStatus().light?.state === 'FADING_IN' || sensorStatus().light?.state === 'ON'}
                   >
                     Fade In
                   </button>
                   <button
                     class="btn btn-primary btn-sm"
                     onClick={() => {
                       fetch('/light/fade_out', { method: 'GET' })
                         .then(response => {
                           if (response.ok) {
                             // Status will be updated on next refresh
                           }
                         })
                         .catch(error => {
                           console.error('Light fade out error:', error);
                         });
                     }}
                     disabled={sensorStatus().light?.state === 'FADING_OUT' || sensorStatus().light?.state === 'OFF'}
                   >
                     Fade Out
                   </button>
                 </div>

                 {/* Brightness Slider */}
                 <div class="form-control mt-4">
                   <label class="label">
                     <span class="label-text">Brightness Control</span>
                   </label>
                   <input
                     type="range"
                     min="0"
                     max={sensorStatus().light?.max_brightness || 100}
                     value={sensorStatus().light?.current_brightness || 0}
                     class="range range-primary"
                     aria-label="Light brightness control"
                     onInput={(e) => {
                       const brightness = parseInt((e.target as HTMLInputElement).value);
                       fetch('/light/set_brightness', {
                         method: 'POST',
                         headers: { 'Content-Type': 'application/json' },
                         body: JSON.stringify({ brightness })
                       })
                         .then(response => {
                           if (response.ok) {
                             // Status will be updated on next refresh
                           }
                         })
                         .catch(error => {
                           console.error('Light brightness error:', error);
                         });
                     }}
                   />
                   <div class="label">
                     <span class="label-text-alt">0%</span>
                     <span class="label-text-alt">{sensorStatus().light?.max_brightness || 100}%</span>
                   </div>
                 </div>

                 {/* Light Statistics */}
                 <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
                   <div class="stat">
                     <div class="stat-title">Statistics</div>
                     <div class="stat-desc text-sm">
                       Total Cycles: {sensorStatus().light?.total_cycles || 0}
                     </div>
                     <div class="stat-desc text-sm">
                       Total On Time: {formatTime(sensorStatus().light?.total_on_time || 0)}
                     </div>
                     <button
                       class="btn btn-xs btn-outline mt-2"
                       onClick={() => {
                         fetch('/light/reset_stats', { method: 'GET' })
                           .then(response => {
                             if (response.ok) {
                               // Status will be updated on next refresh
                             }
                           })
                           .catch(error => {
                             console.error('Light reset stats error:', error);
                           });
                       }}
                     >
                       Reset Statistics
                     </button>
                   </div>

                   <div class="stat">
                     <div class="stat-title">Fade Progress</div>
                     <div class="stat-value text-lg">
                       {sensorStatus().light?.fade_progress || 0}%
                     </div>
                     <div class="stat-desc">
                       <progress
                         class="progress progress-secondary w-full mt-2"
                         value={sensorStatus().light?.fade_progress || 0}
                         max="100"
                       />
                     </div>
                   </div>
                 </div>
               </div>
             </div>
           </Show>

          {/* Door Control Section */}
          <Show when={sensorStatus().door}>
            <div class="card w-full mt-4 bg-base-200 card-sm shadow-sm">
              <div class="card-body">
                <h2 class="card-title">Door Control</h2>
                <div class="stats w-full shadow bg-base-300">
                  <div class="stat">
                    <div class="stat-title">Door State</div>
                    <div class={`stat-value text-lg ${sensorStatus().door?.state === 'FAULT' ? 'text-error' : 
                                                   sensorStatus().door?.state === 'OPENING' || sensorStatus().door?.state === 'CLOSING' ? 'text-warning' : 
                                                   sensorStatus().door?.state === 'OPEN' || sensorStatus().door?.state === 'CLOSED' ? 'text-success' : 'text-info'}`}>
                      {sensorStatus().door?.state || 'UNKNOWN'}
                    </div>
                    <div class="stat-desc">
                      {sensorStatus().door?.state === 'OPENING' ? 'Door is opening' :
                       sensorStatus().door?.state === 'CLOSING' ? 'Door is closing' :
                       sensorStatus().door?.state === 'OPEN' ? 'Door is fully open' :
                       sensorStatus().door?.state === 'CLOSED' ? 'Door is fully closed' :
                       sensorStatus().door?.state === 'FAULT' ? 'Door fault detected' :
                       'Door is idle'}
                    </div>
                  </div>
                  
                  <div class="stat">
                    <div class="stat-title">Position</div>
                    <div class={`stat-value text-lg ${sensorStatus().door?.position === 'OPEN' ? 'text-success' : 
                                                   sensorStatus().door?.position === 'CLOSED' ? 'text-info' : 
                                                   sensorStatus().door?.position === 'PARTIAL' ? 'text-warning' : 'text-error'}`}>
                      {sensorStatus().door?.position || 'UNKNOWN'}
                    </div>
                    <div class="stat-desc">
                      {sensorStatus().door?.position === 'OPEN' ? 'Fully open' :
                       sensorStatus().door?.position === 'CLOSED' ? 'Fully closed' :
                       sensorStatus().door?.position === 'PARTIAL' ? 'Partially open/closed' :
                       'Position unknown'}
                    </div>
                  </div>
                  
                  <div class="stat">
                    <div class="stat-title">Progress</div>
                    <div class="stat-value text-lg">
                      {sensorStatus().door?.progress || 0}%
                    </div>
                    <div class="stat-desc">
                      <progress 
                        class="progress progress-primary w-full mt-2" 
                        value={sensorStatus().door?.progress || 0} 
                        max="100"
                      />
                    </div>
                  </div>
                </div>

                {/* Door Control Buttons */}
                <div class="flex gap-2 mt-4">
                  <button 
                    class="btn btn-success btn-sm"
                    onClick={() => {
                      fetch('/door/open', { method: 'GET' })
                        .then(response => {
                          if (response.ok) {
                            // Status will be updated on next refresh
                          }
                        })
                        .catch(error => {
                          console.error('Door open error:', error);
                        });
                    }}
                    disabled={sensorStatus().door?.state === 'OPENING' || sensorStatus().door?.state === 'OPEN'}
                  >
                    Open
                  </button>
                  <button 
                    class="btn btn-error btn-sm"
                    onClick={() => {
                      fetch('/door/close', { method: 'GET' })
                        .then(response => {
                          if (response.ok) {
                            // Status will be updated on next refresh
                          }
                        })
                        .catch(error => {
                          console.error('Door close error:', error);
                        });
                    }}
                    disabled={sensorStatus().door?.state === 'CLOSING' || sensorStatus().door?.state === 'CLOSED'}
                  >
                    Close
                  </button>
                  <button 
                    class="btn btn-warning btn-sm"
                    onClick={() => {
                      fetch('/door/stop', { method: 'GET' })
                        .then(response => {
                          if (response.ok) {
                            // Status will be updated on next refresh
                          }
                        })
                        .catch(error => {
                          console.error('Door stop error:', error);
                        });
                    }}
                    disabled={sensorStatus().door?.state === 'IDLE' || sensorStatus().door?.state === 'OPEN' || sensorStatus().door?.state === 'CLOSED'}
                  >
                    Stop
                  </button>
                  <Show when={sensorStatus().door?.state === 'FAULT'}>
                    <button 
                      class="btn btn-outline btn-sm"
                      onClick={() => {
                        fetch('/door/clear_fault', { method: 'POST' })
                          .then(response => {
                            if (response.ok) {
                              // Status will be updated on next refresh
                            }
                          })
                          .catch(error => {
                            console.error('Door clear fault error:', error);
                          });
                      }}
                    >
                      Clear Fault
                    </button>
                  </Show>
                </div>
                
                {/* Door Status Details */}
                <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
                  <div class="stat">
                    <div class="stat-title">Auto Mode</div>
                    <div class={`stat-value text-lg ${sensorStatus().door?.auto_mode ? 'text-success' : 'text-warning'}`}>
                      {sensorStatus().door?.auto_mode ? 'Enabled' : 'Disabled'}
                    </div>
                    <div class="stat-desc">
                      {sensorStatus().door?.next_scheduled_action || 'No scheduled action'}
                    </div>
                  </div>
                  
                  <div class="stat">
                    <div class="stat-title">Statistics</div>
                    <div class="stat-desc text-sm">
                      Total Cycles: {sensorStatus().door?.total_cycles || 0}
                    </div>
                    <div class="stat-desc text-sm">
                      Open Time: {formatTime(sensorStatus().door?.total_open_time || 0)}
                    </div>
                    <div class="stat-desc text-sm">
                      Close Time: {formatTime(sensorStatus().door?.total_close_time || 0)}
                    </div>
                    <button 
                      class="btn btn-xs btn-outline mt-2"
                      onClick={() => {
                        fetch('/door/reset_stats', { method: 'GET' })
                          .then(response => {
                            if (response.ok) {
                              // Status will be updated on next refresh
                            }
                          })
                          .catch(error => {
                            console.error('Door reset stats error:', error);
                          });
                      }}
                    >
                      Reset Statistics
                    </button>
                  </div>
                </div>
                
                {/* Hall Sensor Status */}
                <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
                  <div class="stat">
                    <div class="stat-title">Hall Sensors</div>
                    <div class="stat-desc text-sm">
                      Open Sensor: {sensorStatus().door?.hall_open ? 'ACTIVE' : 'INACTIVE'}
                    </div>
                    <div class="stat-desc text-sm">
                      Closed Sensor: {sensorStatus().door?.hall_closed ? 'ACTIVE' : 'INACTIVE'}
                    </div>
                    <Show when={sensorStatus().door?.test_mode}>
                      <div class="stat-desc text-sm text-warning">
                        Test Mode Active
                      </div>
                    </Show>
                  </div>
                  
                  <div class="stat">
                    <div class="stat-title">Test Mode</div>
                    <div class={`stat-value text-lg ${sensorStatus().door?.test_mode ? 'text-warning' : 'text-info'}`}>
                      {sensorStatus().door?.test_mode ? 'ENABLED' : 'DISABLED'}
                    </div>
                    <div class="stat-desc">
                      Test mode simulates door movement without hardware
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </Show>

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

          {/* System Status Section */}
          <div class="card w-full mt-4 bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title">System Status</h2>
              <Show when={systemStatus()} fallback={<p>Loading system status...</p>}>
                <div class="stats stats-vertical shadow">
                  <div class="stat">
                    <div class="stat-title">Uptime</div>
                    <div class="stat-value text-2xl">{systemStatus()?.uptime_formatted}</div>
                  </div>
                  
                  <div class="stat">
                    <div class="stat-title">Memory Usage</div>
                    <div class="stat-value text-2xl">
                      {systemStatus()?.heap_used_percent.toFixed(1)}%
                    </div>
                    <div class="stat-desc">
                      {((systemStatus()?.heap_free || 0) / 1024).toFixed(1)} KB free of {((systemStatus()?.heap_size || 0) / 1024).toFixed(1)} KB
                    </div>
                    <progress 
                      class="progress progress-primary w-full mt-2" 
                      value={systemStatus()?.heap_used_percent || 0} 
                      max="100"
                      classList={{
                        "progress-error": (systemStatus()?.heap_used_percent || 0) > 80,
                        "progress-warning": (systemStatus()?.heap_used_percent || 0) > 60
                      }}
                    />
                  </div>
                  
                  <div class="stat">
                    <div class="stat-title">Chip Info</div>
                    <div class="stat-value text-xl">{systemStatus()?.chip_model}</div>
                    <div class="stat-desc">
                      {systemStatus()?.cpu_freq_mhz || 0} MHz | Flash: {((systemStatus()?.flash_size || 0) / 1024 / 1024).toFixed(1)} MB
                    </div>
                  </div>
                  
                  <Show when={(systemStatus()?.wifi_rssi || 0) !== 0}>
                    <div class="stat">
                      <div class="stat-title">WiFi Signal</div>
                      <div class="stat-value text-2xl">{systemStatus()?.wifi_rssi || 0} dBm</div>
                      <div class="stat-desc">{systemStatus()?.wifi_ssid || "Unknown"}</div>
                    </div>
                  </Show>
                </div>
              </Show>
            </div>
          </div>

        </div>
      )}
    </div>
  )
}

export default Status