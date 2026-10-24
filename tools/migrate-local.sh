#!/usr/bin/env bash
# One-time import of tracked source files from a local checkout, without Git history.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "Usage: bash tools/migrate-local.sh /path/to/source-checkout" >&2
  exit 2
fi
root=$(git rev-parse --show-toplevel)
root=$(cd "$root" && pwd -P)
source_root=$(git -C "$1" rev-parse --show-toplevel)
source_root=$(cd "$source_root" && pwd -P)
[[ "$root" != "$source_root" ]] || { echo "Source and destination must differ" >&2; exit 2; }
cd "$root"
git diff --quiet && git diff --cached --quiet || {
  echo "Tracked changes already exist; inspect them before migration" >&2
  exit 2
}
temp=$(mktemp -d)
trap 'rm -rf -- "$temp"' EXIT
git -C "$source_root" archive --format=tar HEAD | tar -xf - -C "$temp"
paths=()
for dir in gamemodes db tests; do
  if [[ -d "$temp/$dir" ]]; then
    if [[ -e "$root/$dir" ]]; then
      echo "Keeping existing $dir/ from previous attempt"
    else
      cp -R -- "$temp/$dir" "$root/$dir"
    fi
    paths+=("$dir")
  fi
done
mkdir -p tools
for p in "$temp"/tools/*.py; do
  [[ ! -f "$p" ]] || cp -n -- "$p" "$root/tools/"
done
paths+=("tools")
if [[ -d "$temp/config" ]]; then
  mkdir -p config
  for p in "$temp"/config/*.example "$temp"/config/*.example.cfg "$temp"/config/*.ini.example; do
    [[ ! -f "$p" ]] || cp -n -- "$p" "$root/config/"
  done
  paths+=("config")
fi
if (( ${#paths[@]} == 0 )); then
  echo "No source directories found; refusing empty migration" >&2
  exit 2
fi
sensitive=$(find "${paths[@]}" -type f \( -name '*.pem' -o -name '*.key' -o -name '.env*' -o -name '*.ini' -o -name '*.amx' -o -name '*.sqlite' \) -print)
if [[ -n "$sensitive" ]]; then
  echo "Sensitive-looking files detected; inspect before continuing:" >&2
  printf '%s\n' "$sensitive" >&2
  exit 1
fi
git add -- "${paths[@]}"
echo "Source files staged. Review the entire staged diff and scan for secrets and identifying terms BEFORE publishing."
git diff --cached --stat
