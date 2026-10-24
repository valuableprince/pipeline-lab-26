#!/usr/bin/env bash
# One-time migration from a local source checkout into this repository.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "Usage: bash tools/migrate-local.sh /path/to/source-checkout" >&2
  exit 2
fi
root=$(git rev-parse --show-toplevel)
root=$(cd "$root" && pwd -P)
source_root=$(git -C "$1" rev-parse --show-toplevel)
source_root=$(cd "$source_root" && pwd -P)
[[ "$root" != "$source_root" ]] || { echo "Use separate source and destination" >&2; exit 2; }
cd "$root"
[[ -z "$(git status --porcelain)" ]] || { echo "Destination has local changes" >&2; exit 2; }
[[ ! -d gamemodes && ! -d db && ! -d tests ]] || { echo "Destination already contains source files" >&2; exit 2; }
temp=$(mktemp -d)
trap 'rm -rf -- "$temp"' EXIT
git -C "$source_root" archive --format=tar HEAD | tar -xf - -C "$temp"
# Only committed code, tests, migration scripts, and example config. No documentation or old .git history.
for dir in gamemodes db tests; do
  [[ ! -d "$temp/$dir" ]] || cp -R "$temp/$dir" "$root/$dir"
done
mkdir -p tools config
for p in "$temp"/tools/*.py; do
  [[ ! -f "$p" ]] || cp "$p" "$root/tools/"
done
for p in "$temp"/config/*.example "$temp"/config/*.example.cfg "$temp"/config/*.ini.example; do
  [[ ! -f "$p" ]] || cp "$p" "$root/config/"
done
# Do not accidentally publish credentials, private keys, or generated artifacts.
if find gamemodes db tests tools config -type f \( -name '*.pem' -o -name '*.key' -o -name '.env*' -o -name '*.ini' -o -name '*.amx' -o -name '*.sqlite' \) -print | grep -q .; then
  echo "Sensitive file types found. Do not publish; review contents." >&2
  exit 1
fi
git add gamemodes db tests tools config
echo "Review all staged changes and search for credentials and identifying strings BEFORE pushing."
echo "git diff --cached --stat"
echo "git diff --cached | grep -Ei '(password|token|secret|api[_-]?key|tarkenso|open\.mp|pawn|gta|samp)'"
