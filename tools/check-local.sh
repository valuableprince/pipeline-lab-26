#!/usr/bin/env bash
# Run before every push; no tooling can revoke information already published.
set -euo pipefail
repo_root=$(git rev-parse --show-toplevel)
cd "$repo_root"
python3 tools/verify_public_tree.py
git diff --check
git diff --cached --check
if git ls-files | grep -E '(^|/)(\.env|id_rsa|.*\.(pwn|inc|amx|key|pem|p12|pfx|zip|tar|gz|log))$' ; then
  echo "Potential private material is tracked" >&2
  exit 1
fi
echo "Local pre-push check passed. Also inspect git log and staged changes."
