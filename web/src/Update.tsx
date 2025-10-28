

import { createSignal, onMount } from 'solid-js';

interface VersionInfo {
  firmware_version: string;
  chip_family: string;
  build_date: string;
  build_time: string;
}

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
              <p>Build Date: {versionInfo()!.build_date}</p>
              <p>Build Time: {versionInfo()!.build_time}</p>
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