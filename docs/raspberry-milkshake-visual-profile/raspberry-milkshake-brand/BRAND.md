# raspberry milkshake — visual direction

An audio-reactive identity for the Raspberry Pi / ProjectM visualizer: sweet, spatial, and technically crisp. The personality is **a milkshake caught inside an oscilloscope**—gentle at rest, liquid and distorted in motion. The selected mark is a three-lobed raspberry vortex around a milk-white core.

## Logo

The mark is a three-lobed liquid vortex with a milk-white centre. It suggests a shake, a spinning Pi-like fruit, a speaker cone, and a MilkDrop vortex without depending on a literal glass or fruit illustration.

- Use the full mark in app headers, startup screens, and projection slates.
- Use the icon alone for favicons, compact controls, and LED/projector calibration screens.
- Let the raspberry vortex rotate slowly (6–10 s) or pulse its scale to bass.
- At tiny sizes, use the dark-tile version; remove highlights before reducing the size of the core.

The geometry is deliberately made from three broad paths and a centre circle, making it easy to animate as SVG, Lottie, WebGL, or a shader mask.

## Palette

| Role | Hex | Use |
|---|---:|---|
| Midnight canvas | `#18181C` | App background; close to VS Code Dark without being blue-black |
| Raised surface | `#222228` | Panels, inspector, playback bar |
| Quiet surface | `#2B2B32` | Hover, selected groups, graph grid |
| Hairline | `#3D3F46` | Borders and dividers |
| Soft white | `#F2F3F5` | Primary type and the milkshake glass |
| Muted text | `#9DA1AA` | Secondary labels, inactive controls |
| Raspberry | `#E84F8C` | Primary action, live audio, logo depth |
| Raspberry milk | `#FFB8D2` | Highlights, active outline, accent type |
| Signal blue | `#7AA2F7` | Technical/monitoring information only |

Use pink as a signal, not as a wash. Most of the interface remains dark charcoal, white, and neutral gray; the milkshake color feels more special against that restraint.

## Interface profile

- **Surfaces:** deep charcoal planes with 1 px `#3D3F46` borders. Avoid drop shadows; elevation is communicated by a slightly lighter surface and a hairline.
- **Corners:** 6 px for standard controls, 10 px for large floating panels. The logo is rounder than the UI so it feels alive inside a precise instrument.
- **Type:** Inter / system sans. Use 13 px medium for controls, 12 px regular for metadata, 18–22 px semibold for section titles. Lowercase names reinforce the calm, friendly tone.
- **Controls:** quiet icon buttons, turning raspberry only when armed, playing, recording, or selected. Use a 2 px `#FFB8D2` focus ring for keyboard navigation.
- **Visual canvas:** treat the projected visualization as a boundaryless stage. Its surrounding control frame is a thin neutral line with a tiny raspberry live dot, rather than a heavy chrome.

## Motion language

- **Base motion:** viscous, continuous, slightly delayed—like liquid responding to sound. Prefer sine easing and 180–360 ms UI transitions.
- **Audio response:** bass enlarges the spiral by 2–5%; mids twist it; highs introduce brief white glints. Do not flash the whole UI.
- **MilkDrop influence:** allow the raspberry stroke to trail, warp, and briefly split during preset transitions, then settle back into the clean spiral.
- **Reduced motion:** freeze the glass and show only a low-contrast breathing raspberry dot for playback state.

## Example component states

| Element | Rest | Active / live |
|---|---|---|
| Play button | `#222228` surface, white icon | Raspberry fill, midnight icon |
| Selected preset | 1 px gray border | 2 px milk-pink border + subtle `#2B2B32` fill |
| Spectrum / meters | neutral gray | Raspberry-to-milk gradient; blue reserved for calibration |
| Projector output | clean edge | small raspberry dot + `LIVE` label |

## Do / avoid

- Do keep the mark flat and high-contrast on a dark field.
- Do use the swirl as a clipping mask or distortion source in animated visuals.
- Avoid literal fruit seeds, photorealistic shake imagery, glossy UI gradients, or red/green Raspberry Pi mimicry.
- Avoid filling every surface with pink—the visualizer should feel like a dark room lit by raspberry light.
