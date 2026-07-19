# LLM Weather-Decider Design (Issue #6)

**Status:** Autonomous design — user away, execution pre-authorized. All decisions
below are flagged for confirmation on return; see "Ambiguous Decisions" at the end.

## Goal

Issue #6: add an LLM-backed door-open weather decision (pluggable provider —
Ollama Cloud/local, or any OpenAI-compatible endpoint), a connection-test
button for it, and a test button for the existing OpenWeatherMap weather
check. The LLM must reason about the door's *actual open-window* for the day
(sunrise+offset through sunset/close+offset), not flag on any bad weather
anywhere in the raw forecast. Falls back to the existing rule-based decider
when disabled or on any failure.

## Existing architecture (recap, not new)

`WeatherManager` (`lib/WeatherManager/`) already fetches OpenWeatherMap data on
a timer and delegates the open/no-open call to a pluggable `IWeatherDecider`
(`RuleBasedWeatherDecider` ships today). The decision is cached and recomputed
**once per successful fetch** (default every 10 min), never per loop — so a
network-calling decider is bounded. `DoorController::checkAutoOpenSchedule()`
consults `weatherManager->isWeatherGoodForOpening()` only when
`isWithinOpenWindow()` is already true, and holds the door with an hourly
recheck otherwise. None of this changes — the LLM decider is a second
`IWeatherDecider` implementation plugged in via the existing `setDecider()`.

## New pieces

### 1. `LlmWeatherDecider` (new: `lib/WeatherManager/LlmWeatherDecider.h/.cpp`)

Implements `IWeatherDecider`. Configured with: base URL, API key (bearer,
optional), model name, provider wire-format, timeout. On `decide()`:

1. Builds a prompt from `WeatherDecisionInput` (current conditions + forecast
   blocks) **plus the door's today open-window** (see below) — explicitly
   telling the model "the door will be open from HH:MM to HH:MM today; judge
   conditions for that window, not the full forecast."
2. POSTs to the provider, requesting a JSON response
   (`{"open": bool, "reason": string}`), using `response_format:
   {"type":"json_object"}` where the wire format supports it.
3. Parses the reply. On HTTP failure, timeout, or unparseable/missing `open`
   field: logs a warning and **falls back to `RuleBasedWeatherDecider`'s
   decision for that cycle** (computed inline), not a bare `open=true`. This
   keeps the existing "never trap the chickens" guarantee while being more
   informative than an unconditional true when the model is reachable but
   confused.
4. `name()` returns `"llm"`.

### 2. Door open-window plumbed into the decision

`WeatherDecisionInput` gains two fields: `int window_open_minutes` and
`int window_close_minutes` (minutes since midnight, local time; -1 = unknown).
`WeatherManager` gains `setOpenWindowMinutes(int openMin, int closeMin)`,
called from `DoorController` each time it recomputes its schedule (cheap int
assignment, no new dependency — `WeatherManager` still doesn't know about
`DoorController` types, avoiding a circular library dependency since
`DoorController` already depends on `WeatherManager`, not the reverse).
`RuleBasedWeatherDecider` ignores the new fields (unchanged behavior);
`LlmWeatherDecider` uses them to scope the prompt and to select which forecast
blocks fall inside the window when composing the prompt text.

### 3. HAL gap: plain-HTTP POST support

`IHAL::httpPost` (and `httpGet`) always use `WiFiClientSecure`, i.e. TLS-only.
A LAN Rapid-MLX/Ollama endpoint (`http://192.168.x.x:11434` or
`http://localhost:8000`) is plain HTTP and the TLS handshake would fail. Fix:
extend `HAL_ESP32::httpPost` (and add a lightweight
`httpPostAuth(url, jsonBody, bearerToken, timeout_ms)` overload used by the LLM
decider and its test endpoint) to detect `http://` vs `https://` and use a
plain `WiFiClient` for the former, `WiFiClientSecure` for the latter — mirrors
the scheme-detection already present in `httpGet`/`httpPost` for port
selection. `IHAL` gets the new virtual method; `MockHAL` gets a matching mock
(mirrors the existing `httpPost` mock: records last URL/body/token, returns a
configurable canned response).

