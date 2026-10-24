#!/usr/bin/env bash
# Run locally, outside CI. Requires age and an AGE_RECIPIENT public key.
set -euo pipefail
if [ "$#" -ne 2 ]; then
  echo "Usage: AGE_RECIPIENT=age1... bash tools/pack.sh INPUT_DIRECTORY OUTPUT.age" >&2
  exit 2
fi
: "${AGE_RECIPIENT:?Set AGE_RECIPIENT to an age public recipient key}"
command -v age >/dev/null || { echo "age command not found" >&2; exit 2; }
command -v tar >/dev/null || { echo "tar command not found" >&2; exit 2; }
input=$1
output=$2
test -d "$input"
mkdir -p "$(dirname "$output")"
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT
tar -C "$input" -cf "$tmp" .
age -r "$AGE_RECIPIENT" -o "$output" "$tmp"
echo "Encrypted payload created. Inspect its contents locally before publishing."
