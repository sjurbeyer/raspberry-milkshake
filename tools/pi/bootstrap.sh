#!/usr/bin/env bash
# Installs the packages needed to build raspberry-milkshake natively on the Pi. Run via
# `just pi-bootstrap` (piped over SSH), or copy onto the Pi and run directly. Idempotent: safe to
# run again after this list changes.
#
# Mirrors the package list in ../../Dockerfile (same Debian Trixie base as Raspberry Pi OS), minus
# the container-only pieces (novnc/websockify: the Pi drives its own real displays, nothing needs
# to be bridged to a browser).
set -euo pipefail

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build git ca-certificates pkg-config \
  libegl-dev libgles-dev libegl1 libgles2 libegl-mesa0 libgl1-mesa-dri libgbm1 \
  qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools qt6-svg-dev libqt6svg6 qt6-qpa-plugins \
  qml6-module-qtqml qml6-module-qtqml-workerscript qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-templates qml6-module-qtquick-layouts qml6-module-qtquick-window \
  qml6-module-qttest fonts-inter

echo "bootstrap done: $(cmake --version | head -1), $(g++ --version | head -1)"
