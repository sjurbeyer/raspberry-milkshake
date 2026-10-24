# Raspberry Milkshake: everyday commands. Run `just` to list them.
# Everything builds and runs in Docker, so this machine only needs Docker Desktop and `just`.

set windows-shell := ["pwsh.exe", "-NoLogo", "-NoProfile", "-Command"]

# How long every track plays until real audio exists. Try: just track_seconds=6 dashboard
track_seconds := "60"
# What the main output shows while paused: hold, fade or home. Try: just pause_display=fade dashboard
pause_display := "hold"
export RM_APP_ARGS := "--track-seconds " + track_seconds + " --pause-display " + pause_display

dashboard_url := "http://localhost:8080"

# List the commands
default:
    @just --list

# Start the dashboard (touch UI, main output, LED chain) and open it in the browser
dashboard: presets
    docker compose up -d --wait --wait-timeout 900 dashboard
    @just open
    @echo "Dashboard: {{dashboard_url}} (touch UI on port 6080). Stop it with: just stop"

# Open the dashboard in the browser
[windows]
open:
    Start-Process {{dashboard_url}}

[linux]
open:
    xdg-open {{dashboard_url}}

[macos]
open:
    open {{dashboard_url}}

# Follow the dashboard's log
logs:
    docker compose logs -f --tail 50 dashboard

# Stop the dashboard
stop:
    docker compose stop dashboard

# Restart the dashboard (rebuilds first, so it picks up code changes)
restart:
    docker compose restart dashboard

# Download the community preset pack, once (does nothing if it is already there)
presets:
    docker compose run --rm fetch-presets

# List the presets rated 0 (hidden from random picks); change a rating with the buttons on the dashboard
hidden-presets:
    docker compose run --rm dev bash tools/dev/list-hidden-presets.sh

# Build and run every test
test:
    docker compose run --rm test

# Run one label of tests: unit, sequencer, render, ui or app
test-label label:
    docker compose run --rm dev bash -c "cmake --preset container && cmake --build --preset container && ctest --preset container -L {{label}}"

# Check a collection's collection.toml
validate collection="examples/collections/night-drive":
    docker compose run --rm dev bash -c "cmake --preset container && cmake --build --preset container --target rm-validate && /build/rm-validate --no-check-files {{collection}}"

# A shell inside the dev container (source in /src, build tree in /build)
shell:
    docker compose run --rm dev

# Rebuild the dev image (after changing the Dockerfile)
image:
    docker compose build dev

# Forget the compiled shaders (presets stutter on their first use again until it refills)
forget-shaders:
    docker compose down
    docker volume rm raspberry-milkshake_shadercache

# Throw away the build cache volume (the next build starts from scratch; presets are kept)
clean-build:
    docker compose down
    docker volume rm raspberry-milkshake_build

# --- Raspberry Pi: build and test on the real hardware -----------------------------------
# One-time setup: docs/pi-setup.md. Override the host/user with PI_HOST/PI_USER, e.g.
# `just pi_host=192.168.1.50 pi-sync`.
pi_host := env_var_or_default("PI_HOST", "raspberrymilkshake.local")
pi_user := env_var_or_default("PI_USER", "pi")

# Install build dependencies on the Pi (once; safe to re-run after Dockerfile's list changes)
pi-bootstrap:
    scp tools/pi/bootstrap.sh {{pi_user}}@{{pi_host}}:/tmp/rm-bootstrap.sh
    ssh -t {{pi_user}}@{{pi_host}} "bash /tmp/rm-bootstrap.sh && rm /tmp/rm-bootstrap.sh"

# Create the working repo on the Pi and point this repo's "pi" remote at it (once)
pi-init-repo:
    scp tools/pi/init-repo.sh {{pi_user}}@{{pi_host}}:/tmp/rm-init-repo.sh
    ssh {{pi_user}}@{{pi_host}} "bash /tmp/rm-init-repo.sh && rm /tmp/rm-init-repo.sh"
    git remote add pi {{pi_user}}@{{pi_host}}:raspberry-milkshake/.git

# Push the current commit to the Pi (updates its working tree too); commit before syncing
pi-sync:
    git push pi HEAD:refs/heads/main -f

# Configure and build on the Pi (native arm64)
pi-build:
    ssh {{pi_user}}@{{pi_host}} "cd raspberry-milkshake && cmake --preset pi && cmake --build --preset pi"

# Run every test on the Pi
pi-test:
    ssh {{pi_user}}@{{pi_host}} "cd raspberry-milkshake && ctest --preset pi"

# Run one label of tests on the Pi: unit, sequencer, render, ui or app
pi-test-label label:
    ssh {{pi_user}}@{{pi_host}} "cd raspberry-milkshake && ctest --preset pi -L {{label}}"

# Run rm-app on the Pi's own real screens; extra args pass through, e.g. `just pi-run --headless`
pi-run *args:
    ssh -t {{pi_user}}@{{pi_host}} "cd raspberry-milkshake && ./build/rm-app {{args}}"

# A shell on the Pi, in the synced working tree
pi-shell:
    ssh -t {{pi_user}}@{{pi_host}} "cd raspberry-milkshake && exec bash -l"
