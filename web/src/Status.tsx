import { createSignal, onMount, onCleanup, Show, For, createEffect } from 'solid-js'
import { SystemStatus, WeatherStatus } from './types'
import { authenticatedFetch } from './utils/api'

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
      total_cycles: 0,
      pump_off_flow_detected: false,
      scheduled_cycle_active: false,
      time_until_next_scheduled: 0
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
      auto_open_enabled: false,
      auto_close_enabled: false,
      test_mode: false,
      lockout_enabled: false,
      hall_open: false,
      hall_closed: false,
      total_open_time: 0,
      total_close_time: 0,
      total_cycles: 0,
      next_scheduled_action: 'No scheduled action',
      weather_postponed: false,
      auto_calc_timeout_enabled: false,
      recommended_open_timeout: 0,
      recommended_close_timeout: 0
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
  // Failures from control actions (pump/door/light/buzzer). Kept separate from
  // the polling error so a successful poll does not wipe the message before the
  // user can read it.
  const [actionError, setActionError] = createSignal('')
  const bannerError = () => error() || actionError()
  const dismissErrors = () => {
    setError('')
    setActionError('')
  }
  
  // Sunrise/sunset state
  const [sunTimes, setSunTimes] = createSignal({
    sunrise: '--:-- --',
    sunset: '--:-- --',
    sunrise_minutes: 0,
    sunset_minutes: 0
  })

  // Weather state (null until first fetch / when disabled)
  const [weatherStatus, setWeatherStatus] = createSignal<WeatherStatus | null>(null)

  const [localBrightness, setLocalBrightness] = createSignal(0)
  const [isDraggingBrightness, setIsDraggingBrightness] = createSignal(false)

  // Polling / debounce bookkeeping. These are deliberately plain locals (not
  // signals): they guard request pile-up and must not trigger re-renders.
  let statusInFlight = false
  let sensorOnlyInFlight = false
  let weatherInFlight = false
  let sunInFlight = false
  let brightnessTimer: number | undefined

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

  const formatTime = (seconds: number) => {
    const hours = Math.floor(seconds / 3600)
    const minutes = Math.floor((seconds % 3600) / 60)
    const secs = seconds % 60
    return `${hours}h ${minutes}m ${secs}s`
  }

  const formatMinutesToTime = (minutes: number) => {
    const hours = Math.floor(minutes / 60)
    const mins = minutes % 60
    const period = hours >= 12 ? 'PM' : 'AM'
    const displayHours = hours === 0 ? 12 : hours > 12 ? hours - 12 : hours
    return `${displayHours}:${mins.toString().padStart(2, '0')} ${period}`
  }

  // Weather unit label per OpenWeatherMap units system
  const weatherUnitLabel = (units: string, kind: 'temp' | 'wind') => {
    if (kind === 'temp') {
      if (units === 'metric') return '°C'
      if (units === 'standard') return 'K'
      return '°F'
    }
    // wind
    return units === 'imperial' ? 'mph' : 'm/s'
  }

  // Forecast timestamps are unix epoch seconds
  const formatForecastTime = (epochSeconds: number) => {
    if (!epochSeconds) return '--'
    const d = new Date(epochSeconds * 1000)
    let hours = d.getHours()
    const period = hours >= 12 ? 'PM' : 'AM'
    hours = hours % 12
    if (hours === 0) hours = 12
    return `${hours}${period}`
  }

  // Human readable "how old is this weather snapshot" (5m ago / 2h 15m ago / 4d 10h ago)
  const formatAge = (seconds: number) => {
    if (!isFinite(seconds) || seconds < 0) return 'unknown'
    const total = Math.floor(seconds)
    if (total < 60) return `${total}s ago`
    const minutes = Math.floor(total / 60)
    if (minutes < 60) return `${minutes}m ago`
    const hours = Math.floor(minutes / 60)
    const remMinutes = minutes % 60
    if (hours < 24) return remMinutes > 0 ? `${hours}h ${remMinutes}m ago` : `${hours}h ago`
    const days = Math.floor(hours / 24)
    const remHours = hours % 24
    return remHours > 0 ? `${days}d ${remHours}h ago` : `${days}d ago`
  }

  // Compact local timestamp for "last ran" lines (e.g. "Nov 24, 7:55 PM (3m ago)")
  const formatLastRun = (epochSeconds?: number): string => {
    if (typeof epochSeconds !== 'number' || epochSeconds <= 0) return 'Never'
    const d = new Date(epochSeconds * 1000)
    const formatted = d.toLocaleString(undefined, {
      month: 'short',
      day: 'numeric',
      hour: 'numeric',
      minute: '2-digit',
    })
    const age = Math.floor(Date.now() / 1000) - epochSeconds
    return age >= 0 ? `${formatted} (${formatAge(age)})` : formatted
  }

  // Prefer the absolute last-success timestamp so the label advances between polls;
  // fall back to current.fetch_time or the device-reported age.
  const weatherDataAgeSeconds = (): number | null => {
    const weather = weatherStatus()
    if (!weather) return null
    const fetchTime = weather.last_weather_success_epoch || weather.current?.fetch_time
    if (typeof fetchTime === 'number' && fetchTime > 0) {
      const age = Math.floor(Date.now() / 1000) - fetchTime
      if (age >= 0) return age
    }
    if (typeof weather.data_age_seconds === 'number') return weather.data_age_seconds
    return null
  }

  const weatherAgeLabel = (): string | null => {
    const age = weatherDataAgeSeconds()
    return age === null ? null : formatAge(age)
  }

  // Filter out any 3-hour forecast blocks that have already ended (client-side
  // defense in depth alongside firmware pruning).
  const activeForecast = () => {
    const weather = weatherStatus()
    if (!weather || weather.stale || weather.forecast_expired || !weather.forecast) return []
    const nowSec = Math.floor(Date.now() / 1000)
    return weather.forecast.filter((fc) => !fc.dt || fc.dt + 10800 > nowSec)
  }

  const isForecastExpired = (): boolean => {
    const weather = weatherStatus()
    if (!weather) return false
    if (weather.forecast_expired || weather.stale) return true
    if (weather.forecast && weather.forecast.length > 0 && activeForecast().length === 0) return true
    return false
  }

  // True when the LLM decider was configured but fell back to the rule engine or went stale.
  const llmRuleFallback = (): boolean => {
    const weather = weatherStatus()
    if (!weather) return false
    // Standby and pending evaluations are normal states, not fallbacks.
    if (weather.llm_standby || weather.llm_pending) return false
    if (weather.llm_enabled && weather.llm_stale) return true
    if (weather.decider !== 'llm') return false
    return (weather.decision_reason ?? '').toLowerCase().startsWith('llm unavailable')
  }

  const refreshSunTimes = async () => {
    if (sunInFlight) return
    sunInFlight = true
    try {
      const response = await fetch('/sun/times')
      if (!response.ok) {
        throw new Error(`HTTP ${response.status} ${response.statusText}`)
      }
      const data = await response.json()
      setSunTimes(data)
    } catch (err: any) {
      console.error('Failed to fetch sun times:', err)
    } finally {
      sunInFlight = false
    }
  }

  const refreshWeatherStatus = async () => {
    if (weatherInFlight) return
    weatherInFlight = true
    try {
      const response = await fetch('/weather/status')
      if (!response.ok) {
        throw new Error(`HTTP ${response.status} ${response.statusText}`)
      }
      const weatherData = await response.json()
      // Only show the card when the weather feature is enabled
      setWeatherStatus(weatherData?.enabled ? weatherData : null)
    } catch (err: any) {
      console.error('Failed to fetch weather status:', err)
    } finally {
      weatherInFlight = false
    }
  }

  // Fast poll: the two small, frequently changing endpoints only. Sun times and
  // weather move on the order of minutes/hours, so polling them here just
  // multiplies the request load on the ESP32.
  const refreshFastStatus = async () => {
    if (statusInFlight) return
    statusInFlight = true
    try {
      const [sensorResponse, systemResponse] = await Promise.all([
        fetch('/sensor_status'),
        fetch('/system_status')
      ])

      if (!sensorResponse.ok) {
        throw new Error(`HTTP error! status: ${sensorResponse.status} ${sensorResponse.statusText}`)
      }

      setSensorStatus(await sensorResponse.json())

      if (systemResponse.ok) {
        setSystemStatus(await systemResponse.json())
      }

      setLoading(false)
      // Clear any existing error when data is successfully fetched
      setError('')
    } catch (err: any) {
      console.error('Failed to fetch status:', err)
      setError(`Error loading status: ${err.message || 'Unknown error'}`)
      setLoading(false)
    } finally {
      statusInFlight = false
    }
  }

  // Lightweight post-action refresh: /sensor_status only (the fast poll picks up
  // /system_status within a few seconds anyway).
  const refreshSensorOnly = async () => {
    if (sensorOnlyInFlight) return
    sensorOnlyInFlight = true
    try {
      const response = await fetch('/sensor_status')
      if (!response.ok) {
        throw new Error(`HTTP ${response.status} ${response.statusText}`)
      }
      setSensorStatus(await response.json())
    } catch (err: any) {
      console.error('Failed to refresh sensor status:', err)
    } finally {
      sensorOnlyInFlight = false
    }
  }

  // Shared control-action runner: reports failures visibly instead of silently
  // swallowing them, and only refreshes /sensor_status afterwards.
  const runControlAction = async (url: string, label: string, options: RequestInit = { method: 'GET' }) => {
    try {
      const response = await authenticatedFetch(url, options)
      if (!response.ok) {
        let detail = ''
        try {
          detail = (await response.text()).trim()
        } catch {
          // ignore body read errors
        }
        setActionError(`${label} failed (HTTP ${response.status}${detail ? `: ${detail}` : ''})`)
        return
      }
      setActionError('')
      await refreshSensorOnly()
    } catch (err: any) {
      console.error(`${label} error:`, err)
      setActionError(`${label} failed: ${err?.message || 'Network error'}`)
    }
  }

  const handlePumpControl = (action: string) =>
    runControlAction(`/pump/${action}`, `Pump action "${action}"`)

  const handleClearOffFlowDetected = () =>
    runControlAction('/pump/clear_off_flow_detected', 'Clear off-flow warning')

  const handleWaterReset = (sensor: number) =>
    runControlAction(`/water/reset/${sensor}`, `Water counter reset (sensor ${sensor})`)

  const handleLightAction = (action: string) =>
    runControlAction(`/light/${action}`, `Light action "${action}"`)

  const handleBuzzerAction = (action: string, options: RequestInit) =>
    runControlAction(`/buzzer/${action}`, `Buzzer action "${action}"`, options)

  const handleDoorAction = (action: string, options: RequestInit = { method: 'GET' }) =>
    runControlAction(`/door/${action}`, `Door action "${action}"`, options)

  const handleBrightnessChange = async (value: number) => {
    // If light is OFF and slider > 0, turn it on first
    if (sensorStatus()?.light?.state === "OFF" && value > 0) {
      const onResponse = await authenticatedFetch('/light/on')
      if (!onResponse.ok) {
        throw new Error(`turning the light on failed (HTTP ${onResponse.status})`)
      }
      await new Promise(resolve => setTimeout(resolve, 100))
    }

    const response = await authenticatedFetch('/light/set_brightness', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ brightness: value })
    })
    if (!response.ok) {
      throw new Error(`setting brightness failed (HTTP ${response.status})`)
    }
  }

  // Debounced so dragging the slider sends one request instead of dozens.
  const debouncedBrightnessChange = (value: number) => {
    setLocalBrightness(value)
    if (brightnessTimer !== undefined) clearTimeout(brightnessTimer)
    brightnessTimer = window.setTimeout(() => {
      brightnessTimer = undefined
      handleBrightnessChange(value).catch((err: any) => {
        console.error('Brightness change error:', err)
        setActionError(`Brightness change failed: ${err?.message || 'Network error'}`)
      })
    }, 250)
  }

  onMount(() => {
    setLoading(true)

    const fastPoll = () => {
      if (document.hidden) return
      refreshFastStatus()
    }
    const weatherPoll = () => {
      if (document.hidden) return
      refreshWeatherStatus()
    }
    const sunPoll = () => {
      if (document.hidden) return
      refreshSunTimes()
    }

    refreshFastStatus()
    refreshWeatherStatus()
    refreshSunTimes()

    const fastIntervalId = window.setInterval(fastPoll, 3000)
    const weatherIntervalId = window.setInterval(weatherPoll, 30000)
    const sunIntervalId = window.setInterval(sunPoll, 600000)

    // Catch up immediately when the tab becomes visible again instead of
    // waiting for the next interval tick.
    const handleVisibilityChange = () => {
      if (document.hidden) return
      refreshFastStatus()
      refreshWeatherStatus()
      refreshSunTimes()
    }
    document.addEventListener('visibilitychange', handleVisibilityChange)

    onCleanup(() => {
      clearInterval(fastIntervalId)
      clearInterval(weatherIntervalId)
      clearInterval(sunIntervalId)
      document.removeEventListener('visibilitychange', handleVisibilityChange)
      if (brightnessTimer !== undefined) {
        clearTimeout(brightnessTimer)
        brightnessTimer = undefined
      }
    })
  })

  createEffect(() => {
    const light = sensorStatus()?.light
    // Never clobber the slider value while the user is dragging it or while a
    // debounced brightness command is still pending.
    if (light && !isDraggingBrightness() && brightnessTimer === undefined) {
      setLocalBrightness(light.current_brightness)
    }
  })

  return (
    <div>
      {/* Visible error banner for status loads and control actions */}
      <Show when={bannerError()}>
        <div role="alert" class="alert alert-error mt-4">
          <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6 shrink-0 stroke-current" fill="none" viewBox="0 0 24 24" stroke-width="1.5">
            <path stroke-linecap="round" stroke-linejoin="round" d="M12 9v3.75m9-.75a9 9 0 11-18 0 9 9 0 0118 0zm-9 3.75h.008v.008H12v-.008z" />
          </svg>
          <span class="text-sm">{bannerError()}</span>
          <button class="btn btn-xs btn-ghost" onClick={dismissErrors}>Dismiss</button>
        </div>
      </Show>

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
                  <Show when={sensorStatus().pump.flow_error}>
                    <div class="stat-desc text-error">Flow Error Detected!</div>
                    <Show when={sensorStatus().pump.time_until_retry > 0}>
                      <div class="stat-desc text-error">Retry in: {formatTime(sensorStatus().pump.time_until_retry)}</div>
                    </Show>
                  </Show>

                  <Show when={sensorStatus().pump.pump_off_flow_detected}>
                    <div class="alert alert-warning mt-2">
                      <svg xmlns="http://www.w3.org/2000/svg" class="stroke-current-shrink-0 h-6 w-6" fill="none" viewBox="0 0 24 24" stroke-width="1.5">
                        <path stroke-linecap="round" stroke-linejoin="round" d="M12 9v2m0 4h.01M12 17v.01" />
                        <path stroke-linecap="round" stroke-linejoin="round" d="M12 9v.01" />
                      </svg>
                      <span class="font-semibold">Water Flow Detected While Pump is OFF</span>
                      <span class="block text-sm">Possible stuck relay or valve leak. Check pump hardware and water system.</span>
                      <button
                        class="btn btn-sm btn-outline mt-2"
                        onClick={handleClearOffFlowDetected}
                      >
                        Clear Warning
                      </button>
                    </div>
                  </Show>

                  <Show when={sensorStatus().pump.scheduled_cycle_active}>
                    <div class="alert alert-info mt-2">
                      <span class="font-semibold">Scheduled Maintenance Cycle Active</span>
                    </div>
                  </Show>

                  <Show when={!sensorStatus().pump.scheduled_cycle_active && sensorStatus().pump.time_until_next_scheduled > 0}>
                    <div class="stat-desc text-xs mt-1">
                      Next scheduled cycle: {formatTime(sensorStatus().pump.time_until_next_scheduled)}
                    </div>
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
                    onClick={() => handleBuzzerAction('silence', { method: 'POST' })}
                    disabled={!sensorStatus().buzzer?.has_active_alert}
                  >
                    Silence Alerts
                  </button>
                  <button
                    class={`btn btn-sm ${sensorStatus().buzzer?.has_active_alert && sensorStatus().buzzer?.current_alert_type === 'TEST_ALERT' ? 'btn-error' : 'btn-outline'}`}
                    onClick={() => {
                      if (sensorStatus().buzzer?.has_active_alert && sensorStatus().buzzer?.current_alert_type === 'TEST_ALERT') {
                        // Clear the test alert
                        handleBuzzerAction('clear', { method: 'POST' })
                      } else {
                        // Trigger test alert
                        handleBuzzerAction('test', { method: 'GET' })
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
                       {localBrightness()}%
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
                     onClick={() => handleLightAction('on')}
                     disabled={sensorStatus().light?.state === 'ON'}
                   >
                     Turn On
                   </button>
                   <button
                     class="btn btn-error btn-sm"
                     onClick={() => handleLightAction('off')}
                     // disabled={sensorStatus().light?.state === 'OFF'} (reverted)
                   >
                     Turn Off
                   </button>
                   <button
                     class="btn btn-primary btn-sm"
                     onClick={() => handleLightAction('fade_in')}
                     disabled={sensorStatus().light?.state === 'ON'}
                   >
                     Fade In
                   </button>
                   <button
                     class="btn btn-primary btn-sm"
                     onClick={() => handleLightAction('fade_out')}
                     disabled={sensorStatus().light?.state === 'OFF'}
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
                     value={localBrightness()}
                     class="range range-primary"
                     aria-label="Light brightness control"
                     onPointerDown={() => setIsDraggingBrightness(true)}
                     onPointerUp={() => setIsDraggingBrightness(false)}
                     onPointerCancel={() => setIsDraggingBrightness(false)}
                     onBlur={() => setIsDraggingBrightness(false)}
                     onInput={(e) => {
                       const brightness = parseInt((e.target as HTMLInputElement).value);
                       debouncedBrightnessChange(brightness);
                     }}
                   />
                   <div class="label">
                     <span class="label-text-alt">{localBrightness()}%</span>
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
                       onClick={() => handleLightAction('reset_stats')}
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

                {/* Lockout Warning */}
                <Show when={sensorStatus().door?.lockout_enabled}>
                  <div class="alert alert-warning mt-2">
                    <svg xmlns="http://www.w3.org/2000/svg" class="stroke-current shrink-0 h-6 w-6" fill="none" viewBox="0 0 24 24" stroke-width="1.5">
                      <path stroke-linecap="round" stroke-linejoin="round" d="M16.5 10.5V6.75a4.5 4.5 0 10-9 0v3.75m-.75 11.25h10.5a2.25 2.25 0 002.25-2.25v-6.75a2.25 2.25 0 00-2.25-2.25H6.75a2.25 2.25 0 00-2.25 2.25v6.75a2.25 2.25 0 002.25 2.25z" />
                    </svg>
                    <span class="font-semibold">Door Lockout Active</span>
                    <span class="block text-sm">All door operations are disabled. Disable lockout to resume control.</span>
                  </div>
                </Show>

                {/* Door Control Buttons */}
                <div class="flex gap-2 mt-4">
                  <button
                    class="btn btn-success btn-sm"
                    onClick={() => handleDoorAction('open')}
                    disabled={sensorStatus().door?.lockout_enabled || sensorStatus().door?.state === 'OPENING' || sensorStatus().door?.state === 'OPEN'}
                  >
                    Open
                  </button>
                  <button
                    class="btn btn-error btn-sm"
                    onClick={() => handleDoorAction('close')}
                    disabled={sensorStatus().door?.lockout_enabled || sensorStatus().door?.state === 'CLOSING' || sensorStatus().door?.state === 'CLOSED'}
                  >
                    Close
                  </button>
                  <button
                    class="btn btn-warning btn-sm"
                    onClick={() => handleDoorAction('stop')}
                    disabled={sensorStatus().door?.lockout_enabled || sensorStatus().door?.state === 'IDLE' || sensorStatus().door?.state === 'OPEN' || sensorStatus().door?.state === 'CLOSED'}
                  >
                    Stop
                  </button>
                  <button
                    class={`btn btn-sm ${sensorStatus().door?.lockout_enabled ? 'btn-error' : 'btn-outline'}`}
                    onClick={() => handleDoorAction(sensorStatus().door?.lockout_enabled ? 'lockout/off' : 'lockout/on')}
                  >
                    {sensorStatus().door?.lockout_enabled ? 'Unlock Door' : 'Lock Door'}
                  </button>
                  <Show when={sensorStatus().door?.state === 'FAULT'}>
                    <button
                      class="btn btn-outline btn-sm"
                      onClick={() => handleDoorAction('clear_fault', { method: 'POST' })}
                    >
                      Clear Fault
                    </button>
                  </Show>
                </div>
                
                {/* Door Status Details */}
                <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mt-4">
                  <div class="stat">
                    <div class="stat-title">Auto Open</div>
                    <div class={`stat-value text-lg ${sensorStatus().door?.auto_open_enabled ? 'text-success' : 'text-warning'}`}>
                      {sensorStatus().door?.auto_open_enabled ? 'Enabled' : 'Disabled'}
                    </div>
                    <div class="stat-desc">
                      {sensorStatus().door?.next_scheduled_action || 'No scheduled action'}
                    </div>
                    <Show when={sensorStatus().door?.weather_postponed}>
                      <div class="stat-desc text-warning font-semibold">
                        ⛆ Opening postponed — inclement weather
                      </div>
                    </Show>
                  </div>

                  <div class="stat">
                    <div class="stat-title">Auto Close</div>
                    <div class={`stat-value text-lg ${sensorStatus().door?.auto_close_enabled ? 'text-success' : 'text-warning'}`}>
                      {sensorStatus().door?.auto_close_enabled ? 'Enabled' : 'Disabled'}
                    </div>
                    <div class="stat-desc">
                      {sensorStatus().door?.auto_close_enabled ? 'Scheduled by sunset' : 'Manual / off-schedule'}
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
                      onClick={() => handleDoorAction('reset_stats')}
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
                       sensorStatus().pump.state as string}
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

          {/* Weather Section (only when the feature is enabled) */}
          <Show when={weatherStatus()}>
            <div class="card w-full mt-4 bg-base-200 card-sm shadow-sm">
              <div class="card-body">
                <h2 class="card-title">
                  Weather
                  <Show when={weatherStatus()!.stale}>
                    <span class="badge badge-error badge-sm">STALE</span>
                  </Show>
                  <Show when={weatherStatus()!.gate_active}>
                    <span class={`badge badge-sm ${weatherStatus()!.good_for_opening ? 'badge-success' : 'badge-warning'}`}>
                      {weatherStatus()!.good_for_opening ? 'OK to open' : 'Hold door'}
                    </span>
                  </Show>
                </h2>

                {/* Stale data: the gate failed open, so the door is schedule-only */}
                <Show when={weatherStatus()!.stale}>
                  <div role="alert" class="alert alert-warning text-sm">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6 shrink-0 stroke-current" fill="none" viewBox="0 0 24 24" stroke-width="1.5">
                      <path stroke-linecap="round" stroke-linejoin="round" d="M12 9v3.75m-9.303 3.376c-.866 1.5.217 3.374 1.948 3.374h14.71c1.73 0 2.813-1.874 1.948-3.374L13.949 3.378c-.866-1.5-3.032-1.5-3.898 0L2.697 16.126zM12 15.75h.008v.008H12v-.008z" />
                    </svg>
                    <span>
                      <span class="font-semibold">Weather data is stale</span> — weather gate failed open, so the
                      door will follow the normal schedule until fresh data arrives.
                      <Show when={weatherAgeLabel()}>
                        {' '}Last successful fetch: {weatherAgeLabel()}.
                      </Show>
                      <Show when={weatherStatus()!.stale_threshold_seconds}>
                        {' '}(data goes stale after {(weatherStatus()!.stale_threshold_seconds! / 60).toFixed(0)} min without a successful fetch)
                      </Show>
                    </span>
                  </div>
                </Show>

                {/* Not yet configured / no data */}
                <Show when={!weatherStatus()!.configured}>
                  <div class="alert alert-warning text-sm">
                    Weather is enabled but no API key is set. Add an OpenWeatherMap API key in Settings.
                  </div>
                </Show>
                <Show when={weatherStatus()!.configured && !weatherStatus()!.current}>
                  <div class="text-sm opacity-70">
                    Waiting for first weather update…
                  </div>
                </Show>

                {/* Last fetch error — shown even when cached data exists, otherwise
                    a multi-day outage stays invisible. */}
                <Show when={weatherStatus()!.last_error}>
                  <div
                    role="alert"
                    class={`alert text-sm ${weatherStatus()!.current ? 'alert-warning' : 'alert-error'}`}
                  >
                    <span>
                      <span class="font-semibold">Last weather fetch failed:</span>{' '}
                      {weatherStatus()!.last_error}
                      <Show when={weatherStatus()!.consecutive_failures}>
                        {' '}({weatherStatus()!.consecutive_failures} consecutive failures)
                      </Show>
                      <Show when={weatherAgeLabel()}>
                        {' '}· cached data is {weatherAgeLabel()}.
                      </Show>
                    </span>
                  </div>
                </Show>

                {/* Current conditions */}
                <Show when={weatherStatus()!.current}>
                  <div class="stats stats-vertical md:stats-horizontal w-full shadow bg-base-300">
                    <div class="stat">
                      <div class="stat-title">Conditions</div>
                      <div class={`stat-value text-lg ${weatherStatus()!.current!.condition === 'GOOD' ? 'text-success' : 'text-warning'}`}>
                        {weatherStatus()!.current!.description || weatherStatus()!.current!.condition}
                      </div>
                      <Show when={weatherStatus()!.last_weather_success_epoch || weatherStatus()!.current!.fetch_time}>
                        <div class="text-xs opacity-60 mt-0.5">
                          Weather last ran: {formatLastRun(weatherStatus()!.last_weather_success_epoch || weatherStatus()!.current!.fetch_time)}
                        </div>
                      </Show>
                      <Show when={
                        weatherStatus()!.decision_reason &&
                        !(weatherStatus()!.llm_enabled && weatherStatus()!.llm_stale) &&
                        !(weatherStatus()!.llm_standby && (weatherStatus()!.decision_reason ?? '').toLowerCase().startsWith('llm unavailable'))
                      }>
                        <div class="stat-desc">
                          Decision: {weatherStatus()!.decision_reason}
                          <Show when={weatherStatus()!.decider}>
                            {' '}({weatherStatus()!.decider === 'llm' ? 'LLM' : 'rules'})
                          </Show>
                        </div>
                      </Show>
                      <Show when={weatherStatus()!.llm_enabled && weatherStatus()!.llm_standby}>
                        <div class="text-xs opacity-60 mt-0.5">
                          LLM standby ({weatherStatus()!.llm_standby_reason || 'outside open window'})
                          <Show when={weatherStatus()!.llm_last_success_epoch}>
                            {' · last ran: '}{formatLastRun(weatherStatus()!.llm_last_success_epoch)}
                          </Show>
                        </div>
                      </Show>
                      <Show when={weatherStatus()!.llm_enabled && !weatherStatus()!.llm_standby && weatherStatus()!.llm_pending}>
                        <div class="text-xs opacity-60 mt-0.5">
                          LLM evaluation pending…
                        </div>
                      </Show>
                      <Show when={weatherStatus()!.llm_enabled && !weatherStatus()!.llm_standby && !weatherStatus()!.llm_pending && !weatherStatus()!.llm_stale}>
                        <div class="text-xs opacity-60 mt-0.5">
                          LLM last ran: {formatLastRun(weatherStatus()!.llm_last_success_epoch)}
                        </div>
                      </Show>
                    </div>

                    <Show when={weatherStatus()!.current!.temp !== undefined}>
                      <div class="stat">
                        <div class="stat-title">Temperature</div>
                        <div class="stat-value text-lg">
                          {weatherStatus()!.current!.temp!.toFixed(0)}{weatherUnitLabel(weatherStatus()!.units, 'temp')}
                        </div>
                        <Show when={weatherStatus()!.current!.feels_like !== undefined}>
                          <div class="stat-desc">Feels {weatherStatus()!.current!.feels_like!.toFixed(0)}{weatherUnitLabel(weatherStatus()!.units, 'temp')}</div>
                        </Show>
                      </div>
                    </Show>

                    <Show when={weatherStatus()!.current!.wind_speed !== undefined}>
                      <div class="stat">
                        <div class="stat-title">Wind</div>
                        <div class="stat-value text-lg">
                          {weatherStatus()!.current!.wind_speed!.toFixed(0)} {weatherUnitLabel(weatherStatus()!.units, 'wind')}
                        </div>
                        <Show when={weatherStatus()!.current!.humidity !== undefined}>
                          <div class="stat-desc">Humidity {weatherStatus()!.current!.humidity}%</div>
                        </Show>
                      </div>
                    </Show>
                  </div>

                  {/* Stale / unavailable LLM connection notice */}
                  <Show when={weatherStatus()!.llm_enabled && !weatherStatus()!.llm_standby && !weatherStatus()!.llm_pending && (weatherStatus()!.llm_stale || llmRuleFallback())}>
                    <div role="alert" class="alert alert-warning text-sm mt-2">
                      <div>
                        <span class="font-semibold">LLM weather check is unavailable or stale</span> — cleared outdated LLM decision and using rule-based fallback.
                        <Show when={weatherStatus()!.llm_last_error}>
                          <div class="text-xs mt-0.5 opacity-90">{weatherStatus()!.llm_last_error}</div>
                        </Show>
                        <div class="text-xs opacity-70 mt-0.5">
                          LLM last ran: {formatLastRun(weatherStatus()!.llm_last_success_epoch)}
                        </div>
                      </div>
                    </div>
                  </Show>

                  {/* Short-term forecast (cleared automatically when blocks have passed) */}
                  <Show
                    when={activeForecast().length > 0}
                    fallback={
                      <Show when={isForecastExpired()}>
                        <div role="alert" class="alert alert-warning text-sm mt-2">
                          <div>
                            <span class="font-semibold">Forecast expired</span> — previous forecast has already passed and could not be updated.
                            <Show when={weatherStatus()!.forecast_error}>
                              <div class="text-xs mt-0.5 opacity-90">{weatherStatus()!.forecast_error}</div>
                            </Show>
                            <div class="text-xs opacity-70 mt-0.5">
                              Forecast last ran: {formatLastRun(weatherStatus()!.last_forecast_success_epoch)}
                            </div>
                          </div>
                        </div>
                      </Show>
                    }
                  >
                    <div class="mt-2">
                      <div class="flex flex-wrap items-baseline justify-between gap-2 mb-1">
                        <span class="text-sm font-semibold">Short-term forecast</span>
                        <span class="text-xs opacity-60">
                          Forecast last ran: {formatLastRun(weatherStatus()!.last_forecast_success_epoch)}
                        </span>
                      </div>
                      <div class="flex flex-wrap gap-2">
                        <For each={activeForecast()}>
                          {(fc) => (
                            <div class="badge badge-outline badge-lg gap-1">
                              <span>{formatForecastTime(fc.dt)}</span>
                              <span class="opacity-70">{fc.description}</span>
                              <Show when={fc.temp !== undefined}>
                                <span>{fc.temp!.toFixed(0)}{weatherUnitLabel(weatherStatus()!.units, 'temp')}</span>
                              </Show>
                              <Show when={fc.precip_prob !== undefined && fc.precip_prob >= 0}>
                                <span class={fc.precip_prob! >= 0.5 ? 'text-warning' : 'opacity-70'}>💧{Math.round(fc.precip_prob! * 100)}%</span>
                              </Show>
                            </div>
                          )}
                        </For>
                      </div>
                    </div>
                  </Show>
                </Show>

                <div class="text-xs opacity-50 mt-1">
                  Updates every {weatherStatus()!.update_interval_minutes} min · engine:{' '}
                  {llmRuleFallback()
                    ? 'llm (fallback to rules)'
                    : (weatherStatus()!.decider ?? 'rules')} ·
                  {' '}{weatherStatus()!.successful_fetches} ok / {weatherStatus()!.failed_fetches} failed
                  <Show when={weatherStatus()!.consecutive_failures}>
                    {' '}· {weatherStatus()!.consecutive_failures} consecutive failures
                  </Show>
                </div>
              </div>
            </div>
          </Show>

          {/* Sunrise/Sunset Section */}
          <div class="card w-full mt-4 bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title">Sunrise & Sunset</h2>
              <div class="stats w-full shadow bg-base-300">
                <div class="stat">
                  <div class="stat-title">Sunrise</div>
                  <div class="stat-value text-lg">{sunTimes().sunrise}</div>
                  <div class="stat-desc text-sm">
                    {formatMinutesToTime(sunTimes().sunrise_minutes)}
                  </div>
                </div>
                
                <div class="stat">
                  <div class="stat-title">Sunset</div>
                  <div class="stat-value text-lg">{sunTimes().sunset}</div>
                  <div class="stat-desc text-sm">
                    {formatMinutesToTime(sunTimes().sunset_minutes)}
                  </div>
                </div>
                
                <div class="stat">
                  <div class="stat-title">Daylight Duration</div>
                  <div class="stat-value text-lg">
                    {sunTimes().sunset_minutes > sunTimes().sunrise_minutes ?
                      `${sunTimes().sunset_minutes - sunTimes().sunrise_minutes} minutes` :
                      'N/A'}
                  </div>
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

                  <Show when={systemStatus()?.reset_reason}>
                    <div class="stat">
                      <div class="stat-title">Last Reboot</div>
                      <div
                        class="stat-value text-xl"
                        classList={{
                          // Power-on (1) and software restart (3) are benign;
                          // everything else (panic=4, watchdog=5/6/7, brownout=9,
                          // power glitch=14) suggests trouble worth attention.
                          "text-warning": [4, 5, 6, 7, 9, 14, 15].includes(systemStatus()?.reset_reason_code ?? -1),
                        }}
                      >
                        {systemStatus()?.reset_reason}
                      </div>
                      <div class="stat-desc">code {systemStatus()?.reset_reason_code ?? "?"}</div>
                    </div>
                  </Show>
                  
                  <Show when={(systemStatus()?.wifi_rssi || 0) !== 0}>
                    <div class="stat">
                      <div class="stat-title">WiFi Signal</div>
                      <div class="stat-value text-2xl">{systemStatus()?.wifi_rssi || 0} dBm</div>
                      <div class="stat-desc">{systemStatus()?.wifi_ssid || "Unknown"}</div>
                    </div>
                  </Show>

                  <Show when={systemStatus()?.wifi_ip}>
                    <div class="stat">
                      <div class="stat-title">Network</div>
                      <div class="stat-value text-lg">{systemStatus()?.wifi_ip}</div>
                      <div class="stat-desc">MAC: {systemStatus()?.wifi_mac || "N/A"}</div>
                      <div class="stat-desc">BSSID: {systemStatus()?.wifi_bssid || "N/A"}</div>
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