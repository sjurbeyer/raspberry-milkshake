# Shaker: technical specification (v0.1, draft)

Shaker is the desktop editor for Raspberry Milkshake **collections** (`docs/show-format.md`): a "movie editor"-style app where you load songs, browse `.milk` presets with a live preview, drag them onto a timeline, and export a collection folder ready to copy to the Pi. The name plays on "milkSHAKEr" (shaking tracks and presets together) and reads naturally as a production-tool name on its own.

**This document is meant to leave this repository.** It lives in a **new, separate git repository** — there is no Pi/ARM/hardware constraint on Shaker, it is a normal desktop app on the author's Windows machine (cross-platform via Qt is a nice-to-have, not a requirement). Build it there from this spec. The one thing that ties it back here is a git submodule dependency (section 2) and the cross-repo item in section 10.

## 0. Decisions already made
- **Name:** Shaker.
- **Stack:** Qt 6 / QML / C++23, the same stack as `raspberry-milkshake`'s touch UI, sharing that repo's `src/core` (`rm_core`) via git submodule rather than re-implementing the format.
- **Scope:** Shaker absorbs the audio-preparation job that `raspberry-milkshake`'s design spec had assigned to a planned `rm-import` CLI (transcode, loudness-normalize, detect the beat grid). `rm-import` is retired; see the edits noted in `docs/design-spec.md`.

## 1. Scope and non-goals
In scope: authoring, previewing and validating albums, playlists and interactive collections as defined by `show-format.md`; preparing raw audio into the format the Pi expects; browsing and rating a `.milk` preset library.

