

import { createSignal, onMount, onCleanup, Show } from 'solid-js';
import { authenticatedFetch } from './utils/api';

interface VersionInfo {
  firmware_version: string;
  chip_family: string;
  build_date: string;
  build_time: string;
  github_repo?: string;
  git_commit_sha?: string;
}

interface UpdateCheckResult {
  update_available: boolean;
  current_version: string;
  available_version: string;
  manifest_url: string;
  last_check_time: number;
  firmware: { version: string; url: string; size_bytes: number };
  filesystem: { version: string; url: string; size_bytes: number };
  release_date?: string;
  github_repo?: string;
}

interface UpdateStatus {
  status: 'idle' | 'checking' | 'available' | 'current' | 'downloading' | 'verifying' | 'installing' | 'complete' | 'error';
  progress: number;
  phase: string;
  last_check: number;
  error: string;
}

// Build-time constants injected by Vite
declare const __BUILD_DATE__: string;
declare const __BUILD_TIME__: string;

function Update() {
  const [loading, setLoading] = createSignal(true);
  const [versionInfo, setVersionInfo] = createSignal<VersionInfo | null>(null);
  const [checking, setChecking] = createSignal(false);
  const [checkResult, setCheckResult] = createSignal<UpdateCheckResult | null>(null);
  const [checkError, setCheckError] = createSignal('');
  const [installing, setInstalling] = createSignal(false);
  const [installError, setInstallError] = createSignal('');
  const [updateStatus, setUpdateStatus] = createSignal<UpdateStatus | null>(null);
  const [skipFilesystem, setSkipFilesystem] = createSignal(false);
  const [showInstallConfirm, setShowInstallConfirm] = createSignal(false);
  const [forceUpdate, setForceUpdate] = createSignal(false);
  const [deviceRestarting, setDeviceRestarting] = createSignal(false);
  let statusInterval: number | undefined;
  let pollFailCount = 0;

  onMount(() => {
    setLoading(true);
    fetch('/version')
      .then((response) => response.json())
      .then((data) => {
        setVersionInfo(data);
        setLoading(false);
      });
  });

  onCleanup(() => {
    if (statusInterval) clearInterval(statusInterval);
  });

  const commitUrl = () => {
    const info = versionInfo();
    if (info?.github_repo && info?.git_commit_sha) {
      return `https://github.com/${info.github_repo}/commit/${info.git_commit_sha}`;
    }
    return null;
  };

  const shortSha = () => {
    const sha = versionInfo()?.git_commit_sha;
    return sha ? sha.substring(0, 7) : null;
  };

  const handleCheckForUpdates = async () => {
    setChecking(true);
    setCheckError('');
    setCheckResult(null);
    try {
      const response = await fetch('/update/check');
      if (!response.ok) {
        throw new Error(`Check failed: ${response.status} ${response.statusText}`);
      }
      const data: UpdateCheckResult = await response.json();
      setCheckResult(data);
    } catch (err: any) {
      setCheckError(err.message || 'Failed to check for updates');
    } finally {
      setChecking(false);
    }
  };

  const startStatusPolling = () => {
    if (statusInterval) clearInterval(statusInterval);
    pollFailCount = 0;
    setDeviceRestarting(false);
    statusInterval = window.setInterval(async () => {
      try {
        const response = await fetch('/update/status');
        if (response.ok) {
          pollFailCount = 0;
          setDeviceRestarting(false);
          const status: UpdateStatus = await response.json();
          setUpdateStatus(status);
          if (status.status === 'complete' || status.status === 'error' || status.status === 'idle') {
            clearInterval(statusInterval);
            statusInterval = undefined;
            setInstalling(false);
            if (status.status === 'error' && status.error) {
              setInstallError(status.error);
            }
          }
        }
      } catch {
        pollFailCount++;
        if (pollFailCount >= 2) {
          setDeviceRestarting(true);
          setUpdateStatus({ status: 'installing', progress: 100, phase: 'restarting', last_check: 0, error: '' });
        }
        if (pollFailCount >= 8) {
          // Device should have rebooted by now, try reloading
          clearInterval(statusInterval);
          statusInterval = undefined;
          window.location.reload();
        }
      }
    }, 2000);
  };

  const handleInstallUpdate = async () => {
    setShowInstallConfirm(false);
    setInstalling(true);
    setInstallError('');
    setUpdateStatus(null);
    try {
      const response = await authenticatedFetch('/update/install', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ skip_filesystem: skipFilesystem(), force: forceUpdate() }),
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({ message: response.statusText }));
        throw new Error(data.message || `Install failed: ${response.status}`);
      }
      startStatusPolling();
    } catch (err: any) {
      setInstallError(err.message || 'Failed to start update');
      setInstalling(false);
    }
  };

  const isActive = () => {
    const s = updateStatus()?.status;
    return s === 'downloading' || s === 'verifying' || s === 'installing' || s === 'checking';
  };

  const formatReleaseDate = (isoDate: string) => {
    const d = new Date(isoDate);
    if (isNaN(d.getTime())) return isoDate;
    const months = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
    const pad = (n: number) => n.toString().padStart(2, '0');
    return `${months[d.getUTCMonth()]} ${pad(d.getUTCDate())} ${d.getUTCFullYear()} ${pad(d.getUTCHours())}:${pad(d.getUTCMinutes())}:${pad(d.getUTCSeconds())} UTC`;
  };

  const formatBytes = (bytes: number) => {
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  };

  return (
    <div class="card w-full max-w-full min-w-0">
      {loading() ? (
        <p>Loading version info... <span class="loading loading-spinner loading-xl"></span></p>
      ) : (
        <div>
          {versionInfo() && (
            <div>
              <p>Firmware Version: {versionInfo()!.firmware_version}</p>
              <p>Chip Family: {versionInfo()!.chip_family}</p>
              <p>FW Build: {versionInfo()!.build_date} {versionInfo()!.build_time}</p>
              <p>UI/FS Build: {__BUILD_DATE__} {__BUILD_TIME__}</p>
              <Show when={shortSha()}>
                <p>
                  Git Commit:{' '}
                  <Show when={commitUrl()} fallback={<span class="font-mono">{shortSha()}</span>}>
                    <a
                      href={commitUrl()!}
                      target="_blank"
                      rel="noopener noreferrer"
                      class="link link-primary font-mono"
                    >
                      {shortSha()}
                    </a>
                  </Show>
                </p>
              </Show>
            </div>
          )}

          <div class="divider"></div>

          <h2 class="text-lg font-bold mb-4">OTA Update</h2>

          <div class="flex flex-wrap gap-2 items-center mb-4">
            <button
              class="btn btn-primary"
              onClick={handleCheckForUpdates}
              disabled={checking() || installing()}
            >
              {checking() ? (
                <>
                  <span class="loading loading-spinner loading-xs"></span>
                  Checking...
                </>
              ) : (
                'Check for Updates'
              )}
            </button>
          </div>

          <Show when={checkError()}>
            <div role="alert" class="alert alert-error mb-4">{checkError()}</div>
          </Show>

          <Show when={checkResult()}>
            <div class="card bg-base-200 card-sm shadow-sm mb-4">
              <div class="card-body">
                <p>Current Version: <span class="font-mono">{checkResult()!.current_version}</span></p>
                <p>Available Version:{' '}
                  <Show when={checkResult()?.github_repo || versionInfo()?.github_repo} fallback={<span class="font-mono">{checkResult()!.available_version}</span>}>
                    <a
                      href={`https://github.com/${(checkResult()?.github_repo || versionInfo()?.github_repo)}/releases/tag/v${checkResult()!.available_version}`}
                      target="_blank"
                      rel="noopener noreferrer"
                      class="link link-primary font-mono"
                    >
                      {checkResult()!.available_version}
                    </a>
                  </Show>
                </p>
                <Show when={checkResult()!.release_date}>
                  <p>Release Date: <span class="font-mono">{formatReleaseDate(checkResult()!.release_date!)}</span>
                    <Show when={checkResult()?.github_repo || versionInfo()?.github_repo}>
                      {' '}<a
                        href={`https://github.com/${(checkResult()?.github_repo || versionInfo()?.github_repo)}/releases/tag/v${checkResult()!.available_version}`}
                        target="_blank"
                        rel="noopener noreferrer"
                        class="link link-primary text-sm"
                      >
                        (View Release)
                      </a>
                    </Show>
                  </p>
                </Show>
                <Show when={checkResult()!.update_available}>
                  <div role="alert" class="alert alert-info mt-2">
                    A new update is available!
                    <Show when={checkResult()!.firmware}>
                      <span> Firmware: {formatBytes(checkResult()!.firmware.size_bytes)}</span>
                    </Show>
                    <Show when={checkResult()!.filesystem}>
                      <span> | Filesystem: {formatBytes(checkResult()!.filesystem.size_bytes)}</span>
                    </Show>
                  </div>
                  <div class="form-control mt-2">
                    <label class="label cursor-pointer">
                      <span class="label-text">Skip filesystem update (firmware only)</span>
                      <input
                        type="checkbox"
                        class="toggle"
                        checked={skipFilesystem()}
                        onChange={(e) => setSkipFilesystem(e.currentTarget.checked)}
                      />
                    </label>
                  </div>
                  <button
                    class="btn btn-accent mt-2"
                    onClick={() => { setForceUpdate(false); setShowInstallConfirm(true); }}
                    disabled={installing()}
                  >
                    Install Update
                  </button>
                </Show>
                <Show when={!checkResult()!.update_available}>
                  <div role="alert" class="alert alert-success mt-2">Firmware is up to date.</div>
                  <details class="collapse collapse-arrow bg-base-300 mt-2">
                    <summary class="collapse-title text-sm font-medium">Force Reinstall</summary>
                    <div class="collapse-content">
                      <p class="text-sm opacity-70 mb-2">
                        Reinstall the latest manifest version even though it's not recognized as newer.
                        Useful for recovering from a bad flash or reinstalling the same version.
                      </p>
                      <div class="form-control mb-2">
                        <label class="label cursor-pointer">
                          <span class="label-text">Skip filesystem update (firmware only)</span>
                          <input
                            type="checkbox"
                            class="toggle"
                            checked={skipFilesystem()}
                            onChange={(e) => setSkipFilesystem(e.currentTarget.checked)}
                          />
                        </label>
                      </div>
                      <button
                        class="btn btn-warning btn-sm"
                        onClick={() => { setForceUpdate(true); setShowInstallConfirm(true); }}
                        disabled={installing()}
                      >
                        Force Reinstall
                      </button>
                    </div>
                  </details>
                </Show>
              </div>
            </div>
          </Show>

          <Show when={installError()}>
            <div role="alert" class="alert alert-error mb-4">{installError()}</div>
          </Show>

          <Show when={installing() || (updateStatus() && isActive())}>
            <div class="card bg-base-200 card-sm shadow-sm mb-4">
              <div class="card-body">
                <h3 class="font-bold">Update Progress</h3>
                <p>
                  Status: {updateStatus()?.status ?? 'starting...'}
                  <Show when={updateStatus()?.phase}>
                    {' '}({updateStatus()!.phase})
                  </Show>
                </p>
                <progress class="progress progress-primary w-full" value={updateStatus()?.progress ?? 0} max="100"></progress>
                <p>{updateStatus()?.progress ?? 0}%</p>
              </div>
            </div>
          </Show>

          <Show when={deviceRestarting()}>
            <div role="alert" class="alert alert-info mb-4">
              <span class="loading loading-spinner loading-sm"></span>
              Update installed. Device is restarting... This page will reload automatically.
            </div>
          </Show>

          <Show when={updateStatus()?.status === 'complete' && !deviceRestarting()}>
            <div role="alert" class="alert alert-success mb-4">
              Update complete! The device will reboot shortly.
            </div>
          </Show>

          <Show when={updateStatus()?.status === 'error'}>
            <div role="alert" class="alert alert-error mb-4">
              Update failed: {updateStatus()?.error}
            </div>
          </Show>

          {/* Install Confirmation Dialog */}
          <Show when={showInstallConfirm()}>
            <div class="modal modal-open">
              <div class="modal-box">
                <h3 class="font-bold text-lg">
                  {forceUpdate() ? 'Confirm Force Reinstall' : 'Confirm Update Installation'}
                </h3>
                <p class="py-4">
                  {forceUpdate()
                    ? 'This will force reinstall the latest manifest version even though it matches your current version.'
                    : 'This will download and install the update.'
                  }
                  {' The device will reboot after installation.'}
                  {skipFilesystem() ? ' Only firmware will be updated.' : ' Both firmware and filesystem will be updated.'}
                </p>
                <div class="modal-action">
                  <button class="btn" onClick={() => setShowInstallConfirm(false)}>Cancel</button>
                  <button class="btn btn-accent" onClick={handleInstallUpdate}>
                    Yes, Install Update
                  </button>
                </div>
              </div>
            </div>
          </Show>

          <div class="divider"></div>

          <details class="collapse collapse-arrow bg-base-200">
            <summary class="collapse-title font-medium">Advanced: ElegantOTA (Manual Upload)</summary>
            <div class="collapse-content">
              <iframe class="w-full h-160" src="/update" title="Firmware OTA Update"></iframe>
            </div>
          </details>
        </div>
      )}
    </div>
  );
}

export default Update
