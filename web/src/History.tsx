import { createSignal, onMount, onCleanup, Show, createEffect } from 'solid-js'
import { Chart, registerables } from 'chart.js'
import zoomPlugin from 'chartjs-plugin-zoom'
import { authenticatedFetch } from './utils/api'

// Register Chart.js components and zoom plugin
Chart.register(...registerables, zoomPlugin)

// Event type to point style mapping
const EVENT_POINT_STYLES: Record<string, { style: string; radius: number; color: string }> = {
  temp:  { style: 'circle',   radius: 5, color: 'rgb(59, 130, 246)' },
  pump:  { style: 'rectRot',  radius: 6, color: 'rgb(239, 68, 68)' },
  flow:  { style: 'triangle', radius: 6, color: 'rgb(168, 85, 247)' },
  light: { style: 'star',     radius: 7, color: 'rgb(234, 179, 8)' },
  door:  { style: 'rectRounded', radius: 6, color: 'rgb(239, 68, 68)' },
}

const DEFAULT_POINT = { style: 'circle' as const, radius: 2, color: 'rgb(156, 163, 175)' }

// Zoom plugin options shared across all charts
const zoomOptions = {
  zoom: {
    wheel: { enabled: true },
    pinch: { enabled: true },
    mode: 'x' as const,
  },
  pan: {
    enabled: true,
    mode: 'x' as const,
  },
}

type TimePeriod = '1h' | '6h' | '24h' | 'all'

interface DataPoint {
  timestamp: number
  temperature_f: number
  pump_active: boolean
  flow_rate: number
  light_brightness: number
  door_state: string
  door_position: string
  pump_trigger: string
  door_trigger: string
  light_trigger: string
  event_type: string
}

