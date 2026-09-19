#!/usr/bin/env bash
# Lists the presets rated 0 (hidden from random picks), one per line, relative to the presets folder.
# Runs inside the dev container, where the presets volume is mounted at /presets.
set -uo pipefail
cd /presets || exit 1
grep -rlE --include='*.milk' '^fRating=0(\.0*)?[[:space:]]*$' . | sed 's|^\./||' | sort
exit 0
