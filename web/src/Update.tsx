

import { createSignal, onMount, Show } from 'solid-js';

interface VersionInfo {
  firmware_version: string;
  chip_family: string;
  build_date: string;
  build_time: string;
  github_repo?: string;
  git_commit_sha?: string;
}

// Build-time constants injected by Vite
declare const __BUILD_DATE__: string;
declare const __BUILD_TIME__: string;

function Update() {
  const [loading, setLoading] = createSignal(true);
  const [versionInfo, setVersionInfo] = createSignal<VersionInfo | null>(null);

  onMount(() => {
    setLoading(true);
    fetch('/version')
      .then((response) => response.json())
      .then((data) => {
        setVersionInfo(data);
        setLoading(false);
      });
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

  return (
    <div class="card" >
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
          <iframe class="w-full h-160" src="/update" title="Firmware OTA Update"></iframe>
        </div>
      )
    }
    </div>
  );
}

export default Update
