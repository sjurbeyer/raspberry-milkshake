#!/usr/bin/env bash
# Starts everything the browser dashboard needs, inside the dev container:
#   - rm-touch-ui on Qt's VNC platform plugin (port 5900), restarted if it exits,
#   - websockify + noVNC so a browser can show it (port 6080),
#   - rm-devserver: main output, LED chain and status (port 8080).
# Run from the repository root (docker compose does).
set -uo pipefail

cmake --preset container || exit 1
cmake --build --preset container --target rm-devserver rm-touch-ui || exit 1

export QT_QUICK_BACKEND=software

touch_ui_supervisor() {
  while true; do
    /build/rm-touch-ui -platform "vnc:size=1280x720:port=5900" --library examples/collections
    echo "rm-touch-ui exited with status $? (128+N means signal N); restarting"
    sleep 1
  done
}
touch_ui_supervisor &
websockify --web /usr/share/novnc 6080 127.0.0.1:5900 &
trap 'kill $(jobs -p) 2>/dev/null || true' EXIT

/build/rm-devserver --presets /presets