### 4. Settings (`lib/SettingsManager/`)

New fields on `user_settings`:
```
bool     llm_enabled = false;
String   llm_provider_type = "openai_compatible"; // "openai_compatible" | "ollama_native" | "ollama_cloud"
String   llm_base_url = "";
String   llm_api_key = "";           // bearer token; empty = no auth (LAN Ollama)
String   llm_model = "";
unsigned int llm_timeout_seconds = 15; // clamp 5-60
```
`llm_api_key` follows the existing secret-field pattern (`api_password`,
`telegram_bot_token`): only serialized in `toJson()` when `includePassword`.
Matching getters/setters added to `SettingsManager`.

`llm_provider_type` picks the wire format:
- `openai_compatible` / `ollama_cloud` → `POST {base_url}/v1/chat/completions`
  (OpenAI schema). Both Ollama Cloud and Rapid-MLX speak this; `ollama_cloud`
  is a distinct label only for clearer UI/help text (defaults/placeholder
  differ), not a different code path.
- `ollama_native` → `POST {base_url}/api/chat` (native Ollama schema), for
  users who point at a local Ollama install without its OpenAI-compat layer.

### 5. Web server endpoints (`lib/CoopControllerWebServer/`)

- `POST /weather/llm/test_connection` — mirrors the existing
  `/notifications/test/telegram` pattern: accepts an optional JSON body with
  unsaved provider config (base_url/api_key/model/provider_type), sends a
  minimal cheap prompt ("reply with the single word OK"), returns
  `{success, error, model_reply}`. Uses a short timeout (8s) independent of
  the configured `llm_timeout_seconds` so the UI button doesn't hang.
- `POST /weather/test` — new, satisfies "add a test button to weather" (the
  OpenWeatherMap side): forces one fetch cycle (optionally with an unsaved API
  key override, same override pattern), returns `{success, error, current}`
  snapshot JSON. Does not touch the door or decider state beyond the normal
  fetch/decide cycle.

### 6. Web UI (`web/src/Settings.tsx`, `web/src/types.ts`)

Weather card gets a "Test Connection" button next to the OpenWeatherMap key
field (posts to `/weather/test` with current form values, shows
success/error inline — same UX as the Telegram/Email test buttons).

New "LLM Weather Decision" card below it, shown only when weather is enabled:
- Enable toggle (`llm_enabled`)
- Provider type select: "Ollama Cloud", "Ollama (local/LAN)", "OpenAI-compatible (other)"
- Base URL + port text input (placeholder swaps per provider type)
- API key field with show/hide toggle (same join/button pattern as the
  OpenWeatherMap key field), optional for LAN Ollama
- Model name text input
- Timeout (seconds) number input
- "Test Connection" button → `/weather/llm/test_connection`, shows
  success + model reply, or the error, inline
- Help text: "When enabled, the LLM is asked to judge the forecast for the
  door's actual open period today, not the whole forecast. Falls back
  automatically to the rule-based check if the LLM is unreachable or
  disabled."

`types.ts` gains the new settings fields and a `WeatherLlmTestResult` /
`WeatherTestResult` response shape.

## Testing plan

- Desktop unit tests (MockHAL) for `LlmWeatherDecider`: valid JSON reply →
  correct open/reason; malformed JSON → falls back to rule-based; HTTP
  failure → falls back to rule-based; both wire formats (OpenAI-schema POST
  body, native Ollama POST body) construct the expected request; prompt
  contains the open-window text when window minutes are set.
