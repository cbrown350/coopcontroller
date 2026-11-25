import { createSignal, onMount, onCleanup, createEffect } from 'solid-js'

interface LogEntry {
  uuid: string
  timestamp: number
  message: string
}

function Logs() {
  // Bound the client-side list so a tab left open for days cannot grow memory forever.
  const MAX_LOG_ENTRIES = 500

  const [loading, setLoading] = createSignal(true)
  const [logs, setLogs] = createSignal<LogEntry[]>([])
  const [error, setError] = createSignal('')
  const [isAtBottom, setIsAtBottom] = createSignal(true)
  let intervalId: number | null = null
  let scrollTimerId: number | null = null
  let initialScrollTimerId: number | null = null
  let inFlight = false
  let logContainerRef: HTMLDivElement | undefined

  const formatTimestamp = (timestamp: number): string => {
    try {
      const date = new Date(timestamp * 1000)
      return date.toLocaleString()
    } catch (err) {
      return timestamp.toString()
    }
  }

  const fetchLogs = async () => {
    // Skip overlapping ticks instead of stacking requests on the ESP32.
    if (inFlight) return
    inFlight = true
    try {
      const response = await fetch('/logs')
      if (!response.ok) {
        throw new Error(`Failed to fetch logs: ${response.status} ${response.statusText}`)
      }
      const logData = await response.json() as {
        logs: LogEntry[]
      }

      const existingUuids = new Set(logs().map(log => log.uuid))

      const parsedLogs: LogEntry[] = []
      for (const line of logData.logs) {
        if (!existingUuids.has(line.uuid)) {
          parsedLogs.push(line)
          existingUuids.add(line.uuid)
        }
      }

      if (parsedLogs.length > 0) {
        // Chronological order (oldest first) so new entries appear at the bottom.
        // Timestamps are epoch seconds — compare numerically, not via Date.
        const allLogs = [...logs(), ...parsedLogs].sort((a, b) => a.timestamp - b.timestamp)
        setLogs(allLogs.length > MAX_LOG_ENTRIES ? allLogs.slice(-MAX_LOG_ENTRIES) : allLogs)
      }

      setError('')
      setLoading(false)
    } catch (err: any) {
      setError(`Error fetching logs: ${err.message || 'Unknown error'}`)
      console.error('Failed to fetch logs:', err)
      setLoading(false)
    } finally {
      inFlight = false
    }
  }

  const startAutoRefresh = () => {
    if (intervalId) clearInterval(intervalId)
    intervalId = setInterval(fetchLogs, 5000) // Refresh every 5 seconds
  }

  const stopAutoRefresh = () => {
    if (intervalId) {
      clearInterval(intervalId)
      intervalId = null
    }
  }

  const scrollToBottom = () => {
    if (logContainerRef) {
      logContainerRef.scrollTop = logContainerRef.scrollHeight
    }
  }

  const checkIfAtBottom = () => {
    if (logContainerRef) {
      const { scrollTop, scrollHeight, clientHeight } = logContainerRef
      // Consider "at bottom" if within 5 pixels of the bottom
      const atBottom = scrollHeight - scrollTop - clientHeight < 5
      setIsAtBottom(atBottom)
    }
  }

  const handleScroll = () => {
    checkIfAtBottom()
  }

  // Auto-scroll when logs change, but only if user is at bottom
  createEffect(() => {
    logs()
    if (isAtBottom()) {
      if (scrollTimerId !== null) clearTimeout(scrollTimerId)
      scrollTimerId = window.setTimeout(() => {
        scrollTimerId = null
        scrollToBottom()
      }, 0)
    }
  })

  const handleVisibilityChange = () => {
    if (document.hidden) {
      // Pause polling for background tabs.
      stopAutoRefresh()
    } else {
      fetchLogs()
      startAutoRefresh()
    }
  }

  onMount(async () => {
    setLoading(true)
    await fetchLogs()
    startAutoRefresh()
    // Ensure we start at the bottom
    initialScrollTimerId = window.setTimeout(() => {
      initialScrollTimerId = null
      scrollToBottom()
      setIsAtBottom(true)
    }, 100)

    document.addEventListener('visibilitychange', handleVisibilityChange)
  })

  onCleanup(() => {
    stopAutoRefresh()
    document.removeEventListener('visibilitychange', handleVisibilityChange)
    if (scrollTimerId !== null) {
      clearTimeout(scrollTimerId)
      scrollTimerId = null
    }
    if (initialScrollTimerId !== null) {
      clearTimeout(initialScrollTimerId)
      initialScrollTimerId = null
    }
  })

  return (
    <div>
      {error() && (
        <div role="alert" class="mb-4 alert alert-error">
          {error()}
        </div>
      )}


      {loading() && logs().length === 0 ? (
        <p>Loading logs... <span class="loading loading-spinner loading-xl"></span></p>
      ) : (
        <div class="max-h-160 overflow-y-auto bg-black text-green-400 p-4 rounded font-mono text-sm border border-gray-600" ref={el => logContainerRef = el} onScroll={handleScroll}>
          {logs().length === 0 ? (
            <div class="text-center text-green-400/70">No logs available</div>
          ) : (
            logs().map((log) => (
              <div class="whitespace-pre-wrap">
                <span class="text-green-800">{formatTimestamp(log.timestamp)}:</span> {log.message}
              </div>
            ))
          )}
        </div>
      )}

      <div class="mt-4 text-sm text-base-content/70">
        <p>Logs are automatically refreshed every 5 seconds.</p>
      </div>
    </div>
  )
}

export default Logs 