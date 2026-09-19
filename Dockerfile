# Dev image: same Debian base as Raspberry Pi OS. Source is bind-mounted, not copied.
FROM debian:trixie-slim AS dev

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build git ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
CMD ["bash"]
