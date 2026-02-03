import { createSignal, onMount, onCleanup, Show, For } from 'solid-js'
import type { RecordingMetadata, RecordingStatus } from './types'

function Recordings() {
  const [recordings, setRecordings] = createSignal<RecordingMetadata[]>([])
  const [status, setStatus] = createSignal<RecordingStatus | null>(null)
  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal('')
  const [success, setSuccess] = createSignal('')
  const [label, setLabel] = createSignal('Recording')

  let pollInterval: ReturnType<typeof setInterval> | null = null

  const fetchRecordings = async () => {
    try {
      const [recRes, statusRes] = await Promise.all([
        fetch('/emulator/recordings'),
        fetch('/emulator/recordings/status')
      ])
      if (recRes.ok) {
        const data = await recRes.json()
        setRecordings(data.recordings || [])
      }
      if (statusRes.ok) {
        setStatus(await statusRes.json())
      }
    } catch (err: any) {
      setError(err.message)
    } finally {
      setLoading(false)
    }
  }

  const startPolling = () => {
    pollInterval = setInterval(fetchRecordings, 1000)
  }

  onMount(() => {
    fetchRecordings()
    startPolling()
  })

  onCleanup(() => {
    if (pollInterval) clearInterval(pollInterval)
  })

  const showSuccess = (msg: string) => {
    setSuccess(msg)
    setTimeout(() => setSuccess(''), 3000)
  }

  // ---- Recording controls ----

  const handleStartRecording = async () => {
    try {
      setError('')
      const res = await fetch('/emulator/recordings/start', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ label: label() || 'Recording' })
      })
      const data = await res.json()
      if (!res.ok) throw new Error(data.error || 'Failed to start recording')
      showSuccess('Recording started')
      await fetchRecordings()
    } catch (err: any) {
      setError(err.message)
    }
  }

  const handleStopRecording = async () => {
    try {
      setError('')
      const res = await fetch('/emulator/recordings/stop', { method: 'POST' })
      const data = await res.json()
      if (!res.ok) throw new Error(data.error || 'Failed to stop recording')
      showSuccess('Recording saved')
      await fetchRecordings()
    } catch (err: any) {
      setError(err.message)
    }
  }

  const handleToggleRecordingPause = async () => {
    try {
      setError('')
      const res = await fetch('/emulator/recordings/pause', { method: 'POST' })
      const data = await res.json()
      if (!res.ok) throw new Error(data.error || 'Failed to toggle pause')
      await fetchRecordings()
    } catch (err: any) {
      setError(err.message)
    }
  }

  // ---- Playback controls ----

  const handleStartPlayback = async (id: string) => {
    try {
      setError('')
      const res = await fetch('/emulator/recordings/playback/start', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id, speed_percent: 100 })
      })
      const data = await res.json()
      if (!res.ok) throw new Error(data.error || 'Failed to start playback')
      showSuccess('Playback started')
      await fetchRecordings()
    } catch (err: any) {
      setError(err.message)
    }
  }

  const handleStopPlayback = async () => {
    try {
      setError('')
      const res = await fetch('/emulator/recordings/playback/stop', { method: 'POST' })
      const data = await res.json()
      if (!res.ok) throw new Error(data.error || 'Failed to stop playback')
      await fetchRecordings()
    } catch (err: any) {
      setError(err.message)
    }
  }

  const handleTogglePlaybackPause = async () => {
    try {
      setError('')
      const res = await fetch('/emulator/recordings/playback/pause', { method: 'POST' })
      const data = await res.json()
      if (!res.ok) throw new Error(data.error || 'Failed to toggle pause')
      await fetchRecordings()
    } catch (err: any) {
      setError(err.message)
    }
  }

  const handleSetPlaybackSpeed = async (speed: number) => {
    try {
      const res = await fetch('/emulator/recordings/playback/speed', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ speed_percent: speed })
      })
      if (!res.ok) {
        const data = await res.json()
        throw new Error(data.error || 'Failed to set speed')
      }
    } catch (err: any) {
      setError(err.message)
    }
  }

  // ---- Recording management ----

  const handleDownload = async (id: string, label: string) => {
    try {
      const res = await fetch(`/emulator/recordings/download/${id}`)
      if (!res.ok) throw new Error('Failed to download recording')
      const blob = await res.blob()
      const url = window.URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = `${label.replace(/\s+/g, '_')}.json`
      document.body.appendChild(a)
      a.click()
      window.URL.revokeObjectURL(url)
      document.body.removeChild(a)
    } catch (err: any) {
      setError(err.message)
    }
  }

  const handleDelete = async (id: string) => {
    if (!confirm('Delete this recording?')) return
    try {
      setError('')
      const res = await fetch(`/emulator/recordings/${id}`, { method: 'DELETE' })
      const data = await res.json()
      if (!res.ok) throw new Error(data.error || 'Failed to delete recording')
      showSuccess('Recording deleted')
      await fetchRecordings()
    } catch (err: any) {
      setError(err.message)
    }
  }

  const handleDeleteAll = async () => {
    if (!confirm('Delete all recordings? This cannot be undone.')) return
    try {
      setError('')
      const res = await fetch('/emulator/recordings/all', { method: 'DELETE' })
      const data = await res.json()
      if (!res.ok) throw new Error(data.error || 'Failed to delete recordings')
      showSuccess('All recordings deleted')
      await fetchRecordings()
    } catch (err: any) {
      setError(err.message)
    }
  }

  // ---- Helpers ----

  const formatDuration = (ms: number) => {
    const s = Math.floor(ms / 1000)
    const m = Math.floor(s / 60)
    return `${m}:${String(s % 60).padStart(2, '0')}`
  }

  const isRecording = () => status()?.recording.state === 'RECORDING'
  const isRecordingPaused = () => status()?.recording.state === 'PAUSED'
  const isPlaying = () => status()?.playback.state === 'PLAYING'
  const isPlaybackPaused = () => status()?.playback.state === 'PAUSED'
  const isPlaybackActive = () => isPlaying() || isPlaybackPaused()

  return (
    <div class="space-y-4">
      <h2 class="text-lg font-bold">Log Recordings</h2>

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
        {/* Recording controls */}
        <div class="card bg-base-200 card-sm shadow-sm">
          <div class="card-body">
            <h3 class="card-title">Record Signals</h3>
            <p class="text-sm text-base-content/60 mb-3">
              Capture all monitored and emulated signal states at 100 ms intervals (max 5 min).
            </p>

            <Show when={!isRecording() && !isRecordingPaused()}>
              <div class="flex gap-2 items-end flex-wrap">
                <div class="form-control">
                  <label class="label">
                    <span class="label-text">Recording Label</span>
                  </label>
                  <input
                    type="text"
                    class="input input-bordered input-sm"
                    value={label()}
                    onInput={(e) => setLabel(e.currentTarget.value)}
                    placeholder="Label"
                    maxlength={63}
                  />
                </div>
                <button class="btn btn-error btn-sm" onClick={handleStartRecording}>
                  <span class="inline-block w-3 h-3 rounded-full bg-white mr-1"></span>
                  Record
                </button>
              </div>
            </Show>

            <Show when={isRecording() || isRecordingPaused()}>
              <div class="flex items-center gap-3 flex-wrap">
                <span class={`badge ${isRecording() ? 'badge-error animate-pulse' : 'badge-warning'}`}>
                  {isRecording() ? 'RECORDING' : 'PAUSED'}
                </span>
                <span class="text-sm font-mono">
                  {formatDuration(status()?.recording.duration_ms || 0)} — {status()?.recording.sample_count || 0} samples
                </span>
                <div class="flex gap-2">
                  <button class="btn btn-warning btn-sm" onClick={handleToggleRecordingPause}>
                    {isRecording() ? 'Pause' : 'Resume'}
                  </button>
                  <button class="btn btn-success btn-sm" onClick={handleStopRecording}>
                    Stop & Save
                  </button>
                </div>
              </div>
            </Show>
          </div>
        </div>

        {/* Playback controls */}
        <Show when={isPlaybackActive()}>
          <div class="card bg-base-200 card-sm shadow-sm border border-accent">
            <div class="card-body">
              <h3 class="card-title text-accent">Now Playing</h3>
              <div class="flex items-center gap-3 flex-wrap">
                <span class={`badge ${isPlaying() ? 'badge-accent' : 'badge-warning'}`}>
                  {isPlaying() ? 'PLAYING' : 'PAUSED'}
                </span>
                <span class="text-sm font-mono">
                  {formatDuration(status()?.playback.position_ms || 0)} / {formatDuration(status()?.playback.duration_ms || 0)}
                </span>
                <span class="text-sm text-base-content/60">
                  {status()?.playback.speed_percent || 100}% speed
                </span>
              </div>

              {/* Progress bar */}
              <div class="w-full bg-base-300 rounded-full h-2 mt-3">
                <div
                  class="bg-accent h-2 rounded-full transition-all"
                  style={{
                    width: `${status()?.playback.duration_ms
                      ? (status()!.playback.position_ms / status()!.playback.duration_ms) * 100
                      : 0}%`
                  }}
                ></div>
              </div>

              <div class="flex gap-2 mt-3 flex-wrap items-center">
                <button class="btn btn-warning btn-sm" onClick={handleTogglePlaybackPause}>
                  {isPlaying() ? 'Pause' : 'Resume'}
                </button>
                <button class="btn btn-outline btn-sm" onClick={handleStopPlayback}>
                  Stop
                </button>
                <div class="ml-auto flex gap-1">
                  {[50, 100, 200].map(speed => (
                    <button
                      class={`btn btn-xs ${status()?.playback.speed_percent === speed ? 'btn-accent' : 'btn-outline'}`}
                      onClick={() => handleSetPlaybackSpeed(speed)}
                    >
                      {speed === 50 ? '0.5x' : speed === 100 ? '1x' : '2x'}
                    </button>
                  ))}
                </div>
              </div>
            </div>
          </div>
        </Show>

        {/* Recordings list */}
        <div class="card bg-base-200 card-sm shadow-sm">
          <div class="card-body">
            <div class="flex items-center justify-between">
              <h3 class="card-title">Saved Recordings</h3>
              <Show when={recordings().length > 0}>
                <button class="btn btn-error btn-xs" onClick={handleDeleteAll}>
                  Delete All
                </button>
              </Show>
            </div>

            <Show when={recordings().length === 0}>
              <p class="text-sm text-base-content/60 mt-2">No recordings yet. Start a recording above.</p>
            </Show>

            <Show when={recordings().length > 0}>
              <div class="overflow-x-auto mt-2">
                <table class="table table-sm w-full">
                  <thead>
                    <tr>
                      <th class="text-left">Label</th>
                      <th class="text-right">Duration</th>
                      <th class="text-right">Samples</th>
                      <th class="text-right">Actions</th>
                    </tr>
                  </thead>
                  <tbody>
                    <For each={recordings()}>
                      {(rec) => (
                        <tr>
                          <td class="font-medium">{rec.label}</td>
                          <td class="text-right font-mono text-sm">{formatDuration(rec.duration_ms)}</td>
                          <td class="text-right text-sm">{rec.sample_count}</td>
                          <td class="text-right">
                            <div class="flex gap-1 justify-end">
                              <Show when={!isPlaybackActive()}>
                                <button
                                  class="btn btn-accent btn-xs"
                                  onClick={() => handleStartPlayback(rec.id)}
                                >
                                  Play
                                </button>
                              </Show>
                              <button
                                class="btn btn-outline btn-xs"
                                onClick={() => handleDownload(rec.id, rec.label)}
                              >
                                Download
                              </button>
                              <button
                                class="btn btn-error btn-xs"
                                onClick={() => handleDelete(rec.id)}
                              >
                                Delete
                              </button>
                            </div>
                          </td>
                        </tr>
                      )}
                    </For>
                  </tbody>
                </table>
              </div>
            </Show>
          </div>
        </div>
      </Show>
    </div>
  )
}

export default Recordings
