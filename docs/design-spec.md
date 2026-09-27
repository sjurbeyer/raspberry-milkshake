# Raspberry Milkshake: Design Spec (v0.1, draft)

Status: draft for iteration. The original notes file (`3D sound setup.md`) was superseded by this spec and removed; it is in the git history.

Implementation progress: the portable core (`src/core`: cue times, manifest parser, sequencer, play queue and browse policy, channel map), the `rm-validate` tool, the container test loop, and a headless projectM host (`src/render`: EGL context, host, PNG snapshots, and a `ShowRunner` that steps the sequencer, the A/V lead and the audio window per frame; tested on llvmpipe), an LED perimeter layout with a GPU downscale sampling path, a browser dashboard (main output, touch display, LED chain, status; section 13.1), the Qt touch UI (`src/ui`, section 6.2), and the engine that connects them in one process (`src/app`, section 14), pause/continue/stop transport controls (section 15.1 item 1), and a headless start mode for when no touch display is detected (section 15.1 item 3) exist (Phase 0a and most of Phase 3). Not started: the touch display sleep mode (section 15.1 item 4), jam-mode keyboard navigation, real audio playback and capture, real LED/spotlight transports, everything on the Pi.

## 0. Goals
A Raspberry Pi 5 appliance that runs projectM with two modes:

1. **Shows**: pre-authored albums and playlists (stereo or 5.1) with a timeline of `.milk` preset changes.
2. **Interactive (jam)**: live audio in, low-latency visuals, and keyboard preset navigation.

The Pi's HDMI goes to the Marantz NR1510 AVR and on to the projector. The 7" Touch Display 2 is only a selector and remote for the main output.

Non-goals for v1: LED and spotlight output (designed for, built in Phase 5), AI HAT+ use, network streaming sources, Bluetooth audio (backlog, section 15.3).

The owner's prioritized ideas list is in section 15. Its high-priority items are pause, continue and stop controls, interactive as the first tile (done), interactive mode by default when no touch display is connected, and a touch display sleep mode.

