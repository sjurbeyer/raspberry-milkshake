#!/usr/bin/env bash
# Runs the whole appliance in the dev container and exposes it to a browser:
#   - rm-app: the touch UI (on Qt's VNC platform plugin, port 5900), the projectM engine, and the
#     dashboard server for the main output, LED chain and status (port 8080),
#   - websockify + noVNC so a browser can show the touch UI (port 6080).
# rm-app is restarted if it exits. Extra rm-app options can be passed in RM_APP_ARGS, for example
#   RM_APP_ARGS="--track-seconds 6" docker compose up dashboard
# Run from the repository root (docker compose does).
set -uo pipefail

cmake --preset container || exit 1
cmake --build --preset container --target rm-app || exit 1

export QT_QUICK_BACKEND=software

websockify --web /usr/share/novnc 6080 127.0.0.1:5900 &
trap 'kill $(jobs -p) 2>/dev/null || true' EXIT

while true; do
  # shellcheck disable=SC2086
  /build/rm-app -platform "vnc:size=1280x720:port=5900" --presets /presets ${RM_APP_ARGS:-}
  status=$?
  echo "rm-app exited with status ${status} (128+N means signal N); restarting"
  sleep 1
done
