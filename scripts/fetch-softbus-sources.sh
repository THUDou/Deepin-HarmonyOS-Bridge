#!/usr/bin/env bash
# Fetch the two soft-bus lineages for diffing/rebase. Does not build or install.
#
#   third_party/klink           KLink 4.x Linux host (Apache-2.0)
#   third_party/dsoftbus-6.0    OpenHarmony 6.0-Release dsoftbus (Apache-2.0)
#   third_party/device-auth-6.0 OpenHarmony 6.0-Release device auth (HiChain)
#
# The goal is to extract KLink's Linux host adaptation patches and replay them
# onto the 6.0 tree; see docs/10-auth-first-mvp.md and docs/11-softbus-6x-rebase-report.md.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
TP="$ROOT/third_party"
mkdir -p "$TP"

clone_or_update() {
  local url="$1"
  local dir="$2"
  local branch="${3:-}"
  if [[ -d "$dir/.git" ]]; then
    echo "== update $dir"
    git -C "$dir" fetch --depth 1 origin ${branch:+"$branch"}
    git -C "$dir" reset --hard FETCH_HEAD
  else
    echo "== clone $url -> $dir"
    if [[ -n "$branch" ]]; then
      git clone --depth 1 --branch "$branch" "$url" "$dir"
    else
      git clone --depth 1 "$url" "$dir"
    fi
  fi
}

clone_or_update "https://gitee.com/openkylin/klink.git" "$TP/klink"
clone_or_update "https://gitee.com/openharmony/communication_dsoftbus.git" \
  "$TP/dsoftbus-6.0" "OpenHarmony-6.0-Release"
clone_or_update "https://gitee.com/openharmony/security_device_auth.git" \
  "$TP/device-auth-6.0" "OpenHarmony-6.0-Release"

echo
echo "Fetched:"
git -C "$TP/klink" log -1 --format='  klink       %h %cs %s'
git -C "$TP/dsoftbus-6.0" log -1 --format='  dsoftbus6   %h %cs %s'
git -C "$TP/device-auth-6.0" log -1 --format='  devauth6    %h %cs %s'
echo
echo "Next:"
echo "  python scripts/analyze-softbus-diff.py $TP/klink/dsoftbus $TP/dsoftbus-6.0"
echo "  git diff --no-index --shortstat $TP/klink/base/security/device_auth $TP/device-auth-6.0"
echo "  see docs/11-softbus-6x-rebase-report.md"
