# Dev image: same Debian base as Raspberry Pi OS. Source is bind-mounted, not copied.
# Mesa's llvmpipe provides software OpenGL ES 3 so projectM can run headless without a GPU.
# Qt 6.8 matches what Raspberry Pi OS Trixie ships. novnc/websockify show the touch UI in a browser.
FROM debian:trixie-slim AS dev

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build git ca-certificates pkg-config \
      libegl-dev libgles-dev libegl1 libgles2 libegl-mesa0 libgl1-mesa-dri libgbm1 \
      qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools qt6-svg-dev libqt6svg6 qt6-qpa-plugins \
      qml6-module-qtqml qml6-module-qtqml-workerscript qml6-module-qtquick qml6-module-qtquick-controls \
      qml6-module-qtquick-templates qml6-module-qtquick-layouts qml6-module-qtquick-window \
      qml6-module-qttest fonts-inter \
      novnc websockify \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
CMD ["bash"]
