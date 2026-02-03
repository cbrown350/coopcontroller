import { A, useLocation } from '@solidjs/router'
import { ParentProps } from 'solid-js'

function App(props: ParentProps) {
  const location = useLocation()

  const isActive = (path: string) => {
    const pathname = location.pathname
    if (path === '/' || path === '/status') {
      return pathname === '/' || pathname === '/status'
    }
    return pathname === path
  }

  return (
    <div class="flex flex-col items-center min-h-screen pt-6 bg-base-200">
      <h1 class="text-xl font-bold w-full max-w-5xl pl-1 pb-4 flex items-center gap-2">
        <span class="text-warning">HW</span> Emulator
        <span class="badge badge-outline badge-sm">ESP32</span>
      </h1>
      <div class="tabs tabs-lift w-full max-w-5xl">

        <A href="/status" class={`tab ${isActive('/status') ? 'tab-active' : ''}`}>
          Status
        </A>

        <A href="/door" class={`tab ${isActive('/door') ? 'tab-active' : ''}`}>
          Door
        </A>

        <A href="/water" class={`tab ${isActive('/water') ? 'tab-active' : ''}`}>
          Water
        </A>

        <A href="/settings" class={`tab ${isActive('/settings') ? 'tab-active' : ''}`}>
          Settings
        </A>

      </div>

      <div class="w-full max-w-5xl bg-base-100 border-base-300 p-6 pb-12">
        {props.children}
      </div>

      <footer class="footer footer-center p-4 text-base-content/60">
        <div>
          Hardware Emulator for Coop Controller
        </div>
      </footer>
    </div>
  )
}

export default App