Out of scope for v1 (see section 12 for the fuller backlog): LED/spotlight authoring (that is the *running appliance's* LED editor, design-spec.md section 15.2 item 6, a completely different tool), Bluetooth/jam-mode anything, `tag:` pool authoring (blocked on section 10), full `rm-bench`-style timing benchmarks, sending files to the Pi over the network.

## 2. Repository and build
- New repo, e.g. `shaker`. CMake, C++23, Qt 6.4+.
- `raspberry-milkshake` is added as a git submodule (e.g. at `vendor/raspberry-milkshake`), pinned to a commit. Shaker's `CMakeLists.txt` does `add_subdirectory(vendor/raspberry-milkshake)` (or a trimmed subset) to get the `rm_core` target as-is: `manifest.{hpp,cpp}`, `cue_time.{hpp,cpp}`, `sequencer.{hpp,cpp}`, `dir_catalog.{hpp,cpp}`, `preset_rating.{hpp,cpp}`, `policy.{hpp,cpp}`, `channel_map.{hpp,cpp}`, `clock.{hpp,cpp}`, `frame.hpp`. This is a straight reuse: no changes needed to build `rm_core` standalone (it already has no Qt dependency, see `raspberry-milkshake/CMakeLists.txt`).
- **Not reused as-is:** `rm_render` (`ProjectMHost`, `EglContext`) is built against GLES behind a headless EGL context, for the Pi and the Linux dev container. Shaker needs projectM built for the **desktop GL / Core Profile** path instead (projectM's own CMake supports this: `ENABLE_GLES=OFF`, `find_package(OpenGL REQUIRED)`, GLEW on Windows) and a context owned by Qt, not by hand-rolled EGL. Shaker therefore vendors its own projectM (same pinned version, `v4.1.7`, for preview fidelity) and writes a small `PreviewHost` class analogous to `ProjectMHost` but built around a Qt-owned GL context (section 8). This mirrors `rm_render`'s design without depending on its code.
- Bumping the submodule pin is how Shaker picks up format changes; a CI check (build + run the submodule's own `ctest -L unit` before Shaker's own tests) catches drift early.
- Audio decode/encode: `libsndfile` or `dr_flac`/`dr_wav` (decode) plus `libFLAC` (encode) for the transcoding pipeline; a small cross-platform audio-output library (e.g. `miniaudio`) for timeline playback. These are Shaker-only dependencies, unrelated to the Pi's ALSA path.
- Beat detection: any maintained onset/tempo library (e.g. `aubio`) is acceptable — accuracy is secondary, since the user always gets to correct it (section 6).

## 3. Architecture
Single process. Two logical parts on top of Qt's normal event loop:

```
UI thread (Qt/QML)     Library browser, preset browser, timeline, inspector, dialogs
                        LibraryModel-like scan of the collections folder (read-only reuse of
                        the *shape* of raspberry-milkshake's LibraryModel; QML-facing, so it is
                        rewritten here rather than shared, since AppController's playback
                        semantics do not apply to an editor)

Preview pane            A QQuickFramebufferObject (or QQuickRhiItem on Qt 6.6+) embedding the
(Qt render thread)      PreviewHost (projectM, desktop GL). Runs on Qt Quick's own render
                        thread with Qt's own context — no separate EGL/thread management needed,
                        unlike rm-app's headless render thread (that exists there because rm-app
                        has no window on the Pi/EGLFS backend; Shaker always has a window).

Audio thread             Decodes the track under the playhead and feeds both the audio output
                        and the PreviewHost's PCM input from the same position, so what is heard
                        and what is drawn are the same samples (mirrors ShowRunner's audio-window
                        contract in raspberry-milkshake).
```

Data model (in-memory, UI thread): a `Project` (= the open library folder) owns `Collection` objects; each `Collection` owns `Track`s; each `Track` owns an ordered `Cue` list. These mirror `rm_core`'s `Collection`/`Track`/`Cue` structs directly (section 4) — Shaker edits those structs (or a thin QML-friendly wrapper around them) and hands them to the submoduled parser/validator/writer, rather than inventing a parallel model that could drift from the format.

## 4. Data model and file layout
Unchanged from `show-format.md`; Shaker reads and writes exactly that schema via `rm_core::parse_manifest` / a new `rm_core::write_manifest` (to be added to the submodule build — see below) and validates via `rm_core::check_files` plus the parser's own diagnostics.

```
library/
  <collection-id>/
    collection.toml
    cover.jpg
    01-intro.flac        # album: audio next to the manifest
presets/
  tags.toml              # new in this spec: see section 10
  cream-of-the-crop/...
.shaker/                 # Shaker's own cache, gitignorable, never read by the Pi
  cache/
    presets.json          # last scan: per-preset load ok/fail, mtime+size fingerprint
    waveforms/<hash>.bin   # cached peak data per audio file, for fast redraw
```

**`write_manifest` is new.** `rm_core` today only parses (`manifest.cpp`). Shaker needs a TOML *writer* that round-trips a `Collection` back into the documented layout (field order matching the examples, so hand-diffing stays readable). This is added to `rm_core` in the submodule (a small, format-only addition, no engine/UI implications) so both repos share it — otherwise Shaker's writer and the format documentation could drift apart independently of the parser. It does not need to preserve comments (documented limitation, section 11).

## 5. Preset library and scanning
- Point Shaker at a `presets/` folder (same folder that will be copied to the Pi). Show it as a tree mirroring the on-disk structure (this *is* what `dir:` pools address) with a flat search/filter over the full path.
- **Scan pass**: for each `.milk` file, attempt a headless load with the desktop-GL `PreviewHost` (silence in, one rendered frame) and record ok/fail plus its current `fRating` (read via the submoduled `preset_rating.hpp`, no duplicated parsing logic). Cache results in `.shaker/cache/presets.json` keyed by a (size, mtime) fingerprint so an unchanged file is not re-probed. Mirrors the *pattern* `raspberry-milkshake`'s render tests already established for this exact check.
- **Rating**: shown as 0–5 buttons on the selected preset, identical UI idea to the running appliance's dashboard. Writes go straight through `rm_core::DirCatalog::set_rating` (or the lower-level `write_rating`), so the on-disk effect is byte-identical to what the Pi's own tooling produces — same in-place edit, same atomic replace.
- **Live preview**: selecting a preset renders it in the preview pane. Default audio feed is a short built-in test tone loop; if the timeline has a track loaded and a playhead position, feed that position's real audio instead so the preset is judged against the actual song (a toggle, since sometimes you want to judge a preset in isolation).

## 6. Audio import and preparation
Replaces the design spec's `rm-import`. Pipeline per imported file:
1. **Decode** the source (whatever format) to floating-point PCM.
2. **Resample** to 48 kHz if needed. Channel layout: pass stereo and 5.1 through; anything else (mono, quad, 7.1, ...) is downmixed to stereo with a warning (the format only supports stereo or 5.1, per `channel_map.hpp`'s `Layout`).
3. **Loudness-normalize**: measure integrated loudness (EBU R128) and apply gain toward **−14 LUFS integrated** (a documented default, matching common streaming-service targets; exposed as a project setting, not hard-coded). Show the measured value and applied gain; allow a manual trim before committing.
4. **Encode** to FLAC at 48 kHz.
5. **Beat grid**: run onset/tempo detection for an initial `bpm` guess and a first-downbeat guess for `downbeat_offset`. Overlay this as a grid on the waveform. The user can: type an exact BPM, tap a "tap tempo" button, or drag the first-downbeat marker. None of this needs to be exact — cue placement can always be typed as absolute time instead of musical time.
6. The result becomes a `Track` entry (`file`, `title`, `bpm`, `beats_per_bar`, `downbeat_offset`), ready for cues.

5.1 sources: the waveform display is always a stereo downmix (for visual reference only); the encoded FLAC keeps all six channels.

## 7. Timeline and cue editing
- One row pair (waveform + cue lane) per track. An album with multiple tracks shows them stacked or as tabs (implementation's choice; either satisfies "watch the whole show," since only one track plays at a time anyway, matching the Pi engine's own model, section 14 of `design-spec.md`).
- **Cue-lane blocks** span `[cue.at, next_cue.at)` (or track end for the last one). An explicit-preset block shows the preset name; a pool block shows the pool expression (`tag:edm`, `dir:ambient`, `all`) with a visually distinct (e.g. striped) fill, since the actual preset varies at runtime — never claim a pool block "is" a specific preset.
- **Adding a cue**: drag a preset from the browser onto the timeline (creates an explicit-preset cue at the drop position), or click an empty spot and fill in the inspector. Snap-to-beat-grid and snap-to-existing-cue are independent toggles.
- **Moving/trimming**: drag a block's edges. The UI enforces the format's own invariant (`show-format.md` "Cues" table: strictly increasing `at`) live — dragging past a neighbor either stops at it or pushes/swaps, implementation's choice, but it must never let you leave two cues at the same or reversed times.
- **Inspector** (for the selected cue): `at` (text field accepting both grammars — see below — plus a "set to playhead" button), `preset` XOR `pool` (radio choice), pool sub-fields (`all` / `tag:<name>` — disabled/greyed until section 10 lands / `dir:<folder>`, the last via a picker into the preset tree so it is always a valid relative path), `every` (enabled only when `pool` is chosen), `transition` (`cut` or `blend:<seconds>`, defaulting to the collection's `defaults.transition` when left unset).
- **Time entry** accepts and displays exactly the grammar in `show-format.md` §"Time grammar": `M:SS.fff`, `H:MM:SS`, `<n>s`, `bar N`, `bar N beat B`. A toggle switches the *display* between absolute and musical without changing the stored value's meaning.
- **Preview seed**: a visible "seed" control (numeric, defaulting to the stable title hash the format itself defaults to) with a "re-roll" button that changes it and re-simulates pool choices (via the submoduled `Sequencer`, so the simulation is identical to what the Pi will compute) so the user can audition a few realizations of a pool-heavy show before exporting.
- **Playback**: transport controls (play/pause, scrub) drive both the audio output and the `PreviewHost`, fed from the same decode position (section 3). Seeking the playhead seeks both, matching `ShowRunner::seek`'s contract in `raspberry-milkshake` (load whatever cue is active at the new position, cut, no blend).

## 8. Preview/render host (desktop)
`PreviewHost` (new, Shaker-only; not shared with `rm_render`, see section 2):
- Embedded via a `QQuickFramebufferObject` subclass; projectM renders into that FBO on Qt Quick's render thread, matching `rm_render::ProjectMHost`'s finding that projectM 4.1.7 always draws into the *current* default framebuffer (`design-spec.md` §4 "Output constraint") — so the FBO must be bound and current when projectM renders, exactly as `ProjectMHost::render()` already documents.
- Same public shape as `ProjectMHost` (`load_preset`, `add_pcm_stereo`, `render`, `read_frame`) is a reasonable internal API to copy by hand (it is small and GL-context-specific enough that sharing the binary wouldn't help), so anyone familiar with `raspberry-milkshake` recognizes it immediately.
- Preset load failures surface as a toast/inline message using projectM's own failure callback (`projectm_set_preset_switch_failed_event_callback`), the same mechanism `ProjectMHost` already wraps.

## 9. Validation and export
- **Validate** (available at any time, and always run before export): calls the submoduled `rm_core::parse_manifest` on the in-memory model's serialized form and `rm_core::check_files` for the collection directory, and lists every diagnostic (path + message, error vs. warning) in a panel; clicking one selects the offending track/cue in the timeline or the offending top-level field in the metadata form.
- **Export**: writes `collection.toml` (via the new `write_manifest`, section 4), copies/writes the cover and the prepared audio into `library/<id>/`. Export refuses to write if validation reports any error (warnings are shown but do not block), so Shaker can never hand the Pi a file `rm-validate` would reject.
- **Round-trip**: opening an existing, hand-written collection (e.g. `examples/collections/night-drive` from `raspberry-milkshake`) and re-exporting it without changes must produce a file that still validates and is semantically identical (field values equal; exact byte-for-byte formatting, comments and key order are not guaranteed — no common TOML writer preserves comments, and this is an accepted, documented limitation).

## 10. Preset tags (`tag:` pools) — new design, cross-repo follow-up required
`show-format.md` says `tag:` pools use "presets tagged by `rm-bench`," but **no tag storage format exists anywhere today**, and `raspberry-milkshake`'s `DirCatalog::resolve()` (`src/core/dir_catalog.cpp`) already special-cases `PoolExpr::Kind::Tag` to always return an empty list — `tag:` pools are silently inert right now.

**Proposed format**, owned by this spec since Shaker is what will write it: a single sidecar file at the root of the presets folder, `presets/tags.toml`, mapping each preset's path (relative to that folder, forward slashes, exactly as `DirCatalog` already keys everything) to a tag list:
```toml
# presets/tags.toml — written by Shaker, read by raspberry-milkshake's DirCatalog.
# Presets with no entry have no tags.

["cream-of-the-crop/Fractal/Pointy/suksma - do what thou won't.milk"]
tags = ["edm", "trippy"]

["cream-of-the-crop/Dancer/Wake/suksma - yin - 395.milk"]
tags = ["chill", "slow"]
```
A sidecar (rather than a field embedded in the `.milk` file, the way ratings are) is deliberate: `fRating` is reused because it is *already* a real MilkDrop field that other tools understand and preserve; tags have no such precedent, and community preset packs get re-downloaded/updated wholesale, which would silently wipe any tags embedded in the files themselves. A sidecar the two tools jointly own survives that.

**This is only half the feature.** Shaker can write `tags.toml` and use it locally to power its own `tag:` filters in the browser, but for `tag:` pools to actually work on the Pi, `raspberry-milkshake`'s `DirCatalog` needs a follow-up change (not part of this spec, not built here) to read the same file and populate `Kind::Tag` results. Until that lands, Shaker should grey out or clearly label `tag:` pool authoring as "not yet effective on the Pi" rather than silently letting the user build a show around a pool that resolves to nothing.

## 11. Acceptance criteria (v1 / MVP)
**Library and collections**
- [ ] Open a library folder matching `show-format.md`'s layout; browse existing collections as a cover grid; create a new album, playlist or interactive collection.
- [ ] Edit collection metadata: title, artist, tags, cover (import + crop to square, generate 256²/512² thumbnails), default transition.

**Audio import and prep**
- [ ] Import raw audio files into an album's track list; transcode to 48 kHz FLAC; already-conforming FLAC passes through unchanged.
- [ ] Loudness-normalize toward the configured LUFS target, with the measurement and applied gain visible and adjustable before commit.
- [ ] Auto-detect BPM and downbeat, correctable by typing, tap-tempo, or dragging the marker; stored as the track's `bpm`/`beats_per_bar`/`downbeat_offset`.

**Preset browser**
- [ ] Browse a presets folder as a tree/grid matching its on-disk structure, with search.
- [ ] Live-preview a selected preset (test tone, silence, or the timeline's current audio).
- [ ] View and edit each preset's rating (0–5) in place; 0-rated presets are visibly marked as excluded from pools.
- [ ] A scan pass flags presets that fail to load.

**Timeline**
- [ ] Waveform + cue lane per track; add/move/trim/delete cues by dragging; explicit-preset and pool cues are visually distinct.
- [ ] Cue inspector covering every field in `show-format.md`'s Cues table (`at`, `preset` xor `pool`, `every`, `transition`), with both time grammars supported for `at`.
- [ ] Play/scrub with synchronized live preview; seeking the playhead seeks the preview.
- [ ] Pool-cue preview re-rolls the seed and re-simulates via the submoduled `Sequencer`.

**Playlists and interactive**
- [ ] Author a playlist by picking tracks from other collections in the library (no audio duplication).
- [ ] Author an interactive collection via a simple form (`preset_pool`, `preview.preset`, tags, cover).

**Preview field**
- [ ] Set an album/playlist's `preview` by drag-selecting a region on the waveform; set an interactive collection's `preview.preset` from the browser.

**Validation and export**
- [ ] Validate on demand, using the exact shared parser/validator, with clickable diagnostics.
- [ ] Export writes `collection.toml` + cover + audio into the library folder; refuses on any validation error.
- [ ] Round-trips an existing hand-written collection (semantically; comments/formatting excepted).

**Non-functional**
- [ ] Runs on Windows; no Pi/ARM/EGL dependency; fully local, no network requirement.

## 12. Backlog (v2+, not required for v1)
- Full `rm-bench`-style timing (load / first-frame / steady-frame) rather than pass/fail-only scanning; importing an `rm-bench` report generated on the Pi to annotate the browser with real hardware numbers.
- `tag:` pool authoring becoming *effective*, once the `raspberry-milkshake` `DirCatalog` follow-up (section 10) lands.
- Undo/redo history for timeline edits.
- Multi-select batch operations (rate or tag many presets at once).
- Reusable cue templates/patterns ("apply this 8-bar pool pattern here").
- Direct "send to Pi" (rsync/scp) instead of a manual copy step.
- Cross-platform builds (Linux/macOS) beyond "should work, given Qt" — not verified in v1.

## 13. Open questions to revisit
- **Loudness target**: −14 LUFS integrated is proposed as the default; confirm against whatever the Pi's HDMI/AVR chain actually expects once that is measured (design-spec.md §3 A/V calibration territory, not this spec's problem to solve, but worth a sanity check later).
- **Beat-detection library choice**: left open (any maintained onset/tempo library); pick one when implementation starts, informed by what is easiest to integrate into the chosen build system on Windows.
- **Preset-library drift**: Shaker's local `presets/` folder might not exactly match what ends up on the Pi. No software fix proposed here; document the operational advice (copy the whole `presets/` folder, including `tags.toml`, not just the files referenced by one show) prominently in Shaker's own UI/README.
- **Single-track vs. tabbed timeline layout for multi-track albums** (section 7): left as an implementation choice; revisit once the UI exists and the "movie editor" feel can actually be judged.
