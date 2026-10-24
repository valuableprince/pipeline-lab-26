#!/usr/bin/env bash
# One-time transfer of the current committed source snapshot (no old Git history).
# Nothing is pushed automatically. Inspect the audit before publishing.
set -euo pipefail
ROOT=$(git rev-parse --show-toplevel)
cd "$ROOT"
if [[ -n "$(git diff --cached --name-only)" ]]; then
  echo "Staged changes exist. Unstage them first with: git restore --staged ." >&2
  exit 2
fi
SOURCE="https://github.com/valuableprince/open_mp_Server.git"
REVISION="b03c12329428b7713b8384929f3e78cd7b2755a3"
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
git clone --quiet --no-checkout "$SOURCE" "$tmp/source"
git -C "$tmp/source" checkout --quiet "$REVISION"
actual=$(git -C "$tmp/source" rev-parse HEAD)
[[ "$actual" == "$REVISION" ]] || { echo "Unexpected source commit" >&2; exit 1; }
backup=$(mktemp -d "${TMPDIR:-/tmp}/source-backup.XXXXXXXX")
echo "Local preexisting folders will be saved under: $backup"
for dir in gamemodes db tests config; do
  if [[ -e "$ROOT/$dir" ]]; then mv -- "$ROOT/$dir" "$backup/$dir"; fi
done
for dir in gamemodes db tests; do
  [[ ! -d "$tmp/source/$dir" ]] || cp -R -- "$tmp/source/$dir" "$ROOT/$dir"
done
mkdir -p config
for file in "$tmp/source"/config/*.example "$tmp/source"/config/*.example.cfg "$tmp/source"/config/*.ini.example; do
  [[ ! -f "$file" ]] || cp -- "$file" config/
done
mkdir -p tools
for file in "$tmp/source"/tools/*.py; do
  [[ ! -f "$file" ]] || cp -- "$file" tools/
done
echo "Review paths, credentials and project-identifying text locally."
find gamemodes db tests config tools -type f \( -name '*.pem' -o -name '*.key' -o -name '.env*' -o -name '*.amx' \) -print
git add -- gamemodes db tests config
for file in "$tmp/source"/tools/*.py; do
  [[ ! -f "$file" ]] || git add -- "tools/$(basename "$file")"
done
git diff --cached --stat
echo "DO NOT PUSH until the content and secrets review is complete."
echo "Backup of prior local folders: $backup"
