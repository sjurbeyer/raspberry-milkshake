#!/usr/bin/env bash
# Creates an empty git repository on the Pi that `git push` (from the dev machine) updates in
# place, working tree included — no bare repo, no hook, no GitHub. Run once via `just pi-init-repo`.
# Safe to run again: does nothing if the repo already exists.
set -euo pipefail

dir="$HOME/raspberry-milkshake"

if [ -d "$dir/.git" ]; then
  echo "$dir already has a git repo; leaving it alone."
  exit 0
fi

mkdir -p "$dir"
git -C "$dir" init --quiet -b main
# Lets a push update the checked-out branch directly, working tree included, instead of refusing.
git -C "$dir" config receive.denyCurrentBranch updateInstead
echo "initialized $dir"
