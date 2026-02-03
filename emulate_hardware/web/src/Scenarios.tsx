import { createSignal, onMount, onCleanup, Show, For } from 'solid-js'
import { Scenario, ScenarioId, CustomScenarioInput } from './types'
import ScenarioEditor from './ScenarioEditor'

function Scenarios() {
  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal('')
  const [success, setSuccess] = createSignal('')
  const [scenarios, setScenarios] = createSignal<Scenario[]>([])
  const [customScenarios, setCustomScenarios] = createSignal<Scenario[]>([])
  const [activeScenario, setActiveScenario] = createSignal<Scenario | null>(null)
  const [applying, setApplying] = createSignal(false)
  const [showEditor, setShowEditor] = createSignal(false)
  const [editingScenario, setEditingScenario] = createSignal<CustomScenarioInput | null>(null)
  const [deleteConfirm, setDeleteConfirm] = createSignal<string | null>(null)

  const loadScenarios = async () => {
    try {
      const [scenariosRes, customRes, activeRes] = await Promise.all([
        fetch('/emulator/scenarios'),
        fetch('/emulator/scenarios/custom'),
        fetch('/emulator/scenario/active')
      ])

      if (!scenariosRes.ok) throw new Error(`Failed to load scenarios: ${scenariosRes.status}`)
      if (!activeRes.ok) throw new Error(`Failed to load active scenario: ${activeRes.status}`)

      const scenariosData = await scenariosRes.json()
      const activeData = await activeRes.json()

      setScenarios(scenariosData)
      setActiveScenario(activeData)

      // Load custom scenarios (may be empty array)
      if (customRes.ok) {
        const customData = await customRes.json()
        setCustomScenarios(Array.isArray(customData) ? customData : [])
      }

      setLoading(false)
      setError('')
    } catch (err: any) {
      console.error('Failed to load scenarios:', err)
      setError(`Error: ${err.message || 'Unknown error'}`)
      setLoading(false)
    }
  }

  const refreshActive = async () => {
    try {
      const res = await fetch('/emulator/scenario/active')
      if (res.ok) {
        const data = await res.json()
        setActiveScenario(data)
      }
    } catch (err) {
      console.error('Failed to refresh active scenario:', err)
    }
  }

  const loadCustomScenarios = async () => {
    try {
      const res = await fetch('/emulator/scenarios/custom')
      if (res.ok) {
        const data = await res.json()
        setCustomScenarios(Array.isArray(data) ? data : [])
      }
    } catch (err) {
      console.error('Failed to load custom scenarios:', err)
    }
  }

  onMount(() => {
    loadScenarios()
    const intervalId = setInterval(refreshActive, 2000)
    onCleanup(() => clearInterval(intervalId))
  })

  const applyScenario = async (id: ScenarioId) => {
    setApplying(true)
    try {
      const formData = new FormData()
      formData.append('id', id.toString())

      const res = await fetch('/emulator/scenario/apply', {
        method: 'POST',
        body: formData
      })

      if (!res.ok) throw new Error(`Failed to apply scenario: ${res.status}`)

      const data = await res.json()
      if (data.success) {
        await refreshActive()
      }
    } catch (err: any) {
      console.error('Failed to apply scenario:', err)
      setError(`Failed to apply scenario: ${err.message}`)
    } finally {
      setApplying(false)
    }
  }

  const applyCustomScenario = async (name: string) => {
    setApplying(true)
    try {
      const formData = new FormData()
      formData.append('name', name)

      const res = await fetch('/emulator/scenarios/custom/apply', {
        method: 'POST',
        body: formData
      })

      if (!res.ok) throw new Error(`Failed to apply custom scenario: ${res.status}`)

      const data = await res.json()
      if (data.success) {
        setSuccess(`Applied: ${name}`)
        setTimeout(() => setSuccess(''), 3000)
        await refreshActive()
      }
    } catch (err: any) {
      console.error('Failed to apply custom scenario:', err)
      setError(`Failed to apply custom scenario: ${err.message}`)
    } finally {
      setApplying(false)
    }
  }

  const saveCustomScenario = async (scenario: CustomScenarioInput) => {
    const res = await fetch('/emulator/scenarios/custom/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(scenario)
    })

    if (!res.ok) {
      const data = await res.json()
      throw new Error(data.error || 'Failed to save scenario')
    }

    setSuccess(`Saved: ${scenario.name}`)
    setTimeout(() => setSuccess(''), 3000)
    setShowEditor(false)
    setEditingScenario(null)
    await loadCustomScenarios()
  }

  const deleteCustomScenario = async (name: string) => {
    try {
      const formData = new FormData()
      formData.append('name', name)

      const res = await fetch('/emulator/scenarios/custom/delete', {
        method: 'POST',
        body: formData
      })

      if (!res.ok) throw new Error('Failed to delete scenario')

      setSuccess(`Deleted: ${name}`)
      setTimeout(() => setSuccess(''), 3000)
      setDeleteConfirm(null)
      await loadCustomScenarios()
    } catch (err: any) {
      setError(`Failed to delete: ${err.message}`)
    }
  }

  const startEditScenario = (scenario: Scenario) => {
    setEditingScenario({
      name: scenario.name,
      description: scenario.description,
      auto_simulate_door: scenario.auto_simulate_door,
      simulate_door_stuck: scenario.simulate_door_stuck,
      door_position: scenario.door_position,
      door_state: scenario.door_state,
      auto_generate_pulses: scenario.auto_generate_pulses,
      simulate_frozen_line: scenario.simulate_frozen_line,
      flow_rate_gpm: scenario.flow_rate_gpm,
      inject_door_fault: scenario.inject_door_fault,
      enable_override: scenario.enable_override,
      override_hall_open: scenario.override_hall_open,
      override_hall_close: scenario.override_hall_close,
      override_door_fault: scenario.override_door_fault,
      override_manual_switch: scenario.override_manual_switch
    })
    setShowEditor(true)
  }

  const getScenarioIcon = (id: ScenarioId) => {
    switch (id) {
      case ScenarioId.NORMAL:
        return (
          <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z" />
          </svg>
        )
      case ScenarioId.FREEZE_CONDITION:
        return (
          <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 3v1m0 16v1m9-9h-1M4 12H3m15.364 6.364l-.707-.707M6.343 6.343l-.707-.707m12.728 0l-.707.707M6.343 17.657l-.707.707" />
          </svg>
        )
      case ScenarioId.DOOR_STUCK_OPEN:
      case ScenarioId.DOOR_STUCK_CLOSED:
        return (
          <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8 11V7a4 4 0 118 0m-4 8v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2z" />
          </svg>
        )
      case ScenarioId.MOTOR_FAULT:
        return (
          <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
          </svg>
        )
      case ScenarioId.FROZEN_WATER_LINE:
      case ScenarioId.PUMP_FAILURE:
        return (
          <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19.428 15.428a2 2 0 00-1.022-.547l-2.387-.477a6 6 0 00-3.86.517l-.318.158a6 6 0 01-3.86.517L6.05 15.21a2 2 0 00-1.806.547M8 4h8l-1 1v5.172a2 2 0 00.586 1.414l5 5c1.26 1.26.367 3.414-1.415 3.414H4.828c-1.782 0-2.674-2.154-1.414-3.414l5-5A2 2 0 009 10.172V5L8 4z" />
          </svg>
        )
      case ScenarioId.CUSTOM:
        return (
          <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M11 4a2 2 0 114 0v1a1 1 0 001 1h3a1 1 0 011 1v3a1 1 0 01-1 1h-1a2 2 0 100 4h1a1 1 0 011 1v3a1 1 0 01-1 1h-3a1 1 0 01-1-1v-1a2 2 0 10-4 0v1a1 1 0 01-1 1H7a1 1 0 01-1-1v-3a1 1 0 00-1-1H4a2 2 0 110-4h1a1 1 0 001-1V7a1 1 0 011-1h3a1 1 0 001-1V4z" />
          </svg>
        )
      default:
        return (
          <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M10.325 4.317c.426-1.756 2.924-1.756 3.35 0a1.724 1.724 0 002.573 1.066c1.543-.94 3.31.826 2.37 2.37a1.724 1.724 0 001.065 2.572c1.756.426 1.756 2.924 0 3.35a1.724 1.724 0 00-1.066 2.573c.94 1.543-.826 3.31-2.37 2.37a1.724 1.724 0 00-2.572 1.065c-.426 1.756-2.924 1.756-3.35 0a1.724 1.724 0 00-2.573-1.066c-1.543.94-3.31-.826-2.37-2.37a1.724 1.724 0 00-1.065-2.572c-1.756-.426-1.756-2.924 0-3.35a1.724 1.724 0 001.066-2.573c-.94-1.543.826-3.31 2.37-2.37.996.608 2.296.07 2.572-1.065z" />
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" />
          </svg>
        )
    }
  }

  const getScenarioColor = (id: ScenarioId) => {
    switch (id) {
      case ScenarioId.NORMAL:
        return 'text-success'
      case ScenarioId.FREEZE_CONDITION:
        return 'text-info'
      case ScenarioId.DOOR_STUCK_OPEN:
      case ScenarioId.DOOR_STUCK_CLOSED:
        return 'text-warning'
      case ScenarioId.MOTOR_FAULT:
        return 'text-error'
      case ScenarioId.FROZEN_WATER_LINE:
      case ScenarioId.PUMP_FAILURE:
        return 'text-accent'
      case ScenarioId.CUSTOM:
        return 'text-secondary'
      default:
        return 'text-base-content'
    }
  }

  const getButtonColor = (id: ScenarioId, isActive: boolean) => {
    if (isActive) return 'btn-primary'
    switch (id) {
      case ScenarioId.NORMAL:
        return 'btn-success btn-outline'
      case ScenarioId.FREEZE_CONDITION:
        return 'btn-info btn-outline'
      case ScenarioId.DOOR_STUCK_OPEN:
      case ScenarioId.DOOR_STUCK_CLOSED:
        return 'btn-warning btn-outline'
      case ScenarioId.MOTOR_FAULT:
        return 'btn-error btn-outline'
      case ScenarioId.FROZEN_WATER_LINE:
      case ScenarioId.PUMP_FAILURE:
        return 'btn-accent btn-outline'
      case ScenarioId.CUSTOM:
        return 'btn-secondary btn-outline'
      default:
        return 'btn-outline'
    }
  }

  return (
    <div>
      <Show when={loading()}>
        <p class="flex items-center gap-2">
          Loading scenarios...
          <span class="loading loading-spinner loading-md"></span>
        </p>
      </Show>

      <Show when={error()}>
        <div role="alert" class="alert alert-error mb-4">
          <svg class="w-5 h-5" fill="currentColor" viewBox="0 0 20 20">
            <path fill-rule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z" clip-rule="evenodd" />
          </svg>
          <span>{error()}</span>
          <button class="btn btn-sm btn-ghost" onClick={() => setError('')}>✕</button>
        </div>
      </Show>

      <Show when={success()}>
        <div role="alert" class="alert alert-success mb-4">
          <svg class="w-5 h-5" fill="currentColor" viewBox="0 0 20 20">
            <path fill-rule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zm3.707-9.293a1 1 0 00-1.414-1.414L9 10.586 7.707 9.293a1 1 0 00-1.414 1.414l2 2a1 1 0 001.414 0l4-4z" clip-rule="evenodd" />
          </svg>
          <span>{success()}</span>
        </div>
      </Show>

      {/* Scenario Editor Modal */}
      <Show when={showEditor()}>
        <div class="mb-6">
          <ScenarioEditor
            initialScenario={editingScenario() || undefined}
            isEditing={!!editingScenario()}
            onSave={saveCustomScenario}
            onCancel={() => {
              setShowEditor(false)
              setEditingScenario(null)
            }}
          />
        </div>
      </Show>

      <Show when={!loading() && scenarios().length > 0 && !showEditor()}>
        <div class="space-y-6">
          {/* Active Scenario Card */}
          <div class="card bg-primary text-primary-content shadow-lg">
            <div class="card-body">
              <h2 class="card-title flex items-center gap-2">
                <span class="w-3 h-3 rounded-full bg-primary-content animate-pulse"></span>
                Active Scenario
              </h2>
              <Show when={activeScenario()}>
                <div class="flex items-center gap-4">
                  <div class={`${getScenarioColor(activeScenario()!.id)} text-primary-content`}>
                    {getScenarioIcon(activeScenario()!.id)}
                  </div>
                  <div>
                    <h3 class="text-xl font-bold">{activeScenario()!.name}</h3>
                    <p class="text-primary-content/80">{activeScenario()!.description}</p>
                  </div>
                </div>

                {/* Active scenario details */}
                <div class="mt-4 grid grid-cols-2 md:grid-cols-4 gap-2 text-sm">
                  <div class="badge badge-ghost">
                    Door: {activeScenario()!.auto_simulate_door ? 'Auto' : 'Manual'}
                  </div>
                  <div class="badge badge-ghost">
                    Water: {activeScenario()!.auto_generate_pulses ? 'Auto' : 'Manual'}
                  </div>
                  <Show when={activeScenario()!.simulate_door_stuck}>
                    <div class="badge badge-warning">Door Stuck</div>
                  </Show>
                  <Show when={activeScenario()!.simulate_frozen_line}>
                    <div class="badge badge-info">Frozen Line</div>
                  </Show>
                  <Show when={activeScenario()!.inject_door_fault}>
                    <div class="badge badge-error">Door Fault</div>
                  </Show>
                </div>
              </Show>
            </div>
          </div>

          {/* Custom Scenarios Section */}
          <div class="card bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <div class="flex items-center justify-between mb-4">
                <div>
                  <h2 class="card-title text-secondary">
                    <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                      <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M11 4a2 2 0 114 0v1a1 1 0 001 1h3a1 1 0 011 1v3a1 1 0 01-1 1h-1a2 2 0 100 4h1a1 1 0 011 1v3a1 1 0 01-1 1h-3a1 1 0 01-1-1v-1a2 2 0 10-4 0v1a1 1 0 01-1 1H7a1 1 0 01-1-1v-3a1 1 0 00-1-1H4a2 2 0 110-4h1a1 1 0 001-1V7a1 1 0 011-1h3a1 1 0 001-1V4z" />
                    </svg>
                    Custom Scenarios
                  </h2>
                  <p class="text-sm text-base-content/60">
                    Create and save your own test scenarios ({customScenarios().length}/8)
                  </p>
                </div>
                <button
                  class="btn btn-secondary btn-sm gap-2"
                  onClick={() => {
                    setEditingScenario(null)
                    setShowEditor(true)
                  }}
                  disabled={customScenarios().length >= 8}
                >
                  <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 4v16m8-8H4" />
                  </svg>
                  New Scenario
                </button>
              </div>

              <Show when={customScenarios().length === 0}>
                <div class="text-center py-8 text-base-content/50">
                  <svg class="w-12 h-12 mx-auto mb-2" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 11H5m14 0a2 2 0 012 2v6a2 2 0 01-2 2H5a2 2 0 01-2-2v-6a2 2 0 012-2m14 0V9a2 2 0 00-2-2M5 11V9a2 2 0 012-2m0 0V5a2 2 0 012-2h6a2 2 0 012 2v2M7 7h10" />
                  </svg>
                  <p>No custom scenarios yet</p>
                  <p class="text-sm">Click "New Scenario" to create one</p>
                </div>
              </Show>

              <Show when={customScenarios().length > 0}>
                <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
                  <For each={customScenarios()}>
                    {(scenario) => {
                      const isActive = () => activeScenario()?.name === scenario.name && activeScenario()?.is_custom

                      return (
                        <div class={`card bg-base-100 shadow-sm ${isActive() ? 'ring-2 ring-secondary' : ''}`}>
                          <div class="card-body p-4">
                            <div class="flex items-start justify-between">
                              <div class="text-secondary">
                                {getScenarioIcon(ScenarioId.CUSTOM)}
                              </div>
                              <div class="flex gap-1">
                                <button
                                  class="btn btn-ghost btn-xs"
                                  title="Edit"
                                  onClick={() => startEditScenario(scenario)}
                                >
                                  <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M11 5H6a2 2 0 00-2 2v11a2 2 0 002 2h11a2 2 0 002-2v-5m-1.414-9.414a2 2 0 112.828 2.828L11.828 15H9v-2.828l8.586-8.586z" />
                                  </svg>
                                </button>
                                <button
                                  class="btn btn-ghost btn-xs text-error"
                                  title="Delete"
                                  onClick={() => setDeleteConfirm(scenario.name)}
                                >
                                  <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
                                  </svg>
                                </button>
                              </div>
                            </div>

                            <h3 class="font-bold mt-2">{scenario.name}</h3>
                            <p class="text-xs text-base-content/60 line-clamp-2">
                              {scenario.description || 'No description'}
                            </p>

                            <div class="card-actions justify-end mt-2">
                              <button
                                class={`btn btn-sm ${isActive() ? 'btn-secondary' : 'btn-secondary btn-outline'}`}
                                disabled={applying()}
                                onClick={() => applyCustomScenario(scenario.name)}
                              >
                                {applying() ? (
                                  <span class="loading loading-spinner loading-xs"></span>
                                ) : isActive() ? (
                                  'Active'
                                ) : (
                                  'Apply'
                                )}
                              </button>
                            </div>

                            {/* Delete confirmation */}
                            <Show when={deleteConfirm() === scenario.name}>
                              <div class="alert alert-warning mt-2 p-2">
                                <span class="text-xs">Delete this scenario?</span>
                                <div class="flex gap-1">
                                  <button
                                    class="btn btn-xs btn-error"
                                    onClick={() => deleteCustomScenario(scenario.name)}
                                  >
                                    Delete
                                  </button>
                                  <button
                                    class="btn btn-xs btn-ghost"
                                    onClick={() => setDeleteConfirm(null)}
                                  >
                                    Cancel
                                  </button>
                                </div>
                              </div>
                            </Show>
                          </div>
                        </div>
                      )
                    }}
                  </For>
                </div>
              </Show>
            </div>
          </div>

          {/* Predefined Scenario Grid */}
          <div class="card bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title">Predefined Scenarios</h2>
              <p class="text-sm text-base-content/60 mb-4">
                Click on a scenario to apply it instantly. The emulator will configure all settings automatically.
              </p>

              <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
                <For each={scenarios()}>
                  {(scenario) => {
                    const isActive = () => activeScenario()?.id === scenario.id && !activeScenario()?.is_custom

                    return (
                      <div
                        class={`card bg-base-100 shadow-sm cursor-pointer transition-all hover:shadow-md ${isActive() ? 'ring-2 ring-primary' : ''}`}
                        onClick={() => !applying() && applyScenario(scenario.id)}
                      >
                        <div class="card-body p-4">
                          <div class="flex items-start justify-between">
                            <div class={`${getScenarioColor(scenario.id)}`}>
                              {getScenarioIcon(scenario.id)}
                            </div>
                            <Show when={isActive()}>
                              <span class="badge badge-primary badge-sm">Active</span>
                            </Show>
                          </div>

                          <h3 class="font-bold mt-2">{scenario.name}</h3>
                          <p class="text-xs text-base-content/60 line-clamp-2">
                            {scenario.description}
                          </p>

                          <div class="card-actions justify-end mt-2">
                            <button
                              class={`btn btn-sm ${getButtonColor(scenario.id, isActive())}`}
                              disabled={applying()}
                              onClick={(e) => {
                                e.stopPropagation()
                                applyScenario(scenario.id)
                              }}
                            >
                              {applying() ? (
                                <span class="loading loading-spinner loading-xs"></span>
                              ) : isActive() ? (
                                'Active'
                              ) : (
                                'Apply'
                              )}
                            </button>
                          </div>
                        </div>
                      </div>
                    )
                  }}
                </For>
              </div>
            </div>
          </div>

          {/* Scenario Details Legend */}
          <div class="card bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title text-sm">Scenario Effects</h2>
              <div class="overflow-x-auto">
                <table class="table table-sm">
                  <thead>
                    <tr>
                      <th>Scenario</th>
                      <th>Door</th>
                      <th>Water</th>
                      <th>Faults</th>
                    </tr>
                  </thead>
                  <tbody>
                    <For each={scenarios()}>
                      {(scenario) => (
                        <tr class={activeScenario()?.id === scenario.id && !activeScenario()?.is_custom ? 'bg-primary/10' : ''}>
                          <td class="font-medium">{scenario.name}</td>
                          <td>
                            {scenario.auto_simulate_door ? 'Auto' : 'Manual'}
                            {scenario.simulate_door_stuck && ' (Stuck)'}
                          </td>
                          <td>
                            {scenario.auto_generate_pulses ? `Auto @ ${scenario.flow_rate_gpm} GPM` : 'Manual'}
                            {scenario.simulate_frozen_line && ' (Frozen)'}
                          </td>
                          <td>
                            {scenario.inject_door_fault ? 'Door Fault' : 'None'}
                          </td>
                        </tr>
                      )}
                    </For>
                  </tbody>
                </table>
              </div>
            </div>
          </div>
        </div>
      </Show>
    </div>
  )
}

export default Scenarios
