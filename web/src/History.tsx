import { createSignal, onMount, onCleanup, Show, createEffect } from 'solid-js'
import { Chart, registerables } from 'chart.js'
import { authenticatedFetch } from './utils/api'

// Register Chart.js components
Chart.register(...registerables)

interface DataPoint {
  timestamp: number
  temperature_f: number
  pump_active: boolean
  flow_rate: number
  light_brightness: number
  door_state: string
  pump_trigger: string
  door_trigger: string
  light_trigger: string
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

  const updateCharts = () => {
    const data = historyData()
    if (data.length === 0) return

    const labels = data.map(d => formatTimestamp(d.timestamp))

    // Temperature Chart
    if (tempChartRef) {
      if (tempChart) {
        tempChart.destroy()
      }
      tempChart = new Chart(tempChartRef, {
        type: 'line',
        data: {
          labels,
          datasets: [{
            label: 'Temperature (°F)',
            data: data.map(d => isNaN(d.temperature_f) ? null : d.temperature_f),
            borderColor: 'rgb(59, 130, 246)',
            backgroundColor: 'rgba(59, 130, 246, 0.1)',
            tension: 0.3,
            spanGaps: true
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { display: true }
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
      if (pumpChart) {
        pumpChart.destroy()
      }
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
            fill: true
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { display: true },
            tooltip: {
              callbacks: {
                label: (context) => {
                  const dataPoint = data[context.dataIndex]
                  const state = dataPoint.pump_active ? 'ON' : 'OFF'
                  return `Pump: ${state} (Trigger: ${dataPoint.pump_trigger})`
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
      if (flowChart) {
        flowChart.destroy()
      }
      flowChart = new Chart(flowChartRef, {
        type: 'line',
        data: {
          labels,
          datasets: [{
            label: 'Flow Rate (GPM)',
            data: data.map(d => d.flow_rate),
            borderColor: 'rgb(168, 85, 247)',
            backgroundColor: 'rgba(168, 85, 247, 0.1)',
            tension: 0.3
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { display: true }
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
      if (lightChart) {
        lightChart.destroy()
      }
      lightChart = new Chart(lightChartRef, {
        type: 'line',
        data: {
          labels,
          datasets: [{
            label: 'Light Brightness (%)',
            data: data.map(d => d.light_brightness),
            borderColor: 'rgb(234, 179, 8)',
            backgroundColor: 'rgba(234, 179, 8, 0.1)',
            tension: 0.3
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { display: true },
            tooltip: {
              callbacks: {
                label: (context) => {
                  const dataPoint = data[context.dataIndex]
                  return `Brightness: ${dataPoint.light_brightness}% (Trigger: ${dataPoint.light_trigger})`
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

    // Door Chart
    if (doorChartRef) {
      if (doorChart) {
        doorChart.destroy()
      }

      // Map door states to numeric values for visualization
      const doorStateToValue = (state: string) => {
        const upperState = state.toUpperCase()
        if (upperState === 'OPEN') return 2
        if (upperState === 'CLOSED') return 0
        if (upperState === 'OPENING') return 1.5
        if (upperState === 'CLOSING') return 0.5
        if (upperState === 'FAULT') return -1
        return 1 // IDLE or UNKNOWN
      }

      doorChart = new Chart(doorChartRef, {
        type: 'line',
        data: {
          labels,
          datasets: [{
            label: 'Door State',
            data: data.map(d => doorStateToValue(d.door_state)),
            borderColor: 'rgb(168, 85, 247)',
            backgroundColor: 'rgba(168, 85, 247, 0.2)',
            stepped: true,
            fill: true
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { display: true },
            tooltip: {
              callbacks: {
                label: (context) => {
                  const dataPoint = data[context.dataIndex]
                  return `State: ${dataPoint.door_state} (Trigger: ${dataPoint.door_trigger})`
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
                  if (value === 1) return 'IDLE'
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
    const data = historyData()
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
                    <div class="stat-value text-lg">{historyData().length}</div>
                    <div class="stat-desc">Samples collected</div>
                  </div>
                </div>

                <div class="flex gap-2 flex-wrap">
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

          <Show when={historyData().length > 0} fallback={
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
