#!/usr/bin/env bash
# Produce an encrypted snapshot of tracked files from another local Git checkout.
# Run only on a trusted computer. Never commit the private age identity.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "Usage: AGE_RECIPIENT=age1... bash tools/import-local.sh /path/to/private/checkout" >&2
  exit 2
fi
: "${AGE_RECIPIENT:?Set AGE_RECIPIENT to the public age recipient}"
command -v age >/dev/null || { echo "Install age first" >&2; exit 2; }
command -v git >/dev/null || { echo "Install Git first" >&2; exit 2; }
[[ "$AGE_RECIPIENT" == age1* ]] || { echo "Invalid public age recipient" >&2; exit 2; }
root=$(git rev-parse --show-toplevel)
cd "$root"
source_root=$(git -C "$1" rev-parse --show-toplevel)
source_root=$(cd "$source_root" && pwd -P)
[[ "$root" != "$source_root" ]] || { echo "Source and destination must differ" >&2; exit 2; }
if [[ -n "$(git -C "$source_root" status --porcelain)" ]]; then
  echo "NOTICE: source checkout has local modifications; importing committed HEAD only." >&2
  echo "Uncommitted edits and untracked files will NOT be included." >&2
fi
[[ -z "$(git status --porcelain)" ]] || {
  echo "Destination checkout has uncommitted changes; commit or clean it first" >&2
  exit 2
}
mkdir -p payload
output=payload/release.age
[[ ! -e "$output" ]] || {
  echo "Encrypted release already exists; remove it locally after confirming backups" >&2
  exit 2
}
umask 077
part=$(mktemp "payload/.release.age.tmp.XXXXXXXX")
trap 'rm -f -- "$part"' EXIT
git -C "$source_root" archive --format=tar HEAD | age -r "$AGE_RECIPIENT" -o "$part"
[[ -s "$part" ]] || { echo "Encryption produced no output" >&2; exit 1; }
mv -- "$part" "$output"
trap - EXIT
git add -- "$output"
python3 tools/verify_public_tree.py
echo "Staged encrypted snapshot only. Inspect 'git diff --cached --stat'."
echo "After review: git commit -m 'Update encrypted payload' && git push"