function History() {
  let initialAutoRefresh = false
  if (typeof window !== 'undefined') {
    try {
      initialAutoRefresh = sessionStorage.getItem('historyAutoRefresh') === 'true'
    } catch {
      // Ignore storage errors (private mode, quota, etc.)
    }
  }

  const [loading, setLoading] = createSignal(true)
  const [historyData, setHistoryData] = createSignal<DataPoint[]>([])
  const [error, setError] = createSignal('')
  const [autoRefresh, setAutoRefresh] = createSignal(initialAutoRefresh)
  const [timePeriod, setTimePeriod] = createSignal<TimePeriod>('all')
  const autoRefreshStorageKey = 'historyAutoRefresh'

  let tempChartRef: HTMLCanvasElement | undefined
  let pumpChartRef: HTMLCanvasElement | undefined
  let flowChartRef: HTMLCanvasElement | undefined
  let lightChartRef: HTMLCanvasElement | undefined
  let doorChartRef: HTMLCanvasElement | undefined

  let tempChart: Chart | null = null
  let pumpChart: Chart | null = null
  let flowChart: Chart | null = null
  let lightChart: Chart | null = null
  let doorChart: Chart | null = null

  const fetchHistoryData = async () => {
    try {
      const response = await fetch('/data/history')
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`)
      }
      const data = await response.json()
      setHistoryData(data)
      setError('')
    } catch (err: any) {
      console.error('Failed to fetch history:', err)
      setError(`Error loading history: ${err.message || 'Unknown error'}`)
    } finally {
      setLoading(false)
    }
  }

  const filteredData = () => {
    const data = historyData()
    const period = timePeriod()
    if (period === 'all' || data.length === 0) return data

    const now = Math.max(...data.map(d => d.timestamp))
    const hoursMap: Record<string, number> = { '1h': 1, '6h': 6, '24h': 24 }
    const cutoff = now - (hoursMap[period] || 24) * 3600

    return data.filter(d => d.timestamp >= cutoff)
  }

  const downloadCSV = async () => {
    try {
      const response = await fetch('/data/export_csv')
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`)
      }
      const blob = await response.blob()
      const url = window.URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = 'coop_history.csv'
      document.body.appendChild(a)
      a.click()
      window.URL.revokeObjectURL(url)
      document.body.removeChild(a)
    } catch (err: any) {
      console.error('Failed to download CSV:', err)
      setError(`Error downloading CSV: ${err.message}`)
    }
  }

  const clearHistory = async () => {
    if (!confirm('Are you sure you want to clear all historical data? This cannot be undone.')) {
      return
    }

    try {
      const response = await authenticatedFetch('/data/clear', { method: 'POST' })
      if (response.ok) {
        await fetchHistoryData()
      }
    } catch (err: any) {
      console.error('Failed to clear history:', err)
      setError(`Error clearing history: ${err.message}`)
    }
  }

  const formatTimestamp = (timestamp: number) => {
    const date = new Date(timestamp * 1000)
    return date.toLocaleTimeString()
  }

  const getPointStyle = (eventType: string) => {
    return EVENT_POINT_STYLES[eventType] || DEFAULT_POINT
  }

  const resetZoom = () => {
    [tempChart, pumpChart, flowChart, lightChart, doorChart].forEach(chart => {
      if (chart) chart.resetZoom()
    })
  }

  const updateCharts = () => {
    const data = filteredData()
    if (data.length === 0) return

    const labels = data.map(d => formatTimestamp(d.timestamp))

    // Temperature Chart
    if (tempChartRef) {
      if (tempChart) tempChart.destroy()
      tempChart = new Chart(tempChartRef, {
        type: 'line',
        data: {
          labels,
          datasets: [{
            label: 'Temperature (\u00B0F)',
            data: data.map(d => isNaN(d.temperature_f) ? null : d.temperature_f),
            borderColor: 'rgb(59, 130, 246)',
            backgroundColor: 'rgba(59, 130, 246, 0.1)',
            tension: 0.3,
            spanGaps: true,
            pointStyle: data.map(d => getPointStyle(d.event_type).style) as any,
            pointRadius: data.map(d => d.event_type === 'temp' ? getPointStyle('temp').radius : 1),
            pointBackgroundColor: data.map(d => d.event_type === 'temp' ? getPointStyle('temp').color : 'rgb(59, 130, 246)')
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { display: true },
            zoom: zoomOptions,
            tooltip: {
              callbacks: {
                label: (context) => {
                  const dataPoint = data[context.dataIndex]
                  const eventTag = dataPoint.event_type === 'temp' ? ' [event]' : ''
                  return `Temp: ${dataPoint.temperature_f}\u00B0F${eventTag}`
                }
              }
            }
          },
          scales: {
            x: { display: false },
            y: { beginAtZero: false }
          }
        }
      })
    }

    // Pump Chart
    if (pumpChartRef) {
      if (pumpChart) pumpChart.destroy()
      pumpChart = new Chart(pumpChartRef, {
        type: 'line',
        data: {
          labels,
          datasets: [{
            label: 'Pump State',
            data: data.map(d => d.pump_active ? 1 : 0),
            borderColor: 'rgb(34, 197, 94)',
            backgroundColor: 'rgba(34, 197, 94, 0.2)',
            stepped: true,
            fill: true,
            pointStyle: data.map(d => d.event_type === 'pump' ? getPointStyle('pump').style : 'circle') as any,
            pointRadius: data.map(d => d.event_type === 'pump' ? getPointStyle('pump').radius : 2),
            pointBackgroundColor: data.map(d => d.event_type === 'pump' ? getPointStyle('pump').color : 'rgb(34, 197, 94)')
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { display: true },
            zoom: zoomOptions,
            tooltip: {
              callbacks: {
                label: (context) => {
                  const dataPoint = data[context.dataIndex]
                  const state = dataPoint.pump_active ? 'ON' : 'OFF'
                  const eventTag = dataPoint.event_type === 'pump' ? ' [event]' : ''
                  return `Pump: ${state} (Trigger: ${dataPoint.pump_trigger})${eventTag}`
                }
              }
            }
          },
          scales: {
            x: { display: false },
            y: {
              beginAtZero: true,
              max: 1,
              ticks: {
                stepSize: 1,
                callback: (value) => value === 1 ? 'ON' : 'OFF'
              }
            }
          }
        }
      })
    }

    // Flow Chart
    if (flowChartRef) {
      if (flowChart) flowChart.destroy()
      flowChart = new Chart(flowChartRef, {
        type: 'line',
        data: {
          labels,
          datasets: [{
            label: 'Flow Rate (GPM)',
            data: data.map(d => d.flow_rate),
            borderColor: 'rgb(168, 85, 247)',
            backgroundColor: 'rgba(168, 85, 247, 0.1)',
            tension: 0.3,
            pointStyle: data.map(d => d.event_type === 'flow' ? getPointStyle('flow').style : 'circle') as any,
            pointRadius: data.map(d => d.event_type === 'flow' ? getPointStyle('flow').radius : 2),
            pointBackgroundColor: data.map(d => d.event_type === 'flow' ? getPointStyle('flow').color : 'rgb(168, 85, 247)')
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { display: true },
            zoom: zoomOptions,
            tooltip: {
              callbacks: {
                label: (context) => {
                  const dataPoint = data[context.dataIndex]
                  const eventTag = dataPoint.event_type === 'flow' ? ' [event]' : ''
                  return `Flow: ${dataPoint.flow_rate.toFixed(3)} GPM${eventTag}`
                }
              }
            }
          },
          scales: {
            x: { display: false },
            y: { beginAtZero: true }
          }
        }
      })
    }

    // Light Chart
    if (lightChartRef) {
      if (lightChart) lightChart.destroy()
      lightChart = new Chart(lightChartRef, {
        type: 'line',
        data: {
          labels,
          datasets: [{
            label: 'Light Brightness (%)',
            data: data.map(d => d.light_brightness),
            borderColor: 'rgb(234, 179, 8)',
            backgroundColor: 'rgba(234, 179, 8, 0.1)',
            tension: 0.3,
            pointStyle: data.map(d => d.event_type === 'light' ? getPointStyle('light').style : 'circle') as any,
            pointRadius: data.map(d => d.event_type === 'light' ? getPointStyle('light').radius : 2),
            pointBackgroundColor: data.map(d => d.event_type === 'light' ? getPointStyle('light').color : 'rgb(234, 179, 8)')
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { display: true },
            zoom: zoomOptions,
            tooltip: {
              callbacks: {
                label: (context) => {
                  const dataPoint = data[context.dataIndex]
                  const eventTag = dataPoint.event_type === 'light' ? ' [event]' : ''
                  return `Brightness: ${dataPoint.light_brightness}% (Trigger: ${dataPoint.light_trigger})${eventTag}`
                }
              }
            }
          },
          scales: {
            x: { display: false },
            y: { beginAtZero: true, max: 100 }
          }
        }
      })
    }

    // Door Chart - shows both state and position
    if (doorChartRef) {
      if (doorChart) doorChart.destroy()

      const doorStateToValue = (state: string) => {
        const upperState = state.toUpperCase()
        if (upperState === 'OPEN') return 2
        if (upperState === 'CLOSED') return 0
        if (upperState === 'OPENING') return 1.5
        if (upperState === 'CLOSING') return 0.5
        if (upperState === 'FAULT') return -1
        return 1 // IDLE or UNKNOWN
      }

      const doorPositionToValue = (position: string) => {
        const upper = position.toUpperCase()
        if (upper === 'OPEN') return 2
        if (upper === 'CLOSED') return 0
        if (upper === 'PARTIAL') return 1
        return 1 // UNKNOWN
      }

      doorChart = new Chart(doorChartRef, {
        type: 'line',
        data: {
          labels,
          datasets: [
            {
              label: 'Door State',
              data: data.map(d => doorStateToValue(d.door_state)),
              borderColor: 'rgb(168, 85, 247)',
              backgroundColor: 'rgba(168, 85, 247, 0.2)',
              stepped: true,
              fill: true,
              pointStyle: data.map(d => d.event_type === 'door' ? getPointStyle('door').style : 'circle') as any,
              pointRadius: data.map(d => d.event_type === 'door' ? getPointStyle('door').radius : 2),
              pointBackgroundColor: data.map(d => d.event_type === 'door' ? getPointStyle('door').color : 'rgb(168, 85, 247)')
            },
            {
              label: 'Door Position',
              data: data.map(d => doorPositionToValue(d.door_position || 'UNKNOWN')),
              borderColor: 'rgb(34, 197, 94)',
              borderDash: [5, 5],
              stepped: true,
              fill: false,
              pointStyle: data.map(d => d.event_type === 'door' ? getPointStyle('door').style : 'circle') as any,
              pointRadius: data.map(d => d.event_type === 'door' ? 4 : 1),
              pointBackgroundColor: data.map(d => d.event_type === 'door' ? getPointStyle('door').color : 'rgb(34, 197, 94)')
            }
          ]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { display: true },
            zoom: zoomOptions,
            tooltip: {
              callbacks: {
                label: (context) => {
                  const dataPoint = data[context.dataIndex]
                  const eventTag = dataPoint.event_type === 'door' ? ' [event]' : ''
                  if (context.datasetIndex === 0) {
                    return `State: ${dataPoint.door_state} (Trigger: ${dataPoint.door_trigger})${eventTag}`
                  }
                  return `Position: ${dataPoint.door_position || 'N/A'}${eventTag}`
                }
              }
            }
          },
          scales: {
            x: { display: false },
            y: {
              beginAtZero: false,
              min: -1,
              max: 2,
              ticks: {
                stepSize: 1,
                callback: (value) => {
                  if (value === 2) return 'OPEN'
                  if (value === 1) return 'PARTIAL/IDLE'
                  if (value === 0) return 'CLOSED'
                  if (value === -1) return 'FAULT'
                  return ''
                }
              }
            }
          }
        }
      })
    }
  }

  createEffect(() => {
    const data = filteredData()
    if (data.length === 0) return

    // Retry mechanism to ensure refs are set before rendering charts
    const tryUpdateCharts = (attempts = 0) => {
      if (tempChartRef && pumpChartRef && flowChartRef && lightChartRef && doorChartRef) {
        updateCharts()
      } else if (attempts < 10) {
        // Retry after a short delay if refs aren't ready yet
        setTimeout(() => tryUpdateCharts(attempts + 1), 50)
      }
    }

    // Start the retry process
    setTimeout(() => tryUpdateCharts(), 0)
  })

  createEffect(() => {
    if (typeof window === 'undefined') return
    try {
      sessionStorage.setItem(autoRefreshStorageKey, String(autoRefresh()))
    } catch {
      // Ignore storage errors (private mode, quota, etc.)
    }
  })

  onMount(() => {
    fetchHistoryData()

    const intervalId = setInterval(() => {
      if (autoRefresh()) {
        fetchHistoryData()
      }
    }, 30000) // Refresh every 30 seconds if enabled

    onCleanup(() => {
      clearInterval(intervalId)
      if (tempChart) tempChart.destroy()
      if (pumpChart) pumpChart.destroy()
      if (flowChart) flowChart.destroy()
      if (lightChart) lightChart.destroy()
      if (doorChart) doorChart.destroy()
    })
  })

  return (
    <div>
      <h2 class="text-2xl font-bold mb-4">Historical Data</h2>

      {loading() ? (
        <p>Loading history... <span class="loading loading-spinner loading-xl"></span></p>
      ) : (
        <div>
          {error() && (
            <div class="alert alert-error mb-4">
              <span>{error()}</span>
            </div>
          )}

          {/* Controls */}
          <div class="card w-full mt-4 bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <div class="flex flex-wrap gap-2 items-center justify-between">
                <div class="stats shadow bg-base-300">
                  <div class="stat">
                    <div class="stat-title">Data Points</div>
                    <div class="stat-value text-lg">{filteredData().length}</div>
                    <div class="stat-desc">
                      {(() => {
                        const d = filteredData()
                        const counts = { temp: 0, flow: 0, pump: 0, light: 0, door: 0 }
                        d.forEach(p => { if (p.event_type in counts) counts[p.event_type as keyof typeof counts]++ })
                        return `temp:${counts.temp} flow:${counts.flow} pump:${counts.pump} light:${counts.light} door:${counts.door}`
                      })()}
                    </div>
                  </div>
                </div>

                <div class="flex gap-2 flex-wrap items-center">
                  {/* Time period filter */}
                  <div class="join">
                    {(['1h', '6h', '24h', 'all'] as TimePeriod[]).map(period => (
                      <button
                        class={`join-item btn btn-xs ${timePeriod() === period ? 'btn-active btn-primary' : ''}`}
                        onClick={() => setTimePeriod(period)}
                      >
                        {period === 'all' ? 'All' : period}
                      </button>
                    ))}
                  </div>

                  <button
                    class="btn btn-ghost btn-xs"
                    onClick={resetZoom}
                    title="Reset chart zoom"
                  >
                    Reset Zoom
                  </button>

                  <button
                    class="btn btn-primary btn-sm"
                    onClick={fetchHistoryData}
                  >
                    Refresh
                  </button>
                  <button
                    class="btn btn-success btn-sm"
                    onClick={downloadCSV}
                  >
                    Download CSV
                  </button>
                  <button
                    class="btn btn-error btn-sm"
                    onClick={clearHistory}
                  >
                    Clear History
                  </button>
                  <label class="label cursor-pointer gap-2">
                    <span class="label-text">Auto-refresh</span>
                    <input
                      type="checkbox"
                      class="toggle toggle-sm"
                      checked={autoRefresh()}
                      onChange={(e) => setAutoRefresh(e.target.checked)}
                    />
                  </label>
                </div>
              </div>
            </div>
          </div>

          {/* Point style legend */}
          <div class="card w-full mt-2 bg-base-200 card-sm shadow-sm">
            <div class="card-body py-2">
              <div class="flex flex-wrap gap-4 text-xs items-center">
                <span class="font-semibold">Event markers:</span>
                <span title="Temperature event">&#9679; Temp</span>
                <span title="Pump event">&#9670; Pump</span>
                <span title="Flow event">&#9650; Flow</span>
                <span title="Light event">&#9733; Light</span>
                <span title="Door event">&#9632; Door</span>
                <span class="text-base-content/50">Scroll to zoom, drag to pan</span>
              </div>
            </div>
          </div>

          <Show when={filteredData().length > 0} fallback={
            <div class="alert alert-info mt-4">
              <span>No historical data available yet. Data will be collected automatically.</span>
            </div>
          }>
            {/* Temperature Chart */}
            <div class="card w-full mt-4 bg-base-200 shadow-sm">
              <div class="card-body">
                <h3 class="card-title">Temperature</h3>
                <div style={{ height: '300px' }}>
                  <canvas ref={tempChartRef}></canvas>
                </div>
              </div>
            </div>

            {/* Pump State Chart */}
            <div class="card w-full mt-4 bg-base-200 shadow-sm">
              <div class="card-body">
                <h3 class="card-title">Pump State</h3>
                <div style={{ height: '200px' }}>
                  <canvas ref={pumpChartRef}></canvas>
                </div>
              </div>
            </div>

            {/* Flow Rate Chart */}
            <div class="card w-full mt-4 bg-base-200 shadow-sm">
              <div class="card-body">
                <h3 class="card-title">Water Flow Rate</h3>
                <div style={{ height: '300px' }}>
                  <canvas ref={flowChartRef}></canvas>
                </div>
              </div>
            </div>

            {/* Light Brightness Chart */}
            <div class="card w-full mt-4 bg-base-200 shadow-sm">
              <div class="card-body">
                <h3 class="card-title">Light Brightness</h3>
                <div style={{ height: '300px' }}>
                  <canvas ref={lightChartRef}></canvas>
                </div>
              </div>
            </div>

            {/* Door State Chart */}
            <div class="card w-full mt-4 bg-base-200 shadow-sm">
              <div class="card-body">
                <h3 class="card-title">Door State</h3>
                <div style={{ height: '250px' }}>
                  <canvas ref={doorChartRef}></canvas>
                </div>
              </div>
            </div>
          </Show>
        </div>
      )}
    </div>
  )
}

export default History
