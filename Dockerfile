# Dev image: same Debian base as Raspberry Pi OS. Source is bind-mounted, not copied.
# Mesa's llvmpipe provides software OpenGL ES 3 so projectM can run headless without a GPU.
FROM debian:trixie-slim AS dev

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build git ca-certificates pkg-config \
      libegl-dev libgles-dev libegl1 libgles2 libegl-mesa0 libgl1-mesa-dri libgbm1 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
CMD ["bash"]
