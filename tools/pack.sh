#!/usr/bin/env bash
# Local-only encryption. Never place secret keys inside this repository.
set -euo pipefail
if [[ $# -ne 2 ]]; then
  echo "Usage: AGE_RECIPIENT=age1... bash tools/pack.sh INPUT_DIR OUTPUT.age" >&2
  exit 2
fi
: "${AGE_RECIPIENT:?AGE_RECIPIENT must be a public age recipient}"
command -v age >/dev/null || { echo "age not installed" >&2; exit 2; }
command -v tar >/dev/null || { echo "tar not installed" >&2; exit 2; }
[[ "$AGE_RECIPIENT" == age1* ]] || { echo "Invalid public recipient" >&2; exit 2; }
source_dir=$(cd -- "$1" && pwd -P)
output_dir=$(dirname -- "$2")
mkdir -p -- "$output_dir"
output=$(cd -- "$output_dir" && pwd -P)/$(basename -- "$2")
[[ "$output" == *.age ]] || { echo "Output must end in .age" >&2; exit 2; }
case "$output" in "$source_dir"/*) echo "Output must be outside input directory" >&2; exit 2;; esac
[[ ! -e "$output" ]] || { echo "Output already exists; refusing overwrite" >&2; exit 2; }
umask 077
part=$(mktemp "${output}.tmp.XXXXXXXX")
trap 'rm -f -- "$part"' EXIT
# Stream the tar data through age; no temporary unencrypted tar is written.
tar -C "$source_dir" -cf - . | age -r "$AGE_RECIPIENT" -o "$part"
[[ -s "$part" ]] || { echo "Empty encrypted output" >&2; exit 1; }
mv -- "$part" "$output"
trap - EXIT
echo "Encrypted package created. Check its metadata and contents locally before publication."
