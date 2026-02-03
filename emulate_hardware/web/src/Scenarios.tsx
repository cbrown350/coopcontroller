import { createSignal, onMount, onCleanup, Show, For } from 'solid-js'
import { Scenario, ScenarioId } from './types'

function Scenarios() {
  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal('')
  const [scenarios, setScenarios] = createSignal<Scenario[]>([])
  const [activeScenario, setActiveScenario] = createSignal<Scenario | null>(null)
  const [applying, setApplying] = createSignal(false)

  const loadScenarios = async () => {
    try {
      const [scenariosRes, activeRes] = await Promise.all([
        fetch('/emulator/scenarios'),
        fetch('/emulator/scenario/active')
      ])

      if (!scenariosRes.ok) throw new Error(`Failed to load scenarios: ${scenariosRes.status}`)
      if (!activeRes.ok) throw new Error(`Failed to load active scenario: ${activeRes.status}`)

      const scenariosData = await scenariosRes.json()
      const activeData = await activeRes.json()

      setScenarios(scenariosData)
      setActiveScenario(activeData)
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
        <div role="alert" class="alert alert-error mb-4">{error()}</div>
      </Show>

      <Show when={!loading() && scenarios().length > 0}>
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

          {/* Scenario Grid */}
          <div class="card bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title">Available Scenarios</h2>
              <p class="text-sm text-base-content/60 mb-4">
                Click on a scenario to apply it instantly. The emulator will configure all settings automatically.
              </p>

              <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
                <For each={scenarios()}>
                  {(scenario) => {
                    const isActive = () => activeScenario()?.id === scenario.id

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
                        <tr class={activeScenario()?.id === scenario.id ? 'bg-primary/10' : ''}>
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
