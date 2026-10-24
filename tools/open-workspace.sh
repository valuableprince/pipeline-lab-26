#!/usr/bin/env bash
# Keep private material in a local ignored workspace, not in Git history.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
cd "$root"
: "${AGE_IDENTITY_FILE:?Set AGE_IDENTITY_FILE to the path of your LOCAL secret identity}"
command -v age >/dev/null
command -v python3 >/dev/null
[[ -f "$AGE_IDENTITY_FILE" ]] || { echo "Identity file not found" >&2; exit 2; }
[[ -f payload/release.age ]] || { echo "Encrypted snapshot is missing" >&2; exit 2; }
if [[ -e workspace ]]; then
  echo "workspace already exists; refusing to overwrite local changes" >&2
  exit 2
fi
umask 077
mkdir workspace
if ! (set -o pipefail; age -d -i "$AGE_IDENTITY_FILE" payload/release.age | python3 tools/safe_extract.py "$root/workspace"); then
  echo "Decryption/extraction failed; removing incomplete newly created workspace" >&2
  rm -rf -- "$root/workspace"
  exit 1
fi
echo "Workspace ready at $root/workspace (ignored by Git). Edit files there."
