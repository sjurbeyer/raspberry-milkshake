# raspberry milkshake visual profile

This folder is a drop-in visual-identity package for the spatial music visualizer. It is designed for a dark, VS Code–adjacent interface and the selected three-lobed vortex concept.

## Start here

1. Import `css/raspberry-milkshake.css` globally.
2. Use `assets/logo/raspberry-milkshake-mark.svg` for interface icons and `assets/logo/raspberry-milkshake-lockup-dark.svg` for splash/about screens.
3. Import `css/raspberry-milkshake-motion.css` only where motion is wanted.
4. Use `tokens/raspberry-milkshake.tokens.json` as the canonical source for design-token tooling.

## Asset selection

| File | Intended use |
|---|---|
| `mark.svg` | Default transparent logo; works over the dark UI and visualizer content with enough contrast. |
| `mark-dark.svg` | App icon, launcher, social preview, or any case where a fixed dark tile is preferred. |
| `mark-mono.svg` | Single-colour print, mask, low-bandwidth LED, or tinted-state use. Set `color` in CSS. |
| `lockup-dark.svg` | Startup, about, documentation, and wide layouts. |

## Logo rules

- Keep clear space around the mark equal to the diameter of the centre dot.
- Use the transparent mark at 24 px or larger; use the dark-tile asset at 16–23 px.
- Do not add glow, a drop shadow, an outer halo, or additional gradients to the UI version. The luminous treatment belongs in full-screen visualizer scenes only.
- The central dot is deliberately soft white, not pure white, to match the UI.

## Motion

The vortex can rotate slowly (one revolution in 9 s) on loading or playback. Keep the chrome still: motion belongs to the mark, the live dot, and the visualizer. Respect `prefers-reduced-motion`.

## Accessibility

`--rm-text` on `--rm-canvas` and `--rm-raspberry` on `--rm-canvas` are intended for dark mode. Do not use the pale milk colour as small body text; reserve it for focus, highlights, and larger labels.
