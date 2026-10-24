#!/usr/bin/env bash
# Publish a new encrypted snapshot from an ignored local editing workspace.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
cd "$root"
: "${AGE_IDENTITY_FILE:?Set AGE_IDENTITY_FILE to the path of your LOCAL secret identity}"
command -v age >/dev/null
command -v age-keygen >/dev/null
command -v tar >/dev/null
[[ -f "$AGE_IDENTITY_FILE" ]] || { echo "Local identity not found" >&2; exit 2; }
[[ -d workspace && ! -L workspace ]] || { echo "Run tools/open-workspace.sh first" >&2; exit 2; }
[[ -z "$(git status --porcelain --untracked-files=all)" ]] || {
  echo "Tracked repository has unsaved changes; resolve them before sealing" >&2
  exit 2
}
python3 tools/check_workspace.py workspace
recipient=$(age-keygen -y "$AGE_IDENTITY_FILE")
[[ "$recipient" == age1* ]] || { echo "Invalid public recipient" >&2; exit 2; }
umask 077
part=$(mktemp "payload/.release.age.tmp.XXXXXXXX")
trap 'rm -f -- "$part"' EXIT
tar -C workspace -cf - . | age -r "$recipient" -o "$part"
# Verify the encrypted output is readable using the local secret identity.
age -d -i "$AGE_IDENTITY_FILE" "$part" | tar -tf - > /dev/null
mv -- "$part" payload/release.age
trap - EXIT
git add -- payload/release.age
python3 tools/verify_public_tree.py
echo "Only encrypted snapshot is staged. Check 'git diff --cached --name-only'."
echo "Then commit and push the encrypted snapshot from this repository."
