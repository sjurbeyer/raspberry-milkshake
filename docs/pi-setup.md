# Setting up the Pi for development

One-time steps to get from a bare SD card to a Raspberry Pi you can build, test and iterate on directly — no SD re-flash between changes, no GitHub needed. Once this is done, the day-to-day loop is: edit on Windows → commit → `just pi-sync` → `just pi-build` → `just pi-test` or `just pi-run`.

This intentionally does **not** cover the touch display, LEDs, or audio interface wiring — get the Pi itself talking over SSH first, with a network connection and the OS installed; add the peripherals once that works, per the checklist in `docs/design-spec.md` §10 (Phase 0b).

## 1. Flash the SD card

Use [Raspberry Pi Imager](https://www.raspberrypi.com/software/) on this machine, with the **Kingston 64 GB** card (§1.1 of the design spec).

- **OS**: Raspberry Pi OS **Lite**, 64-bit — no desktop environment. This matches the design (Qt renders straight to the display via EGLFS/KMS, with no compositor to fight with) and it's what the appliance will actually run.
- Before writing, click the gear icon (or press <kbd>Ctrl+Shift+X</kbd>) to open **OS customisation** and set:
  - **Hostname** — e.g. `raspberrymilkshake` (this doc assumes that; the `justfile`'s `pi_host` defaults to `raspberrymilkshake.local`, override with `PI_HOST` if you pick something else).
  - **Username and password** — pick a username (this doc assumes `pi`; override with `PI_USER` if you pick another) and a password (still useful as a fallback even once key auth works).
  - **Enable SSH** → "Allow public-key authentication only", and paste your public key (see step 2 first if you don't have one yet — you can come back to this).
  - **Wi-Fi**, if you're not using Ethernet — but prefer Ethernet for development: it's more reliable for the rsync-free `git push` sync this doc sets up, and one less thing to debug later.
- Write the image, then boot the Pi with it inserted.

## 2. An SSH key (if you don't already have one)

In PowerShell on this machine:
```powershell
Test-Path ~\.ssh\id_ed25519.pub
```
If that's `False`, generate one:
```powershell
ssh-keygen -t ed25519 -C "raspberry-milkshake dev"
```
Accept the default location and an empty passphrase is fine for a dev key. Then `Get-Content ~\.ssh\id_ed25519.pub` and paste that into the Imager's OS customisation screen (step 1) before writing the card. If you've already written the card, add it after first boot instead:
```powershell
Get-Content ~\.ssh\id_ed25519.pub | ssh pi@raspberrymilkshake.local "mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys"
```
(That one command will ask for the password you set in step 1 — the last time it should.)

## 3. Find it and connect

Give it a minute to boot, then:
```powershell
ssh pi@raspberrymilkshake.local
```
If `raspberrymilkshake.local` doesn't resolve (mDNS over Windows is occasionally flaky), check your router's admin page for a device named `raspberrymilkshake` and use its IP address instead — either directly (`ssh pi@192.168.1.x`) or by setting `$env:PI_HOST = "192.168.1.x"` before running any `just pi-*` command.

You should land at a shell prompt with no password asked (key auth). If a password prompt appears, the key wasn't picked up — recheck step 2.

## 4. Install build dependencies

```powershell
just pi-bootstrap
```
This installs the same package list `Dockerfile` uses for the dev container (Qt 6, EGL/GLES dev headers, CMake, Ninja — same Debian Trixie base as Raspberry Pi OS, so the versions line up). Takes a few minutes; safe to re-run later if that list grows.

## 5. Set up the working repo on the Pi

```powershell
just pi-init-repo
```
This creates `~/raspberry-milkshake` on the Pi as a git repository configured to update its own working tree on every push (`receive.denyCurrentBranch=updateInstead` — no bare repo, no GitHub, no rsync), and adds it as a `pi` remote in your local repo here. One-time; safe to re-run (does nothing if it already exists).

## 6. The day-to-day loop

```powershell
just pi-sync          # push the current commit to the Pi (working tree included)
just pi-build         # cmake --preset pi && cmake --build --preset pi, natively, on the Pi
just pi-test          # ctest --preset pi
just pi-test-label render   # just one label: unit, sequencer, render, ui or app
just pi-run --headless      # run rm-app directly on the Pi's own screens
just pi-shell          # a plain shell in ~/raspberry-milkshake, for poking around
```
**Only committed changes sync.** `pi-sync` pushes `HEAD`, not your working tree — commit before syncing, even a throwaway `git commit -am "wip"` is completely fine and matches how this project has been developed all along (small, frequently-committed steps). Nothing here ever pushes to a real remote; `pi` is just another local-style git remote that happens to be over SSH.

`pi-run` shows output on the Pi's **real** screens (HDMI main output, DSI touch panel) — there's nothing to look at over SSH itself; watch the physical displays, and `Ctrl+C` in the SSH session stops it. Logs still print to that same terminal.

## Troubleshooting
- **`ssh: connect to host ... port 22: Connection refused` / times out**: the Pi hasn't finished booting yet, or is on a different network than expected. Give it another minute; check the router's DHCP client list if it's still unreachable.
- **A password prompt when you expected key auth**: your public key isn't in `~/.ssh/authorized_keys` on the Pi. Redo step 2's `ssh-copy-id`-style command.
- **`sudo` inside `pi-bootstrap` asks for a password**: Raspberry Pi OS's default user has passwordless `sudo` out of the box; if you changed that, either restore it or run `tools/pi/bootstrap.sh`'s contents by hand once over an interactive `ssh -t` session.
- **`pi-build` fails to find Qt6 or a package**: `just pi-bootstrap` again — the package list occasionally grows as the project does (check `Dockerfile` for the current list).
