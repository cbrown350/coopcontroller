import { Component, createSignal, Show } from 'solid-js'
import type { CustomScenarioInput } from './types'

interface ScenarioEditorProps {
  initialScenario?: CustomScenarioInput
  onSave: (scenario: CustomScenarioInput) => Promise<void>
  onCancel: () => void
  isEditing?: boolean
}

const defaultScenario: CustomScenarioInput = {
  name: '',
  description: '',
  auto_simulate_door: true,
  simulate_door_stuck: false,
  door_position: 0,
  door_state: 'CLOSED',
  auto_generate_pulses: true,
  simulate_frozen_line: false,
  flow_rate_gpm: 0.5,
  inject_door_fault: false,
  enable_override: false,
  override_hall_open: false,
  override_hall_close: false,
  override_door_fault: false,
  override_manual_switch: false
}

const ScenarioEditor: Component<ScenarioEditorProps> = (props) => {
  const [scenario, setScenario] = createSignal<CustomScenarioInput>(
    props.initialScenario || { ...defaultScenario }
  )
  const [saving, setSaving] = createSignal(false)
  const [error, setError] = createSignal('')

  const updateField = <K extends keyof CustomScenarioInput>(
    field: K,
    value: CustomScenarioInput[K]
  ) => {
    setScenario(prev => ({ ...prev, [field]: value }))
  }

  const handleSave = async () => {
    const s = scenario()
    if (!s.name.trim()) {
      setError('Scenario name is required')
      return
    }
    if (s.name.length > 31) {
      setError('Scenario name must be 31 characters or less')
      return
    }
    if (s.description.length > 127) {
      setError('Description must be 127 characters or less')
      return
    }

    setSaving(true)
    setError('')
    try {
      await props.onSave(s)
    } catch (err: any) {
      setError(err.message || 'Failed to save scenario')
    } finally {
      setSaving(false)
    }
  }

  return (
    <div class="card bg-base-200 shadow-xl">
      <div class="card-body">
        <h2 class="card-title text-primary">
          <svg class="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" 
              d="M11 5H6a2 2 0 00-2 2v11a2 2 0 002 2h11a2 2 0 002-2v-5m-1.414-9.414a2 2 0 112.828 2.828L11.828 15H9v-2.828l8.586-8.586z" />
          </svg>
          {props.isEditing ? 'Edit Scenario' : 'Create Custom Scenario'}
        </h2>

        <Show when={error()}>
          <div class="alert alert-error">
            <svg class="w-5 h-5" fill="currentColor" viewBox="0 0 20 20">
              <path fill-rule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z" clip-rule="evenodd" />
            </svg>
            <span>{error()}</span>
          </div>
        </Show>

        {/* Basic Info */}
        <div class="divider divider-primary">Basic Info</div>
        
        <div class="form-control">
          <label class="label">
            <span class="label-text font-semibold">Scenario Name *</span>
            <span class="label-text-alt">{scenario().name.length}/31</span>
          </label>
          <input
            type="text"
            class="input input-bordered"
            placeholder="My Custom Scenario"
            value={scenario().name}
            onInput={(e) => updateField('name', e.currentTarget.value)}
            maxlength={31}
          />
        </div>

        <div class="form-control">
          <label class="label">
            <span class="label-text font-semibold">Description</span>
            <span class="label-text-alt">{scenario().description.length}/127</span>
          </label>
          <textarea
            class="textarea textarea-bordered h-16"
            placeholder="Describe what this scenario tests..."
            value={scenario().description}
            onInput={(e) => updateField('description', e.currentTarget.value)}
            maxlength={127}
          />
        </div>

        {/* Door Settings */}
        <div class="divider divider-secondary">Door Settings</div>

        <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
          <div class="form-control">
            <label class="label cursor-pointer">
              <span class="label-text">Auto-simulate door movement</span>
              <input
                type="checkbox"
                class="toggle toggle-primary"
                checked={scenario().auto_simulate_door}
                onChange={(e) => updateField('auto_simulate_door', e.currentTarget.checked)}
              />
            </label>
          </div>

          <div class="form-control">
            <label class="label cursor-pointer">
              <span class="label-text">Simulate door stuck (no hall sensors)</span>
              <input
                type="checkbox"
                class="toggle toggle-warning"
                checked={scenario().simulate_door_stuck}
                onChange={(e) => updateField('simulate_door_stuck', e.currentTarget.checked)}
              />
            </label>
          </div>

          <div class="form-control">
            <label class="label cursor-pointer">
              <span class="label-text">Inject door fault signal</span>
              <input
                type="checkbox"
                class="toggle toggle-error"
                checked={scenario().inject_door_fault}
                onChange={(e) => updateField('inject_door_fault', e.currentTarget.checked)}
              />
            </label>
          </div>

          <div class="form-control">
            <label class="label">
              <span class="label-text">Initial door position: {scenario().door_position}%</span>
            </label>
            <input
              type="range"
              class="range range-primary"
              min="0"
              max="100"
              value={scenario().door_position}
              onInput={(e) => updateField('door_position', parseInt(e.currentTarget.value))}
            />
          </div>

          <div class="form-control">
            <label class="label">
              <span class="label-text">Initial door state</span>
            </label>
            <select
              class="select select-bordered"
              value={scenario().door_state}
              onChange={(e) => updateField('door_state', e.currentTarget.value)}
            >
              <option value="OPEN">Open</option>
              <option value="CLOSED">Closed</option>
              <option value="OPENING">Opening</option>
              <option value="CLOSING">Closing</option>
              <option value="STOPPED">Stopped</option>
              <option value="UNKNOWN">Unknown</option>
            </select>
          </div>
        </div>

        {/* Water Settings */}
        <div class="divider divider-accent">Water Settings</div>

        <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
          <div class="form-control">
            <label class="label cursor-pointer">
              <span class="label-text">Auto-generate water pulses</span>
              <input
                type="checkbox"
                class="toggle toggle-accent"
                checked={scenario().auto_generate_pulses}
                onChange={(e) => updateField('auto_generate_pulses', e.currentTarget.checked)}
              />
            </label>
          </div>

          <div class="form-control">
            <label class="label cursor-pointer">
              <span class="label-text">Simulate frozen line (no flow)</span>
              <input
                type="checkbox"
                class="toggle toggle-info"
                checked={scenario().simulate_frozen_line}
                onChange={(e) => updateField('simulate_frozen_line', e.currentTarget.checked)}
              />
            </label>
          </div>

          <div class="form-control">
            <label class="label">
              <span class="label-text">Flow rate: {scenario().flow_rate_gpm.toFixed(2)} GPM</span>
            </label>
            <input
              type="range"
              class="range range-accent"
              min="0"
              max="2"
              step="0.1"
              value={scenario().flow_rate_gpm}
              onInput={(e) => updateField('flow_rate_gpm', parseFloat(e.currentTarget.value))}
            />
          </div>
        </div>

        {/* Override Settings */}
        <div class="collapse collapse-arrow bg-base-300 rounded-box my-4">
          <input type="checkbox" class="peer" />
          <div class="collapse-title font-medium">
            Advanced Override Settings
          </div>
          <div class="collapse-content">
            <div class="form-control">
              <label class="label cursor-pointer">
                <span class="label-text">Enable override mode</span>
                <input
                  type="checkbox"
                  class="toggle toggle-secondary"
                  checked={scenario().enable_override}
                  onChange={(e) => updateField('enable_override', e.currentTarget.checked)}
                />
              </label>
            </div>

            <Show when={scenario().enable_override}>
              <div class="grid grid-cols-2 gap-2 mt-2">
                <div class="form-control">
                  <label class="label cursor-pointer">
                    <span class="label-text text-sm">Hall Open</span>
                    <input
                      type="checkbox"
                      class="checkbox checkbox-sm"
                      checked={scenario().override_hall_open}
                      onChange={(e) => updateField('override_hall_open', e.currentTarget.checked)}
                    />
                  </label>
                </div>
                <div class="form-control">
                  <label class="label cursor-pointer">
                    <span class="label-text text-sm">Hall Close</span>
                    <input
                      type="checkbox"
                      class="checkbox checkbox-sm"
                      checked={scenario().override_hall_close}
                      onChange={(e) => updateField('override_hall_close', e.currentTarget.checked)}
                    />
                  </label>
                </div>
                <div class="form-control">
                  <label class="label cursor-pointer">
                    <span class="label-text text-sm">Door Fault</span>
                    <input
                      type="checkbox"
                      class="checkbox checkbox-sm"
                      checked={scenario().override_door_fault}
                      onChange={(e) => updateField('override_door_fault', e.currentTarget.checked)}
                    />
                  </label>
                </div>
                <div class="form-control">
                  <label class="label cursor-pointer">
                    <span class="label-text text-sm">Manual Switch</span>
                    <input
                      type="checkbox"
                      class="checkbox checkbox-sm"
                      checked={scenario().override_manual_switch}
                      onChange={(e) => updateField('override_manual_switch', e.currentTarget.checked)}
                    />
                  </label>
                </div>
              </div>
            </Show>
          </div>
        </div>

        {/* Actions */}
        <div class="card-actions justify-end mt-4">
          <button
            class="btn btn-ghost"
            onClick={props.onCancel}
            disabled={saving()}
          >
            Cancel
          </button>
          <button
            class="btn btn-primary"
            onClick={handleSave}
            disabled={saving() || !scenario().name.trim()}
          >
            <Show when={saving()} fallback={
              <>
                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7" />
                </svg>
                {props.isEditing ? 'Update Scenario' : 'Create Scenario'}
              </>
            }>
              <span class="loading loading-spinner loading-sm"></span>
              Saving...
            </Show>
          </button>
        </div>
      </div>
    </div>
  )
}

export default ScenarioEditor
