import { createSignal, onMount, onCleanup, Show } from 'solid-js'



const PRINT_STATUS_MAP = {
  0: 'Idle',
  1: 'Homing',
  2: 'Dropping',
  3: 'Exposing',
  4: 'Lifting',
  5: 'Pausing',
  6: 'Paused',
  7: 'Stopping',
  8: 'Stopped',
  9: 'Complete',
  10: 'File Checking',
  13: 'Printing',
  15: 'Unknown: 15',
  16: 'Heating',
  18: 'Unknown: 18',
  19: 'Unknown: 19',
  20: 'Bed Leveling',
  21: 'Unknown: 21',
}

function Status() {

  const [loading, setLoading] = createSignal(true)
  const [sensorStatus, setSensorStatus] = createSignal({
    stopped: false,
    filamentRunout: false,
    elegoo: {
      lastStatusUpdateTimestamp: 0,
      mainboardID: '',
      printStatus: 0,
      isPrinting: false,
      currentLayer: 0,
      totalLayer: 0,
      progress: 0,
      currentTicks: 0,
      totalTicks: 0,
      PrintSpeedPct: 0,
      isWebsocketConnected: false,
      nozzleTempC: null,
      nozzleTargetTempC: null,
      bedTempC: null,
      bedTargetTempC: null,
      chamberTempC: null,
      chamberTargetTempC: null
    },
    heater: {
      chamberTempC: null,
      chamberTargetTempC: null,
      timerActive: false,
      remainingTime: null,
      isAuto: false
    }
  })

  const refreshSensorStatus = async () => {
    const response = await fetch('/sensor_status')
    const data = await response.json()
    setSensorStatus(data)
    setLoading(false)
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
          <div class="stats w-full shadow bg-base-200">
            <div class="stat">
              <div class="stat-title">Printer Connected</div>
              <div class={`stat-value ${sensorStatus().elegoo.isWebsocketConnected ? 'text-success' : 'text-error'}`}> {sensorStatus().elegoo.isWebsocketConnected ? 'Yes' : 'No'}
              </div>
                <Show when={sensorStatus().elegoo.isWebsocketConnected}>
                  <span class="stat-title">Last {new Date(sensorStatus().elegoo.lastStatusUpdateTimestamp * 1000).toLocaleDateString()} {new Date(sensorStatus().elegoo.lastStatusUpdateTimestamp * 1000).toLocaleTimeString()}</span>
                </Show>
            </div>
            {sensorStatus().elegoo.isWebsocketConnected && <>
              <div class="stat">
                <div class="stat-title">Filament Stopped</div>
                <div class={`stat-value ${sensorStatus().stopped ? 'text-error' : 'text-success'}`}> {sensorStatus().stopped ? 'Yes' : 'No'}</div>
              </div>
              <div class="stat">
                <div class="stat-title">Filament Runout</div>
                <div class={`stat-value ${sensorStatus().filamentRunout ? 'text-error' : 'text-success'}`}> {sensorStatus().filamentRunout ? 'Yes' : 'No'}</div>
              </div>
            </>     
            }
          </div>
          
          <div class="card w-full mt-8 bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title">Temperatures</h2>
              <div class="text-sm flex gap-4 flex-wrap">
                <div>
                  <h3 class="font-bold">Nozzle</h3>
                  Set <Show when={sensorStatus().elegoo.nozzleTargetTempC} fallback={<span>--°C</span>}>
                    <span>{sensorStatus().elegoo.nozzleTargetTempC}°C</span>
                  </Show>/Current <Show when={sensorStatus().elegoo.nozzleTempC} fallback={<span>--°C</span>}>
                    <span>{sensorStatus().elegoo.nozzleTempC}°C</span>
                  </Show>
                </div>
                <div>
                  <h3 class="font-bold">Bed</h3>
                  Set <Show when={sensorStatus().elegoo.bedTargetTempC} fallback={<span>--°C</span>}>
                    <span>{sensorStatus().elegoo.bedTargetTempC}°C</span>
                  </Show>/Current <Show when={sensorStatus().elegoo.bedTempC} fallback={<span>--°C</span>}>
                    <span>{sensorStatus().elegoo.bedTempC}°C</span>
                  </Show>
                </div>
                <div>
                  <h3 class="font-bold">Chamber</h3>
                  Set <Show when={sensorStatus().elegoo.chamberTargetTempC} fallback={<span>--°C</span>}>
                    <span>{sensorStatus().elegoo.chamberTargetTempC}°C</span>
                  </Show>/Current <Show when={sensorStatus().elegoo.chamberTempC} fallback={<span>--°C</span>}>
                    <span>{sensorStatus().elegoo.chamberTempC}°C</span>
                  </Show>
                </div>
                <div>
                  <h3 class="font-bold">Chamber Heater</h3>
                  <div>
                    Set <Show when={sensorStatus().heater.chamberTargetTempC} fallback={<span>--°C</span>}>
                      <span>{sensorStatus().heater.chamberTargetTempC}°C</span>
                    </Show>/Current <Show when={sensorStatus().heater.chamberTempC} fallback={<span>--°C</span>}>
                      <span>{sensorStatus().heater.chamberTempC}°C </span>
                    </Show> Timer: <Show when={sensorStatus().heater.timerActive} fallback={<span class="stat-value text-error text-sm">Off</span>}>
                      <span class="stat-value text-success text-sm">{sensorStatus()?.heater?.remainingTime ? 
                        Math.floor(sensorStatus()?.heater?.remainingTime! / 3600) + 'h ' + 
                        Math.floor((sensorStatus()?.heater?.remainingTime! % 3600) / 60) + 'm ' + 
                        Math.floor(sensorStatus()?.heater?.remainingTime! % 60) + 's'
                        : '--:--:--'}</span>
                    </Show>
                    <Show when={sensorStatus().heater.isAuto}>
                      <span class="stat-value text-success text-sm">Auto</span>
                    </Show>
                  </div>
                </div>
              </div>
            </div>
          </div>
          <div class="card w-full mt-8 bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title">More Information</h2>
              <div class="text-sm flex gap-4 flex-wrap">
                <div>
                  <h3 class="font-bold">Mainboard ID</h3>
                  <p>{sensorStatus().elegoo.mainboardID}</p>
                </div>
                <div>
                  <h3 class="font-bold">Currently Printing</h3>
                  <p>{sensorStatus().elegoo.isPrinting ? 'Yes' : 'No'}</p>
                </div>
                <div>
                  <h3 class="font-bold">Print Status</h3>
                  <p>{PRINT_STATUS_MAP[sensorStatus().elegoo.printStatus as keyof typeof PRINT_STATUS_MAP]}</p>
                </div>

                <div>
                  <h3 class="font-bold">Current Layer</h3>
                  <p>{sensorStatus().elegoo.currentLayer}</p>
                </div>
                <div>
                  <h3 class="font-bold">Total Layer</h3>
                  <p>{sensorStatus().elegoo.totalLayer}</p>
                </div>
                <div>
                  <h3 class="font-bold">Progress</h3>
                  <p>{sensorStatus().elegoo.progress}</p>
                </div>
                <div>
                  <h3 class="font-bold">Current Ticks</h3>
                  <p>{sensorStatus().elegoo.currentTicks}</p>
                </div>
                <div>
                  <h3 class="font-bold">Total Ticks</h3>
                  <p>{sensorStatus().elegoo.totalTicks}</p>
                </div>
                <div>
                  <h3 class="font-bold">Print Speed</h3>
                  <p>{sensorStatus().elegoo.PrintSpeedPct}</p>
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