- Desktop unit tests for the new `HAL_ESP32`/`MockHAL` `httpPostAuth` plain-vs-TLS
  scheme selection (via MockHAL recording, since real socket behavior can't be
  unit tested desktop-side — this only verifies the call is made with the
  right recorded URL/token; the scheme-selection logic itself is a small,
  visually-inspectable diff in `HAL_ESP32.cpp` mirroring the existing
  `httpGet` pattern).
- `WeatherManager` tests extended for `setOpenWindowMinutes` plumbing.
- `SettingsManager` tests for new getters/setters + JSON round-trip +
  includePassword gating on `llm_api_key`.
- `CoopControllerWebServer` tests for the two new endpoints (success, error,
  auth-required-when-enabled paths, matching existing test conventions).
- Full USB hardware test: configure the LLM provider against the local
  Rapid-MLX instance (`http://localhost:8000`, `openai_compatible`, model
  `mlx-community/Qwen3.6-35B-A3B-6bit`, API key `kO7dkihVsUVeb`) as a stand-in
  for Ollama; verify test-connection button; verify weather test button
  against OpenWeatherMap (key `11d15648284691c311acf981fef2122d`); verify the
  door's weather gate consults the LLM decider end-to-end and that disabling
  it / killing the local server falls back cleanly to rule-based.

## Also in scope: pre-existing CI break (task #7, unrelated to #6 but blocking)

CI's `pio test -e unit-test-desktop` step (Ubuntu/GCC 13) has been failing
since commit `6dc3cec` added `<mutex>` to `Logger.h`: ArduinoFake's `Arduino.h`
`#define round(x)` / `#define abs(x)` macros collide with `std::chrono`
templates pulled in transitively by `<mutex>` on libstdc++. Apple clang
(local dev machine) tolerates it; GCC 13 does not. Fix: an include-order shim
in the desktop test mocks that `#undef`s `round`/`abs` immediately after
`Arduino.h` is included, restoring them only where `ArduinoFake`'s own sources
need them. Verified locally; GCC-13 safety confirmed by inspection (the
`#undef` removes the exact macros GCC's chrono header trips on) since CI can't
be run from this machine directly — will push and confirm green after.

## Ambiguous Decisions (flag for user review on return)

1. **Single active LLM provider config, not multiple saved profiles.** Issue
   text says "expandable to add different providers in the future" — read as
   expandable in *code* (new provider_type values), not a multi-profile UI.
2. **Two wire formats, three UI labels.** Ollama Cloud and "OpenAI-compatible
   (other)" both hit `/v1/chat/completions`; only true "Ollama native" uses
   `/api/chat`. If you actually want Ollama Cloud to use native `/api/chat`
   instead, that's a one-line change to the provider-type mapping.
3. **HAL now supports plain HTTP, not just HTTPS**, specifically so LAN
   Ollama/Rapid-MLX work. This is a real (if small) change to `HAL_ESP32`'s
   networking helpers, not just new code — flagging since it touches shared
   infrastructure (`httpPost`/`IHAL`).
4. **Fallback-on-error uses the rule-based decision, not a bare `open=true`.**
   The original `IWeatherDecider` doc comment says fall back to `open=true`;
   I judged the rule-based result is a strictly better fallback (still safe,
   more informative) and used that instead. Easy to revert to bare `true` if
   you'd rather keep the simpler contract.
5. **Door open-window passed as plain minutes-since-midnight ints**, recomputed
   by `DoorController` and pushed into `WeatherManager` via a setter, to avoid
   a circular dependency between the two libraries.
6. **Test-connection timeout is a fixed 8s**, independent of the
   user-configured `llm_timeout_seconds`, so the button can't hang the UI even
   if the configured production timeout is long.
7. **JSON response contract**: I'm asking the model for
   `{"open": bool, "reason": string}` and using `response_format:
   json_object` when supported, with a best-effort fallback parse if a small
   local model doesn't strictly comply. Real-world reliability of this will
   only be known after testing against Rapid-MLX/Qwen3.6 on the USB board.
