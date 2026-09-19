# Raspberry Milkshake: everyday commands. Run `just` to list them.
# Everything builds and runs in Docker, so this machine only needs Docker Desktop and `just`.

set windows-shell := ["pwsh.exe", "-NoLogo", "-NoProfile", "-Command"]

# How long every track plays until real audio exists. Try: just track_seconds=6 dashboard
track_seconds := "60"
export RM_APP_ARGS := "--track-seconds " + track_seconds

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

# Throw away the build cache volume (the next build starts from scratch; presets are kept)
clean-build:
    docker compose down
    docker volume rm raspberry-milkshake_build