## 1. Hardware
| Item | Decision |
|---|---|
| Compute | Raspberry Pi 5, 16 GB, case with heatsink and fan. Use the 5 V / 5 A supply. |
| Main display | HDMI0 → Marantz NR1510 → ON CPR 300 projector. |
| Touch display | Touch Display 2 (DSI, 7", natively portrait 720×1280). The UI renders rotated to 1280×720 landscape. Raw DRM apps are not rotated by the OS, so rotation is done in the Qt KMS config, and the touch matrix must match. |
| Audio out | HDMI LPCM, 5.1 at 48 kHz. The NR1510 is a 5.2-channel AVR, so 5.1 is the ceiling. ALSA HDMI channel order (FL FR RL RR FC LFE) differs from file order (FL FR FC LFE BL BR), so the engine owns an explicit channel map. |
| Audio in (jam) | Undecided. The Pi 5 has no line-in or jack, so use a USB audio interface or USB mic through an ALSA capture device. The UI provides input select and gain. |
| AI HAT+ (26 TOPS) | Not in v1 (section 9). Verify it physically fits in the current case. |
| Storage | **Kingston 64 GB.** See below. |

### 1.1 SD card sizing
| Budget item | Size |
|---|---|
| OS + Qt + projectM + ffmpeg libs | ~6 GB |
| Presets and textures | ~1 GB |
| Thumbnails, analysis, shader cache | < 0.2 GB |
| Free for media (about 50 GB of the 64 GB) | |

Media capacity in that ~50 GB:
- ~25 h of 24-bit 5.1 FLAC (~1.9 GB/h)
- ~40 h of 16-bit 5.1 FLAC
- ~125 h of stereo CD-quality FLAC

Peak read rate is ~0.5 MB/s, so the 100 MB/s card has over 100× headroom. Use the 128 GB card only if the library outgrows the above. The 1 TB card is unnecessary.

SD wear: read-only root (overlayfs), logs in tmpfs, and a separate ext4 `/data` partition for the library, config, and shader cache. Transfer media over the network (rsync/SFTP) or by USB stick import.

## 2. Architecture
**One process, Qt EGLFS, two screens.** Only one process can hold DRM master, so a separate UI process and renderer process is not viable without a compositor. The engine is Qt-independent, so a split behind a wlroots compositor stays possible later.

Threads:
- **RT audio thread** (SCHED_FIFO, pinned core): ALSA playback and capture. It writes to a lock-free tap ring for analysis.
- **Render thread** (pinned core, performance governor): owns the projectM GL context on the HDMI screen.
- **Sequencer**: driven by the audio sample clock, never wall clock.
- **Qt UI and Qt render threads**: the touch screen, mostly idle and render-on-demand.
- **Worker pool**: decode, library scan, thumbnail loading.

```
Library (SQLite index) ─► Controller (queue, mode, state)
Audio engine ─► sample clock ─► Sequencer ─► projectM host ─► Canvas ─► Sinks (screen, LEDs, spots)
Audio tap (PCM) ─► projectM (+ future AnalysisProviders)
Touch UI (QML) ◄─► Controller
Keyboard (evdev) ─► Controller (interactive)
```

- **OS**: Raspberry Pi OS Lite 64-bit (current stable), with no desktop or compositor. Direct ALSA (exclusive), with no PulseAudio or PipeWire, since this is a dedicated appliance.
- **Stack**: C++23 (GCC 14 on Raspberry Pi OS Trixie), Qt 6 / QML, CMake, SQLite for the library index.
- **Risk**: the packaged Qt may lack the EGLFS/KMS plugin. Fallback is a custom Qt build.
- **Risk**: GPU contention. The UI and projectM share one V3D GPU. Keep the UI static when idle and measure UI frame times under heavy presets. The touch display sleep mode (section 15.1, item 4) is the main mitigation. Fallback is Qt Quick's software backend for the UI.

## 3. Audio and sync

### 3.1 Show playback
Latency is irrelevant here because the audio is known ahead of time.
- Feed projectM PCM from `play_pos + (display_latency − audio_latency)`. If that difference is negative, delay the audio output instead.
- A per-output calibration value `av_offset_ms` lives in config and is set in a calibration screen with a click/flash test.
- Cues are scheduled to sample accuracy against the audio clock.
- The importer normalizes everything to 48 kHz, so there is no runtime resampling.

### 3.2 Interactive
This is the only latency-critical path.
- USB capture at 64 frames × 2 periods, hw device, no resampling.
- The tap goes straight to `projectm_pcm_add_float`.
- **Budget: ≤ 45 ms p95 from audio-in to the DRM page-flip event on the Pi.** Measured in software by timestamping the capture callback and the flip-complete event.

| Stage | Approx. |
|---|---|
| Capture (USB, 64 × 2) | ~4 ms |
| Analysis window | ~5 ms |
| Render | 8–12 ms |
| Vsync quantization | ~8 ms avg |
| Scanout | 1 frame |

- Projector and AVR latency come on top. Use the projector's game mode and the AVR's lip-sync settings. This is outside our control and the spec states it as such.
- No triple buffering. Use atomic KMS flips.

## 4. Rendering and performance
- **projectM v4.1.7**, pinned through CMake FetchContent, built static with GLES on (Mesa V3D on the Pi, which exposes GLES 3.1; llvmpipe in the dev container), with the playlist library off and the bundled projectM-eval. Version 4.1.4 had a GLES/NPOT texture-resize bug from SOIL2 detection; whether 4.1.7 still has it is unverified on the Pi (spike S1).
- **Output constraint**: projectM 4.1.7 always draws its final image into the context's default framebuffer (`glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0)` in `ProjectM::RenderFrame`; upstream has a "ToDo: allow external apps to provide a custom target framebuffer"). The host therefore renders into the surface of its EGL context (an offscreen pbuffer in tests and the container).
- **Dynamic internal resolution.** Default 1280×720 (range 640×360 to 1920×1080), upscaled in the final blit, with a hysteresis controller holding 60 fps.
- **CPU cost scales with the mesh, not the pixels.** Per-pixel equations run on the warp mesh (CPU, projectM-eval), so mesh size is a setting.
- **`rm-bench`**: renders every preset headless for N seconds, records load, first-frame and steady frame times, and tags each preset ok / heavy / broken (it also lists the ones that fail to load, so the pack can be pruned). Only "ok" presets enter pools by default. It also pre-warms the Mesa shader cache on `/data`, because preset switches otherwise cause 50–300 ms hitches from parse and shader compile.
- **Preset-switch hitches (measured).** On the dev container (llvmpipe, 640x360, 60 presets from the community pack) a switch costs three things, none of them on the Pi yet:
  1. **First frame after a load**, median 194 ms and up to 570 ms cold: the preset's shaders are compiled the first time it renders. With a warm Mesa shader cache this falls to a median of 19 ms (worst 66 ms), and the load call itself halves (32 to 14 ms). A cold 200 ms switch drops about 6 frames at 30 fps.
  2. **Blended transitions** render the old and the new preset together for the whole soft-cut duration, roughly doubling frame time (for example 24 to 41 ms and 28 to 57 ms), so any preset that is already above about half the frame budget drops frames for the length of the blend.
  3. **The preset load call** blocks the render thread (parse and equation compile): 14 to 32 ms median, up to 76 ms.
  The dashboard adds about 7 ms per frame (JPEG) on the same thread, which is dev-only. About 2 % of the sampled presets failed to load (shader syntax errors in the preset itself).
  Consequences: (a) the shader cache lives on persistent storage and is pre-warmed by `rm-bench` on the target device, because a cache is only valid for the GPU driver that made it; the dev dashboard keeps it in a Docker volume (`just forget-shaders` clears it); (b) `rm-bench` records load time, first-frame time (cold and warm) and steady frame time, and excludes presets that fail or whose blend would exceed the frame budget; (c) shows should use `cut` (or a short blend) for heavy presets, and the dynamic-resolution controller may drop resolution during a blend; (d) cache size is about 55 KB per preset, so a curated pool of a few thousand presets stays within the 0.2 GB budget, but the full 9,800-preset pack would need about 0.5 GB.
- **Preset ratings (MilkDrop's own system).** Every `.milk` file carries `fRating=<0..5>`. A preset rated **0 is never picked at random**: it is left out of every pool (`all`, `dir:`, `tag:`, and the interactive pool), but a show that names it explicitly (`preset = "..."`) still plays it. projectM itself ignores ratings (`fRating` appears only in its built-in idle preset), so the catalog enforces them. Facts about the community pack used in development: all 9,795 presets are rated 5, so any lower rating is the user's own choice, and 3 is assumed when a file has no rating.
  - **Where the rating lives:** in the preset file, edited in place (only that line changes, line endings and other bytes are kept, and the file is replaced atomically), so it also works in MilkDrop and other tools and needs no separate database. On the Pi the presets sit on the writable `/data` partition.
  - **Dev dashboard:** the status pane has buttons 0 to 5 for the preset on screen. Rating a preset 0 hides it and the show moves on straight away: the engine re-plans the current track from the position it was at (`Engine::refresh_schedule`), because pool choices are made up front. Only names the catalog itself found can be rated, so a request cannot reach files outside the preset folder. `just hidden-presets` lists everything rated 0; setting a preset back to 5 restores it.
  - **Not done yet:** MilkDrop also weights random picks by rating (a 5 is picked more often than a 1); here the rating only excludes at 0. `rm-bench` should also mark presets that fail to load, and the jam mode's keyboard should be able to rate the preset on screen.
- Performance governor, fan curve check, and `vcgencmd get_throttled` in a debug overlay.

## 5. Collection format
A **collection** is a directory with a TOML manifest, hand-editable and validated by the importer. Album, playlist, and interactive share one model via `type`. The full schema is in [show-format.md](show-format.md); the example below is illustrative.

```toml
# collection.toml
format  = 1
type    = "album"                 # album | playlist | interactive
title   = "Night Drive"
tags    = ["pleasing", "edm"]     # drives the tag dropdown
cover   = "cover.jpg"             # square, >= 1024 px
preview = { track = 1, start = "0:45", length = "20s", preset = "presets/flow.milk" }
defaults = { transition = "blend:2.0" }

[[track]]
file = "01-intro.flac"            # 48 kHz, stereo or 5.1
title = "Intro"
bpm = 122                         # optional, else from analysis

  [[track.cue]]
  at = "0:00.000"                 # or "bar 17", "bar 17 beat 3.5"
  preset = "presets/tunnel.milk"
  transition = "cut"

  [[track.cue]]
  at = "bar 33"
  pool = "tag:edm"                # rule-based: pick from a pool
  every = "8 bars"
```
- Playlists reference tracks from other collections by path and may override cues.
- Interactive collections define `preset_pool`, key banks, and favorites instead of tracks.
- Audio is FLAC (master) with optional Opus for size. Multichannel is supported in both.

### 5.1 Tools
- **Shaker** (laptop, separate repo, [shaker-spec.md](shaker-spec.md)): the collection editor. Retires the `rm-import` CLI this section used to plan — Shaker absorbs that job (transcode to 48 kHz, loudness-normalize, generate cover thumbnails, detect the beat grid) inside a timeline UI for scheduling presets against a waveform, plus preset browsing/rating and validated export.
- `rm-bench` (Pi): the preset bench from section 4.

## 6. Touch UI (Qt Quick, 1280×720 landscape)
- **Top bar**: view toggle button on the left. Filter dropdowns on the right: type (All default, Albums, Playlists, Interactive) and tag (pleasing, edm, challenging).
- **Grid view**: scrollable grid of square covers. Playlists use the same square cover format. "Interactive visualization" is a tile styled as an album. Tapping a tile opens the swipe view at that item.
- **Swipe view**: the cover is centered and flicks left/right between items.
  - If nothing is playing, the preview audio and visual play on the main output.
  - If something is playing, the preview is suppressed and two buttons appear: **Add to queue** and **Play now** (interrupts).
- **Smoothness**: pre-scaled thumbnails uploaded as textures, GridView with cell caching, no ShaderEffects/blur, no disk access on the UI thread, and 60 fps as a tested target.
- **Interactive input**: USB keyboard read directly via evdev. Arrows for prev/next, number keys for favorites banks, space for random, Enter to lock/unlock auto-switch, `[` `]` for sensitivity, and an optional OSD on the main output (off by default).

### 6.1 Visual identity
The identity package is in `docs/raspberry-milkshake-visual-profile/raspberry-milkshake-brand/` (`BRAND.md` for the rules, `tokens/raspberry-milkshake.tokens.json` as the canonical token source). It was drawn for a desktop, VS Code-like interface, so this spec adapts it to a finger-operated 7" panel.

- **Tokens in Qt**: a `Theme` QML singleton holds the colors, radii, spacing and durations from the tokens file. A unit test compares it with the JSON so the two cannot drift. The CSS files are for the dev dashboard and docs only.
- **Look**: midnight canvas, hairline borders, no drop shadows. Raspberry marks live, armed or selected state only. Pale milk is reserved for focus rings, highlights and large labels, never small body text. Signal blue is for calibration and monitoring screens only.
- **Touch sizing** (the panel is about 8.3 px/mm): touch targets are at least 72 px (about 9 mm). Type is scaled about 1.7× from the brand's desktop sizes, keeping their ratios: controls 22 px, metadata 20 px, titles 30 to 38 px. Verify on the real panel.
- **Selection**: a selected tile or preset gets a 2 px milk border and a raised-surface fill. The play button turns raspberry with a midnight icon while live. Filter dropdowns and the view toggle use the same "pressed" state.
- **Labels**: UI chrome is lowercase, following the brand tone. Collection titles are shown exactly as authored.
- **Marks**: the full mark appears in the top bar and on the startup splash (the lockup). The mark alone is used on calibration screens (A/V and LED). The mono mark is for tinted states and low-bandwidth LED use.
- **Motion**: transitions use the 160, 240 and 360 ms durations with the `ease-liquid` curve (`cubic-bezier(.22, 1, .36, 1)`, which is `Easing.BezierCurve` in QML). Only the mark, the live dot and the visualizer move; the chrome stays still. A `reduced_motion` config option freezes the mark and leaves a low-contrast breathing dot.
- **GPU cost**: the UI shares the GPU with projectM (section 2), so continuous animation is confined to small areas (the mark and the live dot), and the UI otherwise renders on demand. Audio-reactive mark scaling (bass enlarges the spiral by 2 to 5 %) needs a low-rate band-energy feed from the engine and is optional, gated by measured GPU headroom.
- **Live indicator**: the projected output is a boundaryless stage, so nothing is drawn over it by default. The one opt-in exception is the pause icon of the `fade` pause behaviour (section 15.1, item 1). The brand's thin frame with a raspberry live dot and `LIVE` label appears around the main-output pane of the dev dashboard and in the touch UI's playback bar.
- **Open**: `BRAND.md` recommends sine easing while the CSS and tokens use the `ease-liquid` quint-out curve, and the tokens file has no easing token. This spec follows the CSS curve until the profile is reconciled.

### 6.2 Implementation (as built)
- **C++ model layer** (`src/ui`, no QML dependency): `LibraryModel` scans a folder of collections (interactive first, then alphabetical; broken manifests are skipped and reported), `LibraryFilter` applies the type and tag dropdowns, and `AppController` wraps the core play queue and browse rules and emits `previewRequested`, `previewStopped`, `playRequested` and `stopped` for the engine to act on.
- **QML module `Milkshake`** (`src/ui/qml`): `HomeScreen` (top bar, grid, swipe carousel, playback bar, empty state) built from `CoverTile`, `CollectionGrid`, `CollectionCarousel`, `PlaybackBar`, `RmButton`, `RmDropdown`, `CoverArt`, `LiveDot` and the `Theme` singleton. Previews start after 250 ms of stillness in the swipe view, and only while idle.
- **SVG**: this Debian build ships no Qt SVG image-format plugin, so SVGs (covers, icons, the mark) load through an app-owned `image://svg/` provider that draws with `QSvgRenderer`. That keeps the UI independent of which plugins a distribution packages.
- **Transport controls**: while something plays or is paused the playback bar shows the collection, the queue length and three text buttons: pause (which becomes continue, in raspberry, while paused), next, and stop. Stop opens a `ConfirmDialog` ("stop playback?" with "Stop <title>?" or "Stop <title> and clear the queue?"); cancel, tapping outside the card and Escape all leave everything as it was, and confirm ends playback and empties the queue. The dialog closes by itself if playback ends while it is open. The space bar toggles pause when something is playing and the dialog is closed. While paused the bar shows a small pause mark and "paused", and the swipe view treats the paused collection as playing (no previews, add to queue and play now). Icons instead of text and the "play now" interrupt prompt are still to do (section 15.2, item 8).
- **Tests**: the model layer, controller and theme-versus-tokens check run as Catch2 tests, and 37 Qt Quick Test cases drive the real UI offscreen with simulated mouse and keyboard input (filters, swipe, preview and queue rules, pause and continue, the stop dialog). `rm-touch-ui --screenshot <file> [--view swipe] [--select <id>] [--play <id>] [--pause] [--ask-stop]` renders a state offscreen for a visual check.
- **Known gaps**: the swipe view has no page indicator and the grid has no keyboard focus handling (touch only, by design; the space bar for pause is the one keyboard shortcut). The controller itself plays nothing: the engine acts on its signals (section 14).

## 7. Output sinks: compute once
MilkDrop is a feedback system: each frame warps the previous frame. Evaluating only at LED locations therefore does not work, because the full field must be simulated at some resolution. The savings come from three other places:

1. **One canvas, many sinks.** All outputs share one rendered image (the "virtual canvas", the union of the screen and LED domains). Because projectM only draws into framebuffer 0 (section 4), the canvas is that framebuffer.
   - The screen sink is the presented framebuffer (with an upscale when the internal resolution is below the display's).
   - An LED or spotlight sink takes a GPU-side downscale of the canvas before it is presented: `glBlitFramebuffer` with linear filtering from framebuffer 0 into a small FBO, then an asynchronous readback (PBO plus fence) of just that small image, sampled at the precomputed LED positions, then gamma/limit/dither and send.
   - Nothing is evaluated twice.
2. **LED-only mode.** With no screen attached, the context's surface is simply small (~160×90 to 320×180, which the headless pbuffer path already supports), with a reduced mesh and no presentation. This cuts GPU cost by roughly 10–50×.
3. **A canvas larger than the screen** (LEDs that extend beyond the picture) is not possible with stock projectM 4.1.7, since it cannot render to a custom target. Options for Phase 5: carry a small patch adding a target-framebuffer parameter (upstream has it on its to-do list), or render the union domain as framebuffer 0 and crop for the screen with a blit. Decide when a real LED layout exists.
4. **A cost planner** compares "one union canvas" against "separate canvases per sink". It only pays off when the domains differ hugely, such as a room-wide LED ring plus a 16:9 screen. v1 always uses the union canvas.

Spotlights use the same sampling plus temporal smoothing. LED transports are abstract (DDP / Art-Net / sACN / serial). The Pi 5's RP1 breaks the old `rpi_ws281x` PWM/PCM path, so verify options in Phase 5. The safest route is an ESP32 (WLED) or Pico receiving over UDP or USB.

## 8. Repo layout
```
src/engine/   audio, sequencer, projectM host, canvas, sinks
src/ui/       QML and view models
src/app/      main, config, systemd glue
tools/        rm-bench (Shaker, the collection editor, is a separate repo)
docs/         design-spec.md, show-format.md, raspberry-milkshake-visual-profile/ (brand)
packaging/    systemd unit, image build, config templates
tests/        parser, sequencer (fake clock), cue timing
```

## 9. AI HAT+ (Hailo-8, 26 TOPS)
It is a CNN inference accelerator, so it cannot run MilkDrop equations or GLSL. Candidate uses:
1. Instrument or mood classification for interactive mode. Latency of 20–100 ms makes it semantic only, not timing-critical.
2. Neural frame post-processing. GPU-to-CPU readback plus PCIe transfer likely erases the benefit.
3. Camera gesture control.

None are on the critical path, so they are out of v1. The engine exposes an `AnalysisProvider` interface so the HAT can plug in later.

## 10. Roadmap
- **Phase 0a (dev container, off-Pi)**: Dockerfile, compose dashboard, backend abstractions and CTest skeleton (section 13). Phases 1–3 are developed mostly here.
- **Phase 0b (Pi spikes, each with exit criteria):**
  - S1: projectM GLES build, and frame-time sweep over ~200 presets at 720p and 1080p.
  - S2: Qt EGLFS with DSI (rotated) plus HDMI both rendering, and UI still at 60 fps under a heavy preset.
  - S3: ALSA HDMI 5.1 playback and USB capture round trip.
  - S4: software latency harness using the capture and flip timestamps.
  - S5: preset-load hitch measurement, with and without a warm Mesa shader cache.
- **Phase 1**: engine core (audio, sequencer, projectM host, A/V offset, queue), driven from a CLI.
- **Phase 2**: collection format, parser, validator, `rm-bench`, two sample collections. (Authoring tooling is Shaker, a separate repo/track of work; see [shaker-spec.md](shaker-spec.md).)
- **Phase 3**: touch UI (grid, swipe, filters, queue/play logic, preview rules).
- **Phase 4**: interactive mode (capture path, keyboard navigation, banks, OSD) and latency tuning.
- **Phase 5**: canvas and sinks (LED, spotlight, planner).
- **Phase 6**: appliance hardening (read-only root, systemd, watchdog, thermals, image build). AI experiments optional.

MVP is Phases 0a–4.

**Backlog placement** (details in section 15): the transport controls (item 1) and the now-playing detail (item 9) are small Phase 3 additions; the sleep mode (item 4) and the symbols and interrupt confirmation (item 8) are Phase 3 UI work; settings (item 5) grows with each feature that needs one; the headless interactive start (item 3) is Phase 6 appliance behaviour, with its no-UI start mode addable earlier; the LED editor (item 6) waits for Phase 5; the home screen sound and light (item 7), the interactive control screen (item 10), background animations (item 11) and Bluetooth (item 12) come after the MVP.

## 11. Acceptance targets
- Curated presets hold ≥ 60 fps at the default internal resolution, with 1%-low ≥ 55 fps.
- Touch scroll and swipe run at 60 fps, and tap-to-response is ≤ 100 ms, including while a heavy preset plays.
- Interactive Pi-side audio-in to page-flip is ≤ 45 ms p95.
- Cue timing error is ≤ 1 frame against the audio clock.
- After setting `av_offset_ms`, the click/flash test aligns audio and visual to within 1 frame.
- A 1-hour soak shows no thermal throttling and zero audio xruns.
- Boot to home screen in < 25 s, and a power cut on the read-only root leaves the system healthy.
- Parser and validator tests cover the sample collections plus malformed input.

## 12. Open questions
- Projector input latency is unknown, so calibration is required.
- Jam input hardware is undecided.
- LED and spotlight hardware and protocol are decided in Phase 5.
- Should Interactive be queueable? Assumption: "Play now" only in v1.
- Preset source: assume cream-of-the-crop plus custom presets, filtered by `rm-bench`.

## 13. Dev environment and test strategy

### 13.1 Dev container
Purpose: develop and validate logic, UI, collection format and sync off the Pi, from a Windows host (Docker Desktop, WSL2 backend).

- **Image**: one `dev` image on the same Debian base as Raspberry Pi OS (Trixie), with the build tools and Mesa EGL/GLES (llvmpipe software rendering). projectM is built from source by CMake (pinned v4.1.7). Qt 6.8 (the same series Raspberry Pi OS Trixie ships), noVNC/websockify and the Inter font are in the image too. Still to add: ALSA/ffmpeg dev libs. Build multi-arch (buildx) later so the Pi uses the same dependency set.
- **Dashboard** (`docker compose run --rm fetch-presets` once, then `docker compose up dashboard`, browser at http://localhost:8080, bound to this machine only). `rm-app` (the whole appliance in one process, section 14) serves:
  - **Main output**: an MJPEG stream of the rendered picture, framed per the brand's projector-output rule (thin neutral line, small raspberry live dot and `LIVE` label). This replaced the planned Xvfb + noVNC pane, which is not needed for a headless render.
  - **LED chain**: a canvas that draws the chain from the values the real LED sampling path produces (a GPU downscale of the same rendered frame, then `sample_leds`). The layout is a 76-LED perimeter strip for now. The same page can later point at a real Pi.
  - **Status**: the main output's mode and collection, current preset, its rating (buttons 0 to 5 to change it, see section 4), the number of hidden presets, media time, frame rate and cost, load errors (each failing preset is also logged by name) and the GL renderer.
  - **Touch display**: the real Qt touch UI, part of the same `rm-app` process, on Qt's VNC platform plugin with the software renderer, shown through websockify + noVNC on port 6080 (also bound to this machine only). A mouse drag stands in for touch, and what you do there drives the main output above. The launcher (`tools/dev/run-dashboard.sh`) restarts `rm-app` if it exits; extra options go in `RM_APP_ARGS` (for example `--track-seconds 6`).
  - The audio is synthetic (kick, hats, pad) until audio playback exists, and the frame rate is fixed at 30 fps in the container.
- **`just`** wraps the everyday commands (`just` lists them): `just dashboard` fetches presets if needed, starts the dashboard, waits until it is healthy and opens the browser; `just stop`, `just logs`, `just restart`, `just test`, `just test-label ui`, `just validate <dir>`, `just shell`. Options can be overridden, for example `just track_seconds=6 dashboard` or `just pause_display=fade dashboard` (hold, fade or home). On Windows the recipes run in PowerShell 7.
- Underneath, `docker compose run --rm test` builds and runs the suite and `docker compose run --rm dev` opens a shell in the container (source at `/src`, build tree at `/build`). The build tree lives in a named volume so artifacts stay out of the OneDrive-synced folder. In Git Bash on Windows, prefix commands that pass absolute container paths with `MSYS_NO_PATHCONV=1`.
- Optional: WSLg passthrough for native windows.
- **Dev profile defaults**: internal resolution 640×360 and a virtual clock, so cue scheduling is exact even if llvmpipe drops frames.

### 13.2 Backend abstractions
The engine selects these by config profile (`pi` or `dev`):

| Interface | Pi | Dev |
|---|---|---|
| `AudioBackend` | ALSA (HDMI 5.1, exclusive) | virtual file-driven (virtual clock) or PulseAudio |
| `InputSource` (jam) | ALSA capture (USB interface) | file loop or tone generator |
| Display profile | Qt EGLFS, two screens | Qt windowed, two windows |
| `LedSink` transport | DDP / Art-Net / sACN / serial | WebSocket simulator |
| Clock | audio sample clock | virtual clock (faster than real time in tests) |

### 13.3 Test layers (CTest, run in the container)
1. **Unit** (Catch2 or GoogleTest): manifest parser and validator, cue math (bar/beat to time), channel mapping, queue and preview rules, dynamic-resolution controller.
2. **Sequencer**: play a sample collection at faster than real time and compare the cue-to-preset log with the expected one.
3. **Rendering sanity**: silent input gives a valid, non-black frame. A click track changes the picture within N frames of each onset. Use tolerance or SSIM comparisons, since llvmpipe output is not bit-exact.
4. **UI** (Qt Quick Test, offscreen): filters, grid to swipe navigation, Add to queue / Play now vs. preview behaviour, and screenshot comparison.
5. **Sink**: a known test pattern on the canvas yields the expected LED values.
6. **Fuzz**: a libFuzzer target on the manifest parser.

### 13.4 Hardware-only tests (label `hw`, run on the Pi)
Interactive latency harness (≤ 45 ms p95), 60 fps preset sweep, dynamic-resolution behaviour under GPU contention, 1-hour thermal soak, xruns, and HDMI 5.1 channel-map check.

### 13.5 What the container cannot tell you
Frame time, GPU contention, latency and thermals. Software GL on x86 is not representative of the Pi's V3D GPU, so these acceptance targets (section 11) are only verified on hardware.

Also, projectM's own timers (the MilkDrop `time` variable and the progress of a blended transition) follow the wall clock, not the frame count. Fast-stepped tests therefore assert hard cuts and picture content, not blend progress. Preset switches and audio windows are exact because the `ShowRunner` decides them from the frame index.

## 14. Engine and app (as built)
`rm-app` is the appliance in one process, as it will run on the Pi:

```
UI thread (Qt)      LibraryModel, LibraryFilter, AppController, QML home screen
   | EngineBridge: controller signals -> engine commands; engine "finished" -> controller.next()
Render thread       EGL context, ProjectMHost, Engine (tick per frame), LED downscale, JPEG
HTTP threads (dev)  DashboardServer: MJPEG, LED colours, status
```

- **Engine** (`src/render/engine.*`, Qt-free): commands (`preview`, `stop_preview`, `play`, `stop`) can be posted from any thread and are applied by `tick()`, once per frame, on the thread that owns the projectM context. Time is the frame count, never the wall clock, so it is deterministic in tests.
- **Modes**: idle (projectM's built-in idle preset, `kIdlePresetUrl`), previewing, playing, paused. projectM 4.1.7 needs the full name after `idle://`: a bare `idle://` throws, which silently left the last preset on screen after a stop until a regression test caught it.
- **Pause** (section 15.1, item 1): `Engine::pause()` and `resume()` freeze the frame clock, so cues, track ends and the audio window all stand still, and resume on the same frame. It applies only to a playing collection (not a preview, and not one in its end-of-collection grace period). `EngineConfig::pause_display` picks what the output shows: **hold** (default) draws nothing, so the last frame stays and no compute is spent; **fade** copies the last frame into a texture and dims it to black over half a second (`Overlay`, a small GLES pass that restores GL state afterwards), then `draw_overlays()` puts the pause icon on top, which the app calls after the LED sampling so the LEDs fade but never show the icon; **home** blends to the home visualization (projectM's idle visual for now) and blends back to the preset that was showing. Continuing from a fade fades back in from black. Play, next and stop end a pause, and a rating change while paused in home mode keeps the home visual. `rm-app --pause-display hold|fade|home` (or `just pause_display=... dashboard`) chooses one until the settings screen exists.
- **Stop and clear**: `AppController::stopAndClear()` empties the queue and returns to idle; the UI asks first (section 6.2).
- **Preview**: loops inside the collection's `preview` window, showing its `preset` if set and otherwise following the track's own cues. Previews are ignored while something plays, and `play` interrupts a preview. An interactive collection previews only its `preset`.
- **Playing**: an album or playlist plays its tracks in order with their cues; an interactive collection plays its preset pool, switching every 8 bars, until stopped.
- **The end of a collection**: the engine reports it once and keeps the visuals going for a short grace period (15 frames), so the next queued collection starts without flashing the idle screen. The bridge turns the report into `AppController::next()` on the UI thread.
- **Dev stand-ins until audio exists**: every track plays for the same length (60 s, or `--track-seconds`), and the audio the visuals react to is synthetic music. Both are replaced when audio decoding lands.
- **Tests**: 11 engine tests (frame-exact cues, track order and finish, preview looping, interruption rules, cross-thread commands) and 9 bridge tests running the real controller against a real engine. The full flow was also driven through the browser path (VNC taps): preview, play, queue, automatic advance, return to idle, preview again.
- **Threading rule**: only the render thread touches the projectM host and GL context; the UI thread only posts commands and reads the last published status.

## 15. Backlog: the owner's ideas list
These are requirements and ideas in the owner's own priority order. Each item says what is wanted, how it fits the design above, and what it depends on. "Done" means it exists in the code today. Open questions are collected at the end of the section.

### 15.1 High priority
1. **Pause, continue and stop.** *Built* (sections 6.2 and 14; the touch UI uses text buttons for now, and icons come with item 8). The design below is what was built, with the setting still an engine option rather than a settings-screen entry. The playback bar has three controls:
   - **Pause / continue** (one toggle). Pause freezes the show clock and the audio together, so nothing advances: no cue fires, no track ends, and the queue is untouched. Continue resumes from the exact sample, so audio and visuals stay aligned. What the main output shows while paused is a setting with three options, described below; the default is to hold the last frame. For an interactive collection, pause holds the current preset (automatic switching stops) and continue resumes switching. Next while paused starts the next item playing. A paused collection counts as "something is playing" for the browse rules: previews stay off, and the swipe view offers add to queue and play now.
   - **Stop, which also clears the queue,** and always asks first: "Stop <title> and clear the queue?" (or just "Stop <title>?" when the queue is empty), with cancel and confirm. It ends the current item, empties the queue, and returns the main output to idle. This shares one confirmation dialog with the "play now" interrupt prompt (item 8).
   - **What the main output shows while paused** is the owner's choice between three behaviours, since the right one depends on how it feels on the real projector:
     - `hold` (the default): the last frame stays on the output and the engine stops rendering, which also saves compute. The LEDs hold their last colours.
     - `fade`: the picture fades to black and a pause icon (two bars, in the brand's flat style) is drawn on the main output. This is the one case where something is drawn over the projected picture (section 6.1), and it is opt-in. The LEDs fade to black too but do not show the icon.
     - `home`: the output transitions, with projectM's soft cut, to the default home screen visualization. This is the same idle visual as in item 7 (today projectM's built-in idle preset).
     Continuing goes back the same way: `hold` simply carries on, `fade` fades back in, and `home` blends back to the preset that was showing when the pause began. All three exist from the start and only the default is decided; the choice moves into the settings screen (item 5), and until then it is an engine option, `EngineConfig::pause_display`.
   - **Keyboard:** the space bar toggles pause, which matters for the headless start (item 3) and jam mode.
   Design impact: a `Paused` state in the core transport policy (`TransportState`) and in the engine mode, engine commands `pause` and `resume`, `Queue::clear()`, and `AppController::pause()`, `resume()` and `stopAndClear()` with the matching signals; the bridge maps them to engine commands. Tests cover the controller rules, the engine (a paused engine does not advance, resumes on the same frame) and the QML flows including the confirmation dialog. The `fade` option needs a small overlay pass after projectM has drawn (one full-screen quad with alpha, plus a small icon texture). For the LEDs the fade is applied before their downscale and the icon after it, so only the fade reaches the LEDs. The `home` option needs the engine to remember the preset that was active at the pause (the runner already tracks it) and to load it back with a blend.
2. **Interactive is the first option, top left of the grid.** *Done:* the library sorts interactive collections first, then everything else alphabetically, so the "Interactive visualization" tile is top left of the grid and first in the swipe view under the default "all" filter (a library-order test guards it). Under a type filter that excludes interactive it is naturally absent.
3. **No touch display connected: start in interactive mode.** *Partly built* (`src/app/headless.{hpp,cpp}`, wired into `rm-app`): `should_run_headless()` triggers when `QGuiApplication::screens()` is empty, overridable either way with `--headless` / `--ui` (the spec's `--no-ui` idea, named to match the pair). Headless skips creating the `QQuickView`/`LibraryFilter`/SVG provider entirely and calls `controller.playNow()` on the first interactive collection in the library (`find_default_interactive_id()`), proven with a test that this needs no QML view at all. The dashboard's `/state.json` carries `"headless"`, and the touch-display pane shows an explanatory note instead of a dead VNC connection when it's true. *Decided:* the check happens at startup only; a touch display plugged in later needs a restart of the app. **Not built yet: keyboard navigation** (jam-mode control from section 6) — headless mode currently starts interactive playback but nothing responds to a keyboard, since that needs a Linux evdev reader independent of Qt windowing, which is not verifiable without a real Pi or a virtual input device and is left for that hardware phase. On the real Pi, whether EGLFS reports zero Qt screens when the DSI panel truly has nothing attached is also unverified (Phase 0b territory); the dev container always reports one screen (real or the VNC plugin's virtual one), so auto-detection is exercised there only via `--headless`.
4. **A sleep mode for the touch display.** After a settings-controlled idle time the touch display shows a blurred version of the active visualization until it is tapped, to ease compute. Design:
   - **No second render** (*decided:* downscaling from the LED path or the screen picture is good enough). The main output is already downscaled on the GPU for the LEDs (section 7). The sleep screen shows a somewhat larger version of that picture (for example 160×90) scaled up with smooth filtering, which gives the blur for free, refreshed at a low rate (about 5 to 10 frames per second). The UI stops all other work: no animations, render on demand only.
   - **Wake-up:** the first tap only wakes the screen and is not passed to whatever is underneath, then the home screen returns at the view it left.
   - **Backlight:** dimming the panel through the Linux backlight interface is a possible extra to check on the real Touch Display 2.
   - **Why it matters:** the UI and projectM share one GPU (section 2). Sleep mode is the main mitigation for that risk, so it should land before heavier UI effects (15.3, item 11).
   - When the main output is idle, the sleep screen is simply dark.

### 15.2 Medium priority
5. **Settings.** A settings screen (a gear in the top bar) backed by a config file on the writable `/data` partition, read at start and applied live where possible. Every setting has a default, so no file is needed. Known settings so far: what the main output shows while paused (item 1), touch display sleep timeout (item 4), reduced motion (section 6.1), the A/V offset from the calibration screen (section 3.1), the jam input device and gain (section 1), the home screen sound and light options (item 7), and the entry to the LED editor (item 6), and Bluetooth pairing (item 12).
6. **LED editor.** An on-device editor for the LED chain, replacing the hard-coded perimeter layout the dev dashboard uses today. Per strip: number of LEDs, where its colours come from on the picture (an edge, a region or a path, with an inset), direction, whether the chain loops (the end wraps to the start) or not, and brightness, gamma and white balance. Design: strips are polylines in the picture's coordinate space and are stored in a validated file (`leds.toml`, checked the same way as `collection.toml`). At run time they become the existing `LedLayout`, and the sampling path is unchanged, so the picture is still evaluated once (section 7). The dashboard's LED pane shows the edited layout during development. Real transports (Phase 5) are needed before it drives hardware.
7. **Home screen sound effects and light visualization.** While browsing, gentle background sound (the example given is the ambient sounds of video game console menus) and a calm light pattern, both switchable in settings. Design notes:
   - **Sound needs a mixer.** Audio output is exclusive ALSA with no sound server (section 2), so the engine needs a small software mixer to play interface sounds next to music. Interface sounds are quiet and only play when idle or while music is ducked.
   - **Light:** the LEDs (and the idle main output) show a slow ambient pattern at the home screen. Today the idle main output is projectM's built-in idle preset; this makes the idle visual configurable.
   - **Motion rules:** honours reduced motion and the sleep mode.
8. **Symbols instead of text; play on the cover; confirm before interrupting.**
   - **Icons** replace the words for play, pause, add to queue, next and stop (item 1), in the brand's flat style. The visible text goes, but each control keeps an accessible name so tests and screen readers still work.
   - **Play on the cover:** in the swipe view the play button sits on the album cover.
   - **Confirmation:** choosing "play now" while something plays asks "Interrupt <title>?" with cancel and confirm at the 72 px touch size. Add to queue, next and pause need none; stop asks a slightly different question (item 1) through the same dialog. This changes the existing "play now" tests, which gain a confirm step.
9. **Now playing shows both the collection and the track.** For example "Night Drive · Intro (track 1 of 2)". The engine status already has the track number and count; it also needs the track title (from the manifest `title`, falling back to the file name) and the controller needs to expose it. For playlists it shows the playlist name and the title of the entry. The dashboard status pane shows the same.

### 15.3 Low priority
10. **Interactive-mode control screen.** In interactive mode, pressing the touch display opens a control panel that can be closed again (an x). It offers:
    - rate the current preset (the 0 to 5 buttons from section 4),
    - next preset and previous preset,
    - an automatic/manual transitions toggle,
    - colour randomization,
    - a preset file tree, its own tab in the panel, to change collection and pick presets by hand.
    Dependencies: a preset history (for "previous"), a manual mode with no automatic switching, a "load this preset now" command, and a browse API over the preset folders. Colour randomization has no projectM API. It is probably a hue shift applied after projectM has drawn, which needs a copy of the picture into a texture and one extra full-screen pass (a cost to measure on the Pi), so its feasibility is open. The same actions are on keys for someone at the keyboard.
11. **Lightweight animations behind the albums** (for example particles, or effects that react to touch). They must stay within a tiny GPU budget because the UI shares the GPU with projectM: a single shader or a particle system with a capped count, off during sleep mode and while a heavy preset runs, and switched off by reduced motion. Measure UI frame time under heavy presets before enabling it, and build it after the sleep mode (item 4).
12. **Bluetooth: a phone streaming to the Pi, with interactive visuals.** The Pi is a Bluetooth audio sink (A2DP): the phone plays, the Pi receives the stream, plays it through HDMI and builds the visuals from it. *Decided:* input only, no Bluetooth speaker as an output.
    - **Sync.** The visuals are built from the received sound as it arrives, so the phone's own Bluetooth buffering (typically 100 to 250 ms) does not misalign sound and picture: both come from the same received stream. What still applies is the output-path A/V offset of section 3.1. Live sound cannot be looked ahead, so unlike shows the visuals can only be delayed, never advanced; the calibration screen needs to allow that direction.
    - **Not for playing along.** Because of that Bluetooth delay, this is a listening mode; someone playing an instrument along with a Bluetooth stream would hear the stream late. Live instruments should use the wired jam input (section 3.2).
    - **How it appears.** Bluetooth is a selectable input for interactive mode (in the settings and in the interactive control screen, item 10), with a pairing switch in settings (item 5). Optional extras: play/pause from the phone (AVRCP) mapped to the pause control (item 1), and showing the phone's track title in "now playing" (item 9).
    - **Stack.** The design uses ALSA directly with no PipeWire or PulseAudio. BlueZ with a bluez-alsa bridge keeps that; otherwise Bluetooth would force a sound server. The Pi 5's built-in Bluetooth is used.

### 15.4 Decisions and remaining questions
Decided by the owner:
- **Transport:** pause and continue are the main controls; stop also clears the queue and always asks for confirmation (item 1).
- **Touch display detection:** startup check only (item 3).
- **Bluetooth:** a phone streaming to the Pi, input only (item 12).
- **Sleep mode picture:** downscaling from the LED path or the screen picture is good enough (item 4).
- **The picture while paused (item 1):** three selectable behaviours, hold the last frame (the default for now), fade to black with a pause icon, or transition to the home screen visualization. The owner will try them on the real projector and the setting arrives with the settings screen.

Nothing is open at the moment.
