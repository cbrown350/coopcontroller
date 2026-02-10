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
    <div class="flex flex-col items-center min-h-screen pt-4 sm:pt-10 bg-base-200 overflow-x-hidden w-full max-w-full">
      <div class="flex items-center gap-3 w-full max-w-5xl px-2 sm:pl-1 pb-4">
        <img src="/logo.webp" alt="Coop Controller" class="w-8 h-8 sm:w-10 sm:h-10 rounded" />
        <h1 class="text-xl font-bold text-primary">Coop Controller</h1>
      </div>
      <div class="tabs tabs-lift w-full max-w-5xl text-xs sm:text-sm">

        <A href="/status" class={`tab ${isActive('/status') ? 'tab-active' : ''}`}>
          Status
        </A>

        <A href="/settings" class={`tab ${isActive('/settings') ? 'tab-active' : ''}`}>
          Settings
        </A>

        <A href="/history" class={`tab ${isActive('/history') ? 'tab-active' : ''}`}>
          History
        </A>

        <A href="/log" class={`tab ${isActive('/log') ? 'tab-active' : ''}`}>
          Logs
        </A>

        <A href="/updates" class={`tab ${isActive('/updates') ? 'tab-active' : ''}`}>
          Update
        </A>

        <A href="/about" class={`tab ${isActive('/about') ? 'tab-active' : ''}`}>
          About
        </A>

      </div>

      <div class="w-full max-w-5xl bg-base-100 border-base-300 px-2 py-4 sm:p-6">
        {props.children}
      </div>
    </div>
  )
}

export default App
