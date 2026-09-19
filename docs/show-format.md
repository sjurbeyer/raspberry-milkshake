# Collection format (`collection.toml`), format version 1

A **collection** is a directory containing `collection.toml`, a cover image, and (for albums) audio files. Albums, playlists and the interactive visualization share one format, selected by `type`. Check a collection with:

```
rm-validate [--no-check-files] <collection-dir>
```
Exit code 0 means valid (warnings allowed), 1 means validation errors, 2 means usage or I/O problems. The reference implementation is `src/core/manifest.cpp`; a complete example is `examples/collections/night-drive/collection.toml`.

## Library layout
```
library/
  night-drive/           album: audio lives next to the manifest
    collection.toml
    cover.jpg
    01-intro.flac
  sunday-mix/            playlist: references audio in other collections
    collection.toml
    cover.jpg
  interactive/           the "Interactive visualization" tile
    collection.toml
presets/                 .milk files; referenced by relative path
```
All paths in a manifest are relative and must not contain `..`, a leading `/` or `\`, or `:`. Playlist `from` paths resolve against the library root (the parent of the collection directory).

## Top-level keys
| Key | Type | Notes |
|---|---|---|
| `format` | int, required | Must be `1`. |
| `type` | string, required | `"album"`, `"playlist"` or `"interactive"`. |
| `title` | string, required | Non-empty. |
| `artist` | string | Optional. |
| `tags` | array of strings | Drive the tag filter (for example `pleasing`, `edm`, `challenging`). |
| `cover` | string | Square image. Required for albums and playlists; optional for interactive (a warning, and default artwork is used). |
| `seed` | int | Seeds pool selection. Default: a stable hash of the title, so a show replays identically. |
| `defaults.transition` | string | Default transition (see below). Default `"blend:2.0"`. |
| `preview` | table | See below. |
| `preset_pool` | string | Interactive only; required there. Pool expression. |
| `[[track]]` | array of tables | Required (at least one) for albums and playlists; not allowed for interactive. |

Unknown keys produce a warning and are ignored.

## Tracks
| Key | Type | Notes |
|---|---|---|
| `file` | string | Albums only, required. 48 kHz FLAC or Opus, stereo or 5.1. |
| `from` | string | Playlists only, required. `"<collection>/<file>"`. |
| `title` | string | Optional. |
| `bpm` | number | In (0, 400]. Needed for any `bar ...` time or `bars`/`beats` duration on this track. |
| `beats_per_bar` | int | 2 to 12, default 4. |
| `downbeat_offset` | string | Absolute time of bar 1, beat 1. Default `"0:00"`. |
| `[[track.cue]]` | array of tables | Cues, in strictly increasing time order. |

The tempo is fixed per track (no tempo map in v1).

## Cues
Each cue sets what is shown from time `at` until the next cue.

| Key | Type | Notes |
|---|---|---|
| `at` | string, required | Time (grammar below). |
| `preset` | string | Path to a `.milk` file. |
| `pool` | string | Pick a preset from a pool instead. |
| `every` | string | Only with `pool`: re-pick at this interval until the next cue or the end of the track. |
| `transition` | string | Overrides the default for this cue. |

Exactly one of `preset` or `pool` is required. A warning is issued if the first cue is after the start of the track (nothing is set before it).

### Time grammar
- Absolute: `"0:45.500"` (M:SS.fff), `"1:02:03"` (H:MM:SS), `"12.5s"`. Seconds are in [0, 60) in clock form.
- Musical: `"bar 17"` (start of bar 17) or `"bar 17 beat 3.5"` (bars are 1-based; the beat is 1-based, may be fractional, and lies in [1, `beats_per_bar` + 1)).
- Durations (for `every`): `"8 bars"`, `"16 beats"`, `"30s"`. They must be positive.

All times are stored as sample positions at 48 kHz, so scheduling is sample-accurate.

### Transitions
`"cut"` or `"blend:<seconds>"`, where seconds is in (0, 60].

### Pool expressions
`"all"`, `"tag:<name>"` (presets tagged by `rm-bench`), or `"dir:<relative dir>"`. Selection is deterministic for a given `seed` and never repeats the previous choice when the pool has more than one preset.

## Preview
The preview plays while browsing in the swipe view, but only when nothing else is playing.

- Albums and playlists: `start` (absolute time) and `length` (duration) are required; `track` (1-based, default 1) and `preset` are optional.
- Interactive: only `preset` (required); there is no audio.

```toml
preview = { track = 1, start = "0:45", length = "20s", preset = "presets/flow.milk" }
```

## Interactive collections
```toml
format      = 1
type        = "interactive"
title       = "Interactive visualization"
tags        = ["challenging"]
preset_pool = "tag:reactive"
preview     = { preset = "presets/flow.milk" }
```